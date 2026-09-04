#pragma once

#include <vector>
#include <Eigen/Dense>

#include <osqp.h>

namespace marinholab
{

namespace solvers
{

namespace osqp
{
// Keep the using-directive scoped to this namespace rather than global
// scope: a global `using namespace Eigen;` would also be active while
// <osqp.h> is parsed, which is unnecessary and can surprise code that
// includes this header.
using namespace Eigen;

/**
 * @brief Holds all user-configurable solver options.
 *
 * Every `OSQPSettings` field is exposed here. Defaults match OSQP's own
 * defaults for a standard double-precision, direct-solver build (see
 * `osqp_set_default_settings()`), except `verbose`, which defaults to off so
 * the solver is quiet by default.
 *
 * @note OSQP validates its settings at setup/update time and will reject a
 *       build with settings outside their allowed range (e.g. non-positive
 *       tolerances).
 */
struct Configuration
{
    // ------------------------------------------------------------------
    // Wrapper-specific option (not part of OSQP's `OSQPSettings`).
    // ------------------------------------------------------------------

    /**
     * @brief Whether subsequent solves reuse the existing OSQP solver and
     *        update its data in place (`osqp_update_data_vec` /
     *        `osqp_update_data_mat`) instead of calling `osqp_setup()` again.
     *
     * This is analogous to qpOASES' `use_hotstart`. It only applies when the
     * problem dimensions and sparsity pattern are unchanged since the last
     * call; a change in shape always triggers a fresh `osqp_setup()`.
     */
    bool use_hotstart = true;

    // ------------------------------------------------------------------
    // OSQP `OSQPSettings` fields (1:1 mapping, native snake_case names).
    // ------------------------------------------------------------------

    // Linear algebra settings

    /**
     * @brief Device identifier; currently used for CUDA devices.
     * @see `OSQPSettings::device`
     */
    OSQPInt device = 0;

    /**
     * @brief Linear system solver to use.
     * @see `OSQPSettings::linsys_solver`
     */
    ::osqp_linsys_solver_type linsys_solver = ::OSQP_DIRECT_SOLVER;

    // Control settings

    /**
     * @brief Whether the solution is allocated in the solver during
     *        `osqp_setup()`.
     * @see `OSQPSettings::allocate_solution`
     */
    OSQPInt allocate_solution = 1;

    /**
     * @brief Whether solver progress is written out.
     *
     * Defaults to off so the solver is quiet by default; note this
     * intentionally differs from OSQP's own default (`OSQP_VERBOSE = 1`).
     * @see `OSQPSettings::verbose`
     */
    OSQPInt verbose = 0;

    /**
     * @brief Level of detail for profiler annotations.
     * @see `OSQPSettings::profiler_level`
     */
    OSQPInt profiler_level = 0;

    /**
     * @brief Whether OSQP warm-starts from the previous solution between
     *        consecutive `osqp_solve()` calls.
     * @see `OSQPSettings::warm_starting`
     */
    OSQPInt warm_starting = OSQP_WARM_STARTING;

    /**
     * @brief Number of heuristic data-scaling iterations; `0` disables
     *        scaling.
     * @see `OSQPSettings::scaling`
     */
    OSQPInt scaling = OSQP_SCALING;

    /**
     * @brief Whether the ADMM solution is polished to improve accuracy.
     * @see `OSQPSettings::polishing`
     */
    OSQPInt polishing = OSQP_POLISHING;

    // ADMM parameters

    /**
     * @brief ADMM penalty parameter (scalar).
     * @see `OSQPSettings::rho`
     */
    OSQPFloat rho = OSQP_RHO;

    /**
     * @brief Whether `rho` is a scalar or a vector.
     * @see `OSQPSettings::rho_is_vec`
     */
    OSQPInt rho_is_vec = OSQP_RHO_IS_VEC;

    /**
     * @brief ADMM regularization parameter (improves conditioning).
     * @see `OSQPSettings::sigma`
     */
    OSQPFloat sigma = OSQP_SIGMA;

    /**
     * @brief ADMM relaxation parameter.
     * @see `OSQPSettings::alpha`
     */
    OSQPFloat alpha = OSQP_ALPHA;

    // CG settings

    /**
     * @brief Maximum number of CG iterations per solve.
     * @see `OSQPSettings::cg_max_iter`
     */
    OSQPInt cg_max_iter = OSQP_CG_MAX_ITER;

    /**
     * @brief Number of consecutive zero CG iterations before the tolerance
     *        is halved.
     * @see `OSQPSettings::cg_tol_reduction`
     */
    OSQPInt cg_tol_reduction = OSQP_CG_TOL_REDUCTION;

    /**
     * @brief CG tolerance, as a fraction of the ADMM residuals.
     * @see `OSQPSettings::cg_tol_fraction`
     */
    OSQPFloat cg_tol_fraction = OSQP_CG_TOL_FRACTION;

    /**
     * @brief Preconditioner used by the CG method.
     * @see `OSQPSettings::cg_precond`
     */
    ::osqp_precond_type cg_precond = ::OSQP_DIAGONAL_PRECONDITIONER;

    // Adaptive rho logic

    /**
     * @brief ADMM `rho` stepsize adaptation method.
     * @see `OSQPSettings::adaptive_rho`
     */
    OSQPInt adaptive_rho = OSQP_ADAPTIVE_RHO_UPDATE_DEFAULT;

    /**
     * @brief Interval between `rho` adaptations (used when
     *        `adaptive_rho == OSQP_ADAPTIVE_RHO_UPDATE_ITERATIONS`).
     * @see `OSQPSettings::adaptive_rho_interval`
     */
    OSQPInt adaptive_rho_interval = OSQP_ADAPTIVE_RHO_INTERVAL;

    /**
     * @brief Adaptation parameter controlling when non-fixed `rho`
     *        adaptations occur (fraction of setup time, or of the previous
     *        KKT error, depending on `adaptive_rho`).
     * @see `OSQPSettings::adaptive_rho_fraction`
     */
    OSQPFloat adaptive_rho_fraction = OSQP_ADAPTIVE_RHO_FRACTION;

    /**
     * @brief Tolerance applied when adapting `rho`: the new `rho` must be
     *        this many times larger or smaller than the current one.
     * @see `OSQPSettings::adaptive_rho_tolerance`
     */
    OSQPFloat adaptive_rho_tolerance = OSQP_ADAPTIVE_RHO_TOLERANCE;

    // Termination parameters

    /**
     * @brief Maximum number of ADMM iterations.
     * @see `OSQPSettings::max_iter`
     */
    OSQPInt max_iter = OSQP_MAX_ITER;

    /**
     * @brief Absolute solution tolerance.
     * @see `OSQPSettings::eps_abs`
     */
    OSQPFloat eps_abs = OSQP_EPS_ABS;

    /**
     * @brief Relative solution tolerance.
     * @see `OSQPSettings::eps_rel`
     */
    OSQPFloat eps_rel = OSQP_EPS_REL;

    /**
     * @brief Primal infeasibility detection tolerance.
     * @see `OSQPSettings::eps_prim_inf`
     */
    OSQPFloat eps_prim_inf = OSQP_EPS_PRIM_INF;

    /**
     * @brief Dual infeasibility detection tolerance.
     * @see `OSQPSettings::eps_dual_inf`
     */
    OSQPFloat eps_dual_inf = OSQP_EPS_DUAL_INF;

    /**
     * @brief Whether the scaled termination criteria are used.
     * @see `OSQPSettings::scaled_termination`
     */
    OSQPInt scaled_termination = OSQP_SCALED_TERMINATION;

    /**
     * @brief Interval at which termination is checked; `0` disables the
     *        periodic check.
     * @see `OSQPSettings::check_termination`
     */
    OSQPInt check_termination = OSQP_CHECK_TERMINATION;

    /**
     * @brief Whether the duality-gap termination criteria are used.
     * @see `OSQPSettings::check_dualgap`
     */
    OSQPInt check_dualgap = OSQP_CHECK_DUALGAP;

    /**
     * @brief Maximum time to solve the problem, in seconds.
     * @see `OSQPSettings::time_limit`
     */
    OSQPFloat time_limit = OSQP_TIME_LIMIT;

    // Polishing parameters

    /**
     * @brief Regularization parameter used by polishing.
     * @see `OSQPSettings::delta`
     */
    OSQPFloat delta = OSQP_DELTA;

    /**
     * @brief Number of iterative refinement steps performed during
     *        polishing.
     * @see `OSQPSettings::polish_refine_iter`
     */
    OSQPInt polish_refine_iter = OSQP_POLISH_REFINE_ITER;

    /**
     * @brief Default constructor.
     *
     * Initialises every option to the defaults documented above.
     *
     * @note Declared here and defined out of line so that the in-class
     *       member initializers are used as expected. See
     *       https://stackoverflow.com/questions/53408962
     */
    Configuration();
};

/**
 * @brief High-level, reusable solver for quadratic programs (QPs) based on
 *        OSQP.
 *
 * `Solver` exposes OSQP's first-order ADMM solver through a
 * MATLAB/`quadprog`-like, matrix-based interface. Internally it keeps an
 * OSQP `OSQPSolver` object so that, once initialised, subsequent calls are
 * warm-started (see `use_hotstart` and `warm_starting`).
 *
 * The solver is configured through a `Configuration` structure whose members
 * map directly onto OSQP's `OSQPSettings` struct. In addition to the standard
 * OSQP settings, the configuration carries the wrapper-specific `use_hotstart`
 * setting.
 *
 * @note The class is not thread-safe: a single instance owns one underlying
 *       OSQP solver and its state changes across calls.
 */
class Solver
{
    protected:
        /** @brief True until the first solve has set up the solver. */
        bool osqp_solve_first_time_;
        /** @brief The underlying OSQP solver. */
        ::OSQPSolver* osqp_solver_;
        /** @brief Active configuration, copied at construction. */
        Configuration configuration_;

        /**
         * @brief Dimensions used in the last successful `osqp_setup()` call.
         * Used to detect whether a new call is compatible with hotstarting or
         * requires a fresh `osqp_setup()`.
         */
        OSQPInt problem_size_;
        OSQPInt constraint_size_;

        /**
         * @brief Holds the CSC (compressed-sparse-column) arrays of a
         *        converted Eigen matrix.
         *
         * The vectors own the storage so that it outlives the temporary
         * `OSQPCscMatrix` wrapper used when calling `osqp_setup()` /
         * `osqp_update_data_mat()`.
         */
        struct CSCMatrixData
        {
            std::vector<OSQPFloat> x;
            std::vector<OSQPInt> i;
            std::vector<OSQPInt> p;
            OSQPInt rows{0};
            OSQPInt cols{0};
        };

        /**
         * @brief Copies an Eigen vector into a std::vector<double>.
         * @param vectorxd Source vector.
         * @return A copy of the vector's data.
         *
         * Copied from SmartArmStack's sas_conversions
         * (https://github.com/SmartArmStack/sas_conversions/blob/master/src/eigen3_std_conversions.cpp).
         */
        std::vector<double> _vectorxd_to_std_vector_double(const VectorXd& vectorxd);

        /**
         * @brief Maps a std::vector<double> into an Eigen vector.
         * @param std_vector_double Source vector.
         * @return An Eigen vector wrapping the source data.
         */
        VectorXd _std_vector_double_to_vectorxd(std::vector<double> std_vector_double) const;

        /**
         * @brief Converts a dense n x n matrix into the upper-triangular CSC
         *        representation required by OSQP for P.
         * @param M Source matrix.
         * @return The CSC arrays of the (upper triangle of the) matrix.
         */
        static CSCMatrixData _dense_to_csc_upper_triangular(const MatrixXd& M);

        /**
         * @brief Converts a dense m x n matrix into a structurally-dense CSC
         *        representation (i.e. every entry, including zeros, is stored
         *        explicitly).
         *
         * Keeping the sparsity pattern stable across calls is what allows
         * `osqp_update_data_mat()` to be used when hotstarting.
         * @param M Source matrix.
         * @return The CSC arrays of the matrix.
         */
        static CSCMatrixData _dense_to_csc(const MatrixXd& M);

        /**
         * @brief Translates the user configuration into an `OSQPSettings`
         *        object.
         * @return An `OSQPSettings` with all fields filled from
         *         `configuration_`.
         */
        OSQPSettings _to_osqp_settings() const;

        /**
         * @brief Releases the current `osqp_solver_` instance, if any.
         */
        void _cleanup();

    public:
        /**
         * @brief Named solution-quality values obtained from the last
         *        successful call to solve_quadratic_program().
         *
         * See `OSQPInfo` in osqp_api_types.h for further details.
         */
        struct Info
        {
            //Primal objective value. See OSQPInfo::obj_val.
            OSQPFloat obj_val = 0.0;
            //Dual objective value. See OSQPInfo::dual_obj_val.
            OSQPFloat dual_obj_val = 0.0;
            //Norm of the primal residual. See OSQPInfo::prim_res.
            OSQPFloat prim_res = 0.0;
            //Norm of the dual residual. See OSQPInfo::dual_res.
            OSQPFloat dual_res = 0.0;
            //Dual solution, i.e. the Lagrange multiplier associated with l <= Ax <= u.
            //See OSQPSolution::y.
            VectorXd dual_solution;
            Info(); //https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
        };

        /**
         * @brief Constructs a solver.
         * @param configuration Options to use; defaults to the default
         *                       configuration.
         */
        Solver(const Configuration& configuration = Configuration());

        /** @brief Destructor; releases the underlying OSQP solver. */
        ~Solver();

        //Not copyable, as this class owns a raw ::OSQPSolver* that is not reference counted.
        Solver(const Solver&) = delete;
        Solver& operator=(const Solver&) = delete;

        /**
         * @brief Solves the following quadratic program:
         *
         *   min(x)  0.5*x'Hx + f'x
         *   s.t.    Ax <= b
         *           Aeq*x = beq.
         *
         * Method signature is compatible with MATLAB's `quadprog`.
         *
         * @param H the n x n matrix of the quadratic coefficients of the
         *        decision variables.
         * @param f the n x 1 vector of the linear coefficients of the
         *        decision variables.
         * @param A the m x n matrix of inequality constraints.
         * @param b the m x 1 value for the inequality constraints.
         * @param Aeq the k x n matrix of equality constraints.
         * @param beq the k x 1 value for the equality constraints.
         * @param x0 optional n x 1 warm-start for the primal variable x, e.g. a
         *        known feasible solution. Passed to OSQP via
         *        `osqp_warm_start()`. Pass an empty vector (the default) to
         *        skip warm-starting. Must be compatible with H.rows() when
         *        provided.
         * @param y0 optional warm-start for the dual variable y, e.g. a dual
         *        solution obtained from get_info().dual_solution in a previous
         *        call. Passed to OSQP via `osqp_warm_start()`. Pass an empty
         *        vector (the default) to skip dual warm-starting. Must have
         *        b.size()+beq.size() entries when provided.
         * @return the optimal x.
         * @throws std::runtime_error if any matrix is size-incompatible or
         *         OSQP fails to solve the problem.
         */
        VectorXd solve_quadratic_program(const MatrixXd& H, const VectorXd& f, const MatrixXd& A, const VectorXd& b, const MatrixXd& Aeq, const VectorXd& beq, const VectorXd& x0 = VectorXd(), const VectorXd& y0 = VectorXd());

        /**
         * @brief Returns named solution-quality values (obj_val, dual_obj_val,
         *        prim_res, dual_res) and the dual solution (dual_solution) from
         *        the last successful call to solve_quadratic_program().
         * @throws std::runtime_error if solve_quadratic_program() has not been
         *         called successfully yet.
         * @return an Info instance with the values populated.
         */
        Info get_info() const;

        /**
         * @brief Round-trips a vector to help evaluate the Eigen <-> std
         *        conversions used across the wrapper.
         * @param v The vector to test.
         * @return The same vector.
         */
        VectorXd test_vectorxd(const VectorXd& v);

        /**
         * @brief Round-trips a matrix to help evaluate the Eigen <-> std
         *        conversions used across the wrapper.
         * @param m The matrix to test.
         * @return The same matrix.
         */
        MatrixXd test_matrixxd(const MatrixXd& m);
};

} // namespace osqp

} // namespace solvers

} // namespace marinholab
