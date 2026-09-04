/**
 * @brief Example usage of the C++ API of `marinholab-solvers-osqp`.
 *
 * Builds a small quadratic program and solves it with
 * `marinholab::solvers::osqp::Solver`, mirroring the Python quickstart in the
 * README.
 *
 * Build and run it with the `BUILD_EXAMPLES` option (OFF by default so the
 * normal `pip install .` build is unaffected):
 *
 *     cmake -B build -GNinja -DBUILD_EXAMPLES=ON
 *     cmake --build build
 *     ./build/example/example_osqp
 */
#include <iostream>

#include <marinholab/solvers/osqp.h>

namespace osqp = marinholab::solvers::osqp;

int main()
{
    // 1. Configure the solver. Only a couple of fields are set here; the rest
    //    keep their defaults (which mirror OSQP's own defaults for a standard
    //    double-precision, direct-solver build; see osqp_set_default_settings()).
    osqp::Configuration config;
    config.eps_abs = 1.0e-9;   // tighter absolute tolerance
    config.eps_rel = 1.0e-9;   // tighter relative tolerance

    osqp::Solver solver(config);

    // 2. The problem:
    //
    //      min_x  0.5 * x' H x + f' x
    //      s.t.   A x <= b
    //             Aeq x = beq
    //
    //    H = I, f = [-1, -1], x[0] <= 0.2, plus one trivially-satisfied equality.
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
    Eigen::VectorXd f(2);
    f << -1.0, -1.0;

    Eigen::MatrixXd A(1, 2);
    A << 1.0, 0.0;
    Eigen::VectorXd b(1);
    b << 0.2;

    Eigen::MatrixXd Aeq = Eigen::MatrixXd::Zero(1, 2);
    Eigen::VectorXd beq = Eigen::VectorXd::Zero(1);

    // 3. Solve. The Solver keeps the underlying OSQP problem, so repeated
    //    calls on the same instance are warm-started by default
    //    (`Configuration.use_hotstart = true` / `warm_starting = 1`).
    Eigen::VectorXd x = solver.solve_quadratic_program(H, f, A, b, Aeq, beq);

    // 4. Inspect the result and the solution quality. `.transpose()` makes
    //    Eigen print the (column) vector as a single horizontal line.
    std::cout << "x = " << x.transpose() << "\n";
    const osqp::Solver::Info info = solver.get_info();
    std::cout << "obj_val  = " << info.obj_val << "\n";
    std::cout << "prim_res = " << info.prim_res << "\n";
    std::cout << "dual_res = " << info.dual_res << "\n";

    return 0;
}
