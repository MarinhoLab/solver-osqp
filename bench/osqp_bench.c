/*
 * Optimization benchmark for the OSQP solver core used by
 * marinholab-solvers-osqp.
 *
 * What it measures
 * ----------------
 * The package's hot path is OSQP itself: the ADMM iteration loop plus the
 * built-in algebra (CSC matrix products) and the qdldl/AMD LDL^t factorizer
 * that solves the KKT linear system. This benchmark compiles exactly those
 * C sources (see build.sh) with a chosen set of compiler flags, so the only
 * thing that varies between runs is the optimization level.
 *
 * Two problem workloads are run:
 *   - "small"  (n ~ 800)  : steady-state ADMM throughput
 *   - "large"  (n ~ 2500) : factorization-heavy setup timing (qdldl scaling)
 *
 * The Hessian P is dense and SPD (P = M'M + n*I), which mirrors how the
 * Python wrapper feeds OSQP: it converts a *dense* Eigen Hessian to CSC, so
 * in practice the top-left block of the KKT system is dense. Box constraints
 * plus a few equality rows are added so the KKT system has a realistic
 * structure.
 *
 * Solves are cold-started (warm_starting = 0) so the iteration count is
 * stable and the ADMM loop does substantial work on every call.
 *
 * Output
 * ------
 * One machine-readable "RESULT ..." line per metric, e.g.
 *
 *   RESULT small setup_time=0.012345s
 *   RESULT small solve_time_avg=0.004321s reps=400
 *   RESULT small iters_avg=50.0
 *   RESULT small obj_val=-0.128031 status=solved
 *
 * The objective value lets a human (or script) verify that a different
 * optimization level did not change the solution.
 */

#include <osqp.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Deterministic PRNG (splitmix64) so every variant sees identical data. */
/* ------------------------------------------------------------------ */
static uint64_t g_st;

static uint64_t sm64(void)
{
    uint64_t z = (g_st += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double runit(void) /* uniform in [0, 1) */
{
    return (double)(sm64() >> 11) * (1.0 / 9007199254740992.0);
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* ------------------------------------------------------------------ */
/* Problem construction.                                                */
/* ------------------------------------------------------------------ */

struct problem
{
    OSQPCscMatrix P, A;
    OSQPFloat *q, *l, *u;
    int n, m;
};

static struct problem make_problem(int n, int m_box, int m_eq, uint64_t seed)
{
    struct problem pr;
    int m = m_box + m_eq;
    pr.n = n;
    pr.m = m;
    g_st = seed;

    /* ---- P = M'M + n*I (dense, SPD), upper triangle in CSC ---- */
    int nnzP = n * (n + 1) / 2;
    pr.P.m = pr.P.n = n;
    pr.P.nzmax = nnzP;
    pr.P.nz = -1; /* CSC */
    pr.P.owned = 0;
    pr.P.p = (OSQPInt *)malloc(sizeof(OSQPInt) * (n + 1));
    pr.P.i = (OSQPInt *)malloc(sizeof(OSQPInt) * nnzP);
    pr.P.x = (OSQPFloat *)malloc(sizeof(OSQPFloat) * nnzP);

    double **M = (double **)malloc(sizeof(double *) * n);
    for (int r = 0; r < n; r++)
    {
        M[r] = (double *)malloc(sizeof(double) * n);
        for (int c = 0; c < n; c++)
            M[r][c] = runit() * 2.0 - 1.0; /* uniform in [-1, 1) */
    }
    int k = 0;
    for (int c = 0; c < n; c++)
    {
        pr.P.p[c] = k;
        for (int r = 0; r <= c; r++)
        {
            double acc = (r == c) ? (double)n : 0.0;
            for (int t = 0; t < n; t++)
                acc += M[t][r] * M[t][c];
            pr.P.i[k] = r;
            pr.P.x[k] = acc;
            k++;
        }
    }
    pr.P.p[n] = k;
    for (int r = 0; r < n; r++)
        free(M[r]);
    free(M);

    pr.q = (OSQPFloat *)malloc(sizeof(OSQPFloat) * n);
    for (int i = 0; i < n; i++)
        pr.q[i] = runit() * 2.0 - 1.0;

    /* ---- A: m_box box rows (A[r,r]=1, r<m_box) + m_eq dense rows ---- */
    int nnzA = m_box + m_eq * n;
    pr.A.m = pr.m;
    pr.A.n = n;
    pr.A.nzmax = nnzA;
    pr.A.nz = -1;
    pr.A.owned = 0;
    pr.A.p = (OSQPInt *)malloc(sizeof(OSQPInt) * (n + 1));
    pr.A.i = (OSQPInt *)malloc(sizeof(OSQPInt) * nnzA);
    pr.A.x = (OSQPFloat *)malloc(sizeof(OSQPFloat) * nnzA);
    int *colcnt = (int *)calloc(n + 1, sizeof(int));
    for (int c = 0; c < n; c++)
    {
        if (c < m_box)
            colcnt[c] += 1;
        colcnt[c] += m_eq;
    }
    int off = 0;
    for (int c = 0; c < n; c++)
    {
        pr.A.p[c] = off;
        off += colcnt[c];
    }
    pr.A.p[n] = off;
    int *pos = (int *)malloc(sizeof(int) * (n + 1));
    for (int c = 0; c < n; c++)
        pos[c] = pr.A.p[c];
    for (int c = 0; c < n; c++)
    {
        if (c < m_box)
        {
            pr.A.i[pos[c]] = c;
            pr.A.x[pos[c]] = 1.0;
            pos[c]++;
        }
        for (int e = 0; e < m_eq; e++)
        {
            pr.A.i[pos[c]] = m_box + e;
            pr.A.x[pos[c]] = 0.2 * (runit() * 2.0 - 1.0);
            pos[c]++;
        }
    }
    free(pos);
    free(colcnt);

    pr.l = (OSQPFloat *)malloc(sizeof(OSQPFloat) * pr.m);
    pr.u = (OSQPFloat *)malloc(sizeof(OSQPFloat) * pr.m);
    for (int r = 0; r < m_box; r++)
    {
        pr.l[r] = -5.0;
        pr.u[r] = 5.0;
    }
    for (int e = 0; e < m_eq; e++)
    {
        pr.l[m_box + e] = -1.0;
        pr.u[m_box + e] = 1.0;
    }
    return pr;
}

static void free_problem(struct problem *pr)
{
    free(pr->q);
    free(pr->l);
    free(pr->u);
    free(pr->P.p);
    free(pr->P.i);
    free(pr->P.x);
    free(pr->A.p);
    free(pr->A.i);
    free(pr->A.x);
}

/* ------------------------------------------------------------------ */
/* Run one workload and report.                                         */
/* ------------------------------------------------------------------ */
static int run_workload(const char *name, struct problem *pr, int warmup, int reps)
{
    OSQPSettings *s = OSQPSettings_new();
    osqp_set_default_settings(s);
    s->verbose = 0;
    s->warm_starting = 0; /* stable iteration count across cold solves */
    s->polishing = 0;
    s->eps_abs = 1e-6;
    s->eps_rel = 1e-6;
    s->max_iter = 5000;

    OSQPSolver *sol = NULL;
    double t0 = now_s();
    OSQPInt rc = osqp_setup(&sol, &pr->P, pr->q, &pr->A, pr->l, pr->u, pr->m, pr->n, s);
    double setup_time = now_s() - t0;
    if (rc != 0)
    {
        fprintf(stderr, "%s: osqp_setup failed (%s)\n", name, osqp_error_message(rc));
        OSQPSettings_free(s);
        return 1;
    }

    for (int i = 0; i < warmup; i++)
        osqp_solve(sol);

    long long iters_sum = 0;
    double total = 0.0;
    OSQPFloat last_obj = 0.0;
    for (int i = 0; i < reps; i++)
    {
        double a = now_s();
        osqp_solve(sol);
        total += now_s() - a;
        iters_sum += (long long)sol->info->iter;
        last_obj = sol->info->obj_val;
    }

    printf("RESULT %s n=%d m=%d\n", name, pr->n, pr->m);
    printf("RESULT %s status=%s obj_val=%.6f prim_res=%.3e dual_res=%.3e\n",
           name, sol->info->status, last_obj, sol->info->prim_res, sol->info->dual_res);
    printf("RESULT %s setup_time=%.6f s\n", name, setup_time);
    printf("RESULT %s solve_time_avg=%.6f s reps=%d warmup=%d\n",
           name, total / reps, reps, warmup);
    printf("RESULT %s iters_avg=%.2f\n", name, (double)iters_sum / reps);

    osqp_cleanup(sol);
    OSQPSettings_free(s);
    return 0;
}

int main(int argc, char **argv)
{
    int small_n = 800, small_eq = 8;
    int large_n = 2500, large_eq = 16;
    int reps = 300, large_reps = 40, warmup = 20, large_warmup = 5;
    uint64_t seed = 42;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-small-n") == 0) small_n = atoi(argv[++i]);
        else if (strcmp(argv[i], "-small-eq") == 0) small_eq = atoi(argv[++i]);
        else if (strcmp(argv[i], "-large-n") == 0) large_n = atoi(argv[++i]);
        else if (strcmp(argv[i], "-large-eq") == 0) large_eq = atoi(argv[++i]);
        else if (strcmp(argv[i], "-reps") == 0) reps = atoi(argv[++i]);
        else if (strcmp(argv[i], "-large-reps") == 0) large_reps = atoi(argv[++i]);
        else if (strcmp(argv[i], "-seed") == 0) seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-skip-large") == 0) large_n = 0;
    }

    int ok = 0;
    struct problem small = make_problem(small_n, small_n, small_eq, seed);
    ok |= run_workload("small", &small, warmup, reps);
    free_problem(&small);

    if (large_n > 0)
    {
        struct problem large = make_problem(large_n, large_n, large_eq, seed + 1);
        ok |= run_workload("large", &large, large_warmup, large_reps);
        free_problem(&large);
    }
    return ok;
}
