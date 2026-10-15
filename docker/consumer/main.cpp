/**
 * Downstream CMake project that links the *installed*
 * `marinholab::solvers::osqp` package, i.e. what a user gets from
 * `apt-get install libmarinholab-solver-osqp` (the repository) or from
 * `cmake --install`.
 *
 * It uses nothing but `find_package()` and the imported target -- no paths into
 * the source tree -- so it is the regression test for the packaging and for
 * `marinholab_solver_osqp-config.cmake`. Run it with `docker/test-ppa.sh`
 * (package from the repository) or `docker/build-deb.sh --test` (locally built
 * package).
 */
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <marinholab/solvers/osqp.h>

namespace osqp = marinholab::solvers::osqp;

namespace
{

int failures = 0;

void check(const std::string& what, const Eigen::VectorXd& got, const std::vector<double>& expected, const double tolerance = 1e-6)
{
    bool ok = (got.size() == static_cast<Eigen::Index>(expected.size()));
    for (Eigen::Index i = 0; ok && i < got.size(); ++i)
    {
        ok = std::abs(got[i] - expected[i]) < tolerance;
    }

    const Eigen::Map<const Eigen::VectorXd> want(expected.data(), static_cast<Eigen::Index>(expected.size()));
    std::cout << (ok ? "ok   " : "FAIL ") << what << ": got [" << got.transpose()
              << "], expected [" << want.transpose() << "]\n";
    if (!ok)
    {
        ++failures;
    }
}

void check_true(const std::string& what, const bool ok)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok)
    {
        ++failures;
    }
}

/**
 * OSQP is a first-order method, so a plain solve only converges to within
 * `eps_abs`/`eps_rel`. With `polishing` on, the active set is refined by a
 * direct solve, which is what makes the tight tolerances below meaningful.
 */
osqp::Configuration accurate()
{
    osqp::Configuration config;
    config.set("polishing", true);
    config.set("eps_abs", 1.0e-9);
    config.set("eps_rel", 1.0e-9);
    config.set("max_iter", 20000);
    return config;
}

/** min 0.5*x'x - [1,1]'x  s.t. x0 <= 0.2 (plus an inactive equality) -> [0.2, 1]. */
void inequality_constraint()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;
    Eigen::MatrixXd A(1, 2);
    A << 1.0, 0.0;
    Eigen::VectorXd b(1);
    b << 0.2;
    Eigen::MatrixXd Aeq = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd beq = Eigen::VectorXd::Zero(1);

    check("inequality-constrained x", solver.solve_quadratic_program(H, f, A, b, Aeq, beq), {0.2, 1.0});
}

/**
 * min 0.5*x'x - [1,1]'x with no real constraints. The C++ API takes matrices
 * rather than `None`, so a single trivially-satisfied zero row stands for "no
 * such constraint" -- the same substitution the Python wrapper makes.
 */
void unconstrained()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;
    Eigen::MatrixXd none = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd zero = Eigen::VectorXd::Zero(1);

    check("unconstrained x", solver.solve_quadratic_program(H, f, none, zero, none, zero), {1.0, 1.0});
}

/** min 0.5*x'x - [1,1]'x  s.t. x0 + x1 = 1  ->  [0.5, 0.5]. */
void equality_constraint()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;
    Eigen::MatrixXd Aeq(1, 2);
    Aeq << 1.0, 1.0;
    Eigen::VectorXd beq(1);
    beq << 1.0;
    Eigen::MatrixXd none = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd zero = Eigen::VectorXd::Zero(1);

    check("equality-constrained x", solver.solve_quadratic_program(H, f, none, zero, Aeq, beq), {0.5, 0.5});
}

/**
 * Rank-deficient Hessian: min 0.5*x0^2 - 2*x0 + 0.5*x1 s.t. -1 <= x <= 1.
 * x0 is capped at its upper bound, x1 is driven to its lower bound -> [1, -1].
 */
void semidefinite()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H(2, 2);
    H << 1.0, 0.0, 0.0, 0.0;
    Eigen::VectorXd f(2);
    f << -2.0, 0.5;
    Eigen::MatrixXd A(4, 2);
    A << -Eigen::MatrixXd::Identity(2, 2), Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd b(4);
    b << 1.0, 1.0, 1.0, 1.0;
    Eigen::MatrixXd none = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd zero = Eigen::VectorXd::Zero(1);

    check("semi-definite x", solver.solve_quadratic_program(H, f, A, b, none, zero), {1.0, -1.0});
}

/** Repeated solves on one instance must stay correct (the hotstart path). */
void warm_start()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;
    Eigen::MatrixXd none = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd zero = Eigen::VectorXd::Zero(1);

    for (int i = 0; i < 5; ++i)
    {
        Eigen::MatrixXd A(1, 2);
        A << 1.0, 0.0;
        Eigen::VectorXd b(1);
        b << 0.1 * i;
        check("warm-started solve " + std::to_string(i), solver.solve_quadratic_program(H, f, A, b, none, zero), {0.1 * i, 1.0});
    }
}

/** Solution-quality reporting is reachable through the installed package. */
void solution_info()
{
    osqp::Solver solver(accurate());

    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;
    Eigen::MatrixXd none = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd zero = Eigen::VectorXd::Zero(1);

    solver.solve_quadratic_program(H, f, none, zero, none, zero);
    const auto info = solver.get_info();
    // min 0.5*x'x - [1,1]'x at x = [1,1] has objective -1, and both residuals
    // must be small at a converged, polished solution.
    check_true("get_info: obj_val ~ -1 and residuals small",
               std::abs(info.obj_val + 1.0) < 1e-6
                   && std::abs(info.prim_res) < 1e-6 && std::abs(info.dual_res) < 1e-6);
}

/** The option accessors work through the installed package. */
void configuration()
{
    osqp::Configuration config;
    config.set("eps_abs", 1.0e-9);

    check_true("Configuration accessors",
               config.has("eps_abs")
                   && std::get<double>(config.get("eps_abs")) == 1.0e-9
                   && !config.keys().empty() && !config.defaults().empty());
}

/**
 * The real-valued defaults must reach the package as doubles. Carrying a
 * `Configuration` through text once turned every tolerance into 0
 * (`std::to_string` truncates), and OSQP then stopped on the first iteration.
 */
void tolerance_defaults()
{
    const auto defaults = osqp::Configuration().defaults();

    for (const char* key : {"eps_abs", "eps_rel", "eps_prim_inf", "eps_dual_inf", "rho", "sigma"})
    {
        const auto it = defaults.find(key);
        const double value = it == defaults.end() ? 0.0 : std::get<double>(it->second);
        check_true(std::string(key) + " default is a non-zero double", value != 0.0);
    }
}

}  // namespace

int main()
{
    inequality_constraint();
    unconstrained();
    equality_constraint();
    semidefinite();
    warm_start();
    solution_info();
    configuration();
    tolerance_defaults();

    if (failures != 0)
    {
        std::cerr << failures << " check(s) FAILED\n";
        return 1;
    }
    std::cout << "All checks passed.\n";
    return 0;
}
