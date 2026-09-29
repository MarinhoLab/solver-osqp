#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <Eigen/Dense>

namespace marinholab
{

namespace solvers
{

namespace osqp
{

/**
 * @brief The value of an option: a boolean, an integer, a real, or the
 *        name of an enumeration value (e.g. "OSQP_DIRECT_SOLVER").
 *
 * Enumeration values are held by name so that this header stays free of
 * OSQP types.
 */
using OptionValue = std::variant<bool, long long, double, std::string>;

/**
 * @brief Holds all user-configurable solver options, keyed by name.
 *
 * Every OSQP `OSQPSettings` field plus the wrapper-specific settings is
 * exposed under its name. Each option has a kind, and values are stored as
 * that kind:
 *
 *  - booleans:      `bool`
 *  - integers:      `long long`
 *  - reals:         `double`
 *  - enumerations:  `std::string` holding the enum value name, e.g.
 *                   "OSQP_DIRECT_SOLVER", "OSQP_DIAGONAL_PRECONDITIONER"
 *
 * `set()` also accepts strings for the other kinds ("true"/"false", an
 * integer or floating-point literal) and an integer for a real, and
 * converts them.
 *
 * Unset options use OSQP's own defaults for a double-precision,
 * direct-solver build (see `osqp_set_default_settings()`), taken directly
 * from OSQP, except `verbose`, which defaults to the least verbose value
 * (`false`) so the solver is quiet by default (OSQP's own default is `true`).
 *
 * @see Solver
 */
class Configuration
{
public:
    /**
     * @brief Default constructor.
     *
     * Starts with an empty option map: every option holds its default.
     */
    Configuration();

    /** @brief Copy constructor. */
    Configuration(const Configuration& other);

    /** @brief Copy assignment. */
    Configuration& operator=(const Configuration& other);

    /** @brief Destructor. */
    ~Configuration();

    /**
     * @brief Sets the option `key` to `value`.
     *
     * The value is validated and converted to the option's kind
     * immediately, so a misspelled key or a wrong type raises
     * `std::invalid_argument` here rather than at solve time.
     *
     * @throws std::invalid_argument if the key is unknown or the value
     *         cannot be converted to that option's kind.
     */
    void set(const std::string& key, const OptionValue& value);

    /**
     * @brief Sets the option `key` from a string literal (so that it is
     *        not converted to `bool`).
     *
     * @throws std::invalid_argument as `set(const std::string&, const OptionValue&)`.
     */
    void set(const std::string& key, const char* value);

    /**
     * @brief Returns the value of the option `key`.
     *
     * @return The value, as the option's kind, or the option's default when
     *         it has not been set.
     * @throws std::invalid_argument if the key is unknown.
     */
    OptionValue get(const std::string& key) const;

    /**
     * @brief Whether the option `key` has been explicitly set.
     *
     * @return `true` when `set()` was called for `key`.
     * @throws std::invalid_argument if the key is unknown.
     */
    bool has(const std::string& key) const;

    /**
     * @brief The names of all settable options, sorted.
     *
     * @return A copy of the sorted option names.
     */
    std::vector<std::string> keys() const;

    /**
     * @brief Removes `key` so the option reverts to its default.
     *
     * @throws std::invalid_argument if the key is unknown.
     */
    void reset(const std::string& key);

    /**
     * @brief Removes all options, reverting every one to its default.
     */
    void reset_all();

    /**
     * @brief The option names and their default values, sorted.
     *
     * @return A copy of the `{name: default_value}` map.
     */
    std::map<std::string, OptionValue> defaults() const;

private:
    friend class Solver;

    /** @brief Explicitly set option values, keyed by option name. */
    std::map<std::string, OptionValue> options_;
};

/**
 * @brief High-level, reusable solver for quadratic programs (QPs) based on
 *        OSQP.
 *
 * `Solver` exposes OSQP's first-order ADMM solver through a
 * MATLAB/`quadprog`-like, matrix-based interface. Internally it keeps an
 * `OSQPSolver` so that, once set up, subsequent calls on the same instance
 * reuse it and update its data in place by default (see the `use_hotstart`
 * option); OSQP additionally warm-starts from the previous iterate between
 * `osqp_solve()` calls.
 *
 * The solver is configured through a `Configuration` keyed by option name.
 *
 * @note The class is not thread-safe: a single instance owns one
 *       underlying OSQP solver and its state changes across calls.
 * @note The class is movable but not copyable (it owns the underlying
 *       solver state).
 */
class Solver
{
public:
    /**
     * @brief Constructs a solver.
     * @param configuration Options to use; defaults to the default
     *                       configuration.
     */
    explicit Solver(const Configuration& configuration = Configuration());

    /**
     * @brief Move constructor.
     * @param other The solver to move from.
     */
    Solver(Solver&& other) noexcept;

    /**
     * @brief Move assignment.
     * @param other The solver to move from.
     */
    Solver& operator=(Solver&& other) noexcept;

    /** @brief Solvers are not copyable (they own the solver state). */
    Solver(const Solver&) = delete;

    /** @brief Solvers are not copyable (they own the solver state). */
    Solver& operator=(const Solver&) = delete;

    /** @brief Destructor; releases the underlying OSQP solver. */
    ~Solver();

    /**
     * @brief Named solution-quality values obtained from the last
     *        successful call to solve_quadratic_program().
     *
     * See `OSQPInfo` in `osqp_api_types.h` for further details.
     */
    struct Info
    {
        /** @brief Primal objective value. See `OSQPInfo::obj_val`. */
        double obj_val = 0.0;
        /** @brief Dual objective value. See `OSQPInfo::dual_obj_val`. */
        double dual_obj_val = 0.0;
        /** @brief Norm of the primal residual. See `OSQPInfo::prim_res`. */
        double prim_res = 0.0;
        /** @brief Norm of the dual residual. See `OSQPInfo::dual_res`. */
        double dual_res = 0.0;
        /**
         * @brief Dual solution, i.e. the Lagrange multiplier associated
         *        with `l <= A x <= u`. See `OSQPSolution::y`.
         */
        Eigen::VectorXd dual_solution;
    };

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
     * @return the optimal x
     * @throws std::runtime_error if any matrix is size-incompatible or
     *         OSQP fails to solve the problem.
     */
    Eigen::VectorXd solve_quadratic_program(const Eigen::MatrixXd& H, const Eigen::VectorXd& f, const Eigen::MatrixXd& A, const Eigen::VectorXd& b, const Eigen::MatrixXd& Aeq, const Eigen::VectorXd& beq, const Eigen::VectorXd& x0 = Eigen::VectorXd(), const Eigen::VectorXd& y0 = Eigen::VectorXd());

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
    Eigen::VectorXd test_vectorxd(const Eigen::VectorXd& v);

    /**
     * @brief Round-trips a matrix to help evaluate the Eigen <-> std
     *        conversions used across the wrapper.
     * @param m The matrix to test.
     * @return The same matrix.
     */
    Eigen::MatrixXd test_matrixxd(const Eigen::MatrixXd& m);

private:
    /**
     * @brief Implementation (pimpl): owns the underlying OSQP solver and
     *        the conversion helpers.
     *
     * Declared here and defined in the implementation so that OSQP types
     * never appear in this public header.
     */
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace osqp

} // namespace solvers

} // namespace marinholab
