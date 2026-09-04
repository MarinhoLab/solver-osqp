/**
(C) Copyright 2025-26 Murilo Marinho (murilomarinho@ieee.org)

pybind11 bindings for marinholab::solvers::osqp::Solver.
*/

#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>

#include <marinholab/solvers/osqp.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace marinholab::solvers::osqp;

PYBIND11_MODULE(_core, m) {

    m.doc() = "Python bindings for an OSQP-based quadratic program solver.";

    py::class_<Solver> osqp_solver(m, "OSQP_Solver",
        "High-level, reusable solver for quadratic programs (QPs) based on OSQP.\n\n"
        "Solves the following problem:\n\n"
        "    min(x)  0.5*x'Hx + f'x\n"
        "    s.t.    Ax <= b\n"
        "            Aeq*x = beq\n\n"
        "Method signature is compatible with MATLAB's `quadprog`. Once the\n"
        "problem has been solved once, subsequent solves are warm-started by\n"
        "default (see `Configuration.use_hotstart`).");

    py::enum_<::osqp_linsys_solver_type>(osqp_solver, "LinsysSolverType",
        "OSQP linear system solvers.")
    .value("OSQP_UNKNOWN_SOLVER", ::OSQP_UNKNOWN_SOLVER)
    .value("OSQP_DIRECT_SOLVER", ::OSQP_DIRECT_SOLVER)
    .value("OSQP_INDIRECT_SOLVER", ::OSQP_INDIRECT_SOLVER)
    .export_values();

    py::enum_<::osqp_precond_type>(osqp_solver, "PreconditionerType",
        "Preconditioners for the conjugate-gradient method.")
    .value("OSQP_NO_PRECONDITIONER", ::OSQP_NO_PRECONDITIONER)
    .value("OSQP_DIAGONAL_PRECONDITIONER", ::OSQP_DIAGONAL_PRECONDITIONER)
    .export_values();

    py::enum_<::osqp_status_type>(osqp_solver, "Status",
        "OSQP solver status codes returned for the last solve.")
    .value("OSQP_SOLVED", ::OSQP_SOLVED)
    .value("OSQP_SOLVED_INACCURATE", ::OSQP_SOLVED_INACCURATE)
    .value("OSQP_PRIMAL_INFEASIBLE", ::OSQP_PRIMAL_INFEASIBLE)
    .value("OSQP_PRIMAL_INFEASIBLE_INACCURATE", ::OSQP_PRIMAL_INFEASIBLE_INACCURATE)
    .value("OSQP_DUAL_INFEASIBLE", ::OSQP_DUAL_INFEASIBLE)
    .value("OSQP_DUAL_INFEASIBLE_INACCURATE", ::OSQP_DUAL_INFEASIBLE_INACCURATE)
    .value("OSQP_MAX_ITER_REACHED", ::OSQP_MAX_ITER_REACHED)
    .value("OSQP_TIME_LIMIT_REACHED", ::OSQP_TIME_LIMIT_REACHED)
    .value("OSQP_NON_CVX", ::OSQP_NON_CVX)
    .value("OSQP_SIGINT", ::OSQP_SIGINT)
    .value("OSQP_UNSOLVED", ::OSQP_UNSOLVED)
    .export_values();

    py::class_<Configuration> osqp_configuration(osqp_solver, "Configuration",
        "All user-configurable solver options.\n\n"
        "Members are a 1:1 mapping of OSQP's `OSQPSettings` fields (plus the\n"
        "wrapper-specific `use_hotstart`). See the OSQP documentation for a\n"
        "full description of each option: https://osqp.org/docs/");

    osqp_configuration.def(py::init<>());

    // Wrapper-specific option
    osqp_configuration.def_readwrite("use_hotstart", &Configuration::use_hotstart,
        "Reuse the existing OSQP solver and update its data in place instead of re-running osqp_setup() when the problem shape is unchanged.");

    // Linear algebra settings
    osqp_configuration.def_readwrite("device", &Configuration::device, "Device identifier; currently used for CUDA devices.");
    osqp_configuration.def_readwrite("linsys_solver", &Configuration::linsys_solver, "Linear system solver to use.");
    // Control settings
    osqp_configuration.def_readwrite("allocate_solution", &Configuration::allocate_solution, "Whether the solution is allocated during osqp_setup().");
    osqp_configuration.def_readwrite("verbose", &Configuration::verbose, "Whether solver progress is written out (0 = quiet, the default).");
    osqp_configuration.def_readwrite("profiler_level", &Configuration::profiler_level, "Level of detail for profiler annotations.");
    osqp_configuration.def_readwrite("warm_starting", &Configuration::warm_starting, "Whether OSQP warm-starts from the previous solution between consecutive osqp_solve() calls.");
    osqp_configuration.def_readwrite("scaling", &Configuration::scaling, "Number of heuristic data-scaling iterations; 0 disables scaling.");
    osqp_configuration.def_readwrite("polishing", &Configuration::polishing, "Whether the ADMM solution is polished to improve accuracy.");
    // ADMM parameters
    osqp_configuration.def_readwrite("rho", &Configuration::rho, "ADMM penalty parameter (scalar).");
    osqp_configuration.def_readwrite("rho_is_vec", &Configuration::rho_is_vec, "Whether rho is a scalar or a vector.");
    osqp_configuration.def_readwrite("sigma", &Configuration::sigma, "ADMM regularization parameter (improves conditioning).");
    osqp_configuration.def_readwrite("alpha", &Configuration::alpha, "ADMM relaxation parameter.");
    // CG settings
    osqp_configuration.def_readwrite("cg_max_iter", &Configuration::cg_max_iter, "Maximum number of CG iterations per solve.");
    osqp_configuration.def_readwrite("cg_tol_reduction", &Configuration::cg_tol_reduction, "Number of consecutive zero CG iterations before the tolerance is halved.");
    osqp_configuration.def_readwrite("cg_tol_fraction", &Configuration::cg_tol_fraction, "CG tolerance, as a fraction of the ADMM residuals.");
    osqp_configuration.def_readwrite("cg_precond", &Configuration::cg_precond, "Preconditioner used by the CG method.");
    // Adaptive rho logic
    osqp_configuration.def_readwrite("adaptive_rho", &Configuration::adaptive_rho, "ADMM rho stepsize adaptation method.");
    osqp_configuration.def_readwrite("adaptive_rho_interval", &Configuration::adaptive_rho_interval, "Interval between rho adaptations (used with the iterations-based method).");
    osqp_configuration.def_readwrite("adaptive_rho_fraction", &Configuration::adaptive_rho_fraction, "Adaptation parameter controlling when non-fixed rho adaptations occur.");
    osqp_configuration.def_readwrite("adaptive_rho_tolerance", &Configuration::adaptive_rho_tolerance, "Tolerance applied when adapting rho (min ratio between new and current rho).");
    // Termination parameters
    osqp_configuration.def_readwrite("max_iter", &Configuration::max_iter, "Maximum number of ADMM iterations.");
    osqp_configuration.def_readwrite("eps_abs", &Configuration::eps_abs, "Absolute solution tolerance.");
    osqp_configuration.def_readwrite("eps_rel", &Configuration::eps_rel, "Relative solution tolerance.");
    osqp_configuration.def_readwrite("eps_prim_inf", &Configuration::eps_prim_inf, "Primal infeasibility detection tolerance.");
    osqp_configuration.def_readwrite("eps_dual_inf", &Configuration::eps_dual_inf, "Dual infeasibility detection tolerance.");
    osqp_configuration.def_readwrite("scaled_termination", &Configuration::scaled_termination, "Whether the scaled termination criteria are used.");
    osqp_configuration.def_readwrite("check_termination", &Configuration::check_termination, "Interval at which termination is checked; 0 disables the periodic check.");
    osqp_configuration.def_readwrite("check_dualgap", &Configuration::check_dualgap, "Whether the duality-gap termination criteria are used.");
    osqp_configuration.def_readwrite("time_limit", &Configuration::time_limit, "Maximum time to solve the problem, in seconds.");
    // Polishing parameters
    osqp_configuration.def_readwrite("delta", &Configuration::delta, "Regularization parameter used by polishing.");
    osqp_configuration.def_readwrite("polish_refine_iter", &Configuration::polish_refine_iter, "Number of iterative refinement steps performed during polishing.");

    py::class_<Solver::Info> osqp_info(osqp_solver, "Info",
        "Solution-quality values obtained from the last successful call to "
        "solve_quadratic_program().");
    osqp_info.def(py::init<>());
    osqp_info.def_readonly("obj_val", &Solver::Info::obj_val, "Primal objective value.");
    osqp_info.def_readonly("dual_obj_val", &Solver::Info::dual_obj_val, "Dual objective value.");
    osqp_info.def_readonly("prim_res", &Solver::Info::prim_res, "Norm of the primal residual.");
    osqp_info.def_readonly("dual_res", &Solver::Info::dual_res, "Norm of the dual residual.");
    osqp_info.def_readonly("dual_solution", &Solver::Info::dual_solution,
        "Dual solution, i.e. the Lagrange multiplier associated with l <= Ax <= u.");

    osqp_solver.def(py::init<const Configuration&>(),
                       py::arg("configuration") = Configuration(),
                       "Constructs a solver with the given configuration (defaults to the default configuration).");
    osqp_solver.def("solve_quadratic_program",
                    &Solver::solve_quadratic_program,
                    py::arg("H"), py::arg("f"), py::arg("A"), py::arg("b"), py::arg("Aeq"), py::arg("beq"),
                    py::arg("x0") = VectorXd(), py::arg("y0") = VectorXd(),
                    "Solves min(x) 0.5*x'Hx + f'x s.t. Ax <= b and Aeq*x = beq (MATLAB `quadprog`-like signature).\n\n"
                    "Returns the optimal x. x0 and y0 are optional warm-starts for the primal and dual variables.");
    osqp_solver.def("get_info",
                    &Solver::get_info,
                    "Returns the solution-quality values (obj_val, dual_obj_val, prim_res, dual_res) and the dual solution (dual_solution) from the last successful call to solve_quadratic_program().");

    // Helps evaluating the wrapper when versions show any issues
    osqp_solver.def("test_vectorxd", &Solver::test_vectorxd, py::arg("v"),
                    "Round-trips a vector to help evaluate the Eigen <-> std conversions used across the wrapper.");
    osqp_solver.def("test_matrixxd", &Solver::test_matrixxd, py::arg("m"),
                    "Round-trips a matrix to help evaluate the Eigen <-> std conversions used across the wrapper.");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
