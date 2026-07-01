/**
(C) Copyright 2025-26 Murilo Marinho (murilomarinho@ieee.org)
*/

#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>

#include <OSQP_solver.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace M3;

PYBIND11_MODULE(_core, m) {

    py::class_<OSQP_Solver> osqp_solver(m, "OSQP_Solver");

    py::class_<OSQP_Solver::Configuration> osqp_configuration(osqp_solver, "Configuration");
    osqp_configuration.def(py::init<>());
    osqp_configuration.def_readwrite("maximum_iterations", &OSQP_Solver::Configuration::maximum_iterations);
    osqp_configuration.def_readwrite("eps_absolute", &OSQP_Solver::Configuration::eps_absolute);
    osqp_configuration.def_readwrite("eps_relative", &OSQP_Solver::Configuration::eps_relative);
    osqp_configuration.def_readwrite("eps_primal_infeasibility", &OSQP_Solver::Configuration::eps_primal_infeasibility);
    osqp_configuration.def_readwrite("eps_dual_infeasibility", &OSQP_Solver::Configuration::eps_dual_infeasibility);
    osqp_configuration.def_readwrite("verbose", &OSQP_Solver::Configuration::verbose);
    osqp_configuration.def_readwrite("polishing", &OSQP_Solver::Configuration::polishing);
    osqp_configuration.def_readwrite("warm_starting", &OSQP_Solver::Configuration::warm_starting);
    osqp_configuration.def_readwrite("use_hotstart", &OSQP_Solver::Configuration::use_hotstart);

    osqp_solver.def(py::init<const OSQP_Solver::Configuration&>(),
                       py::arg("configuration") = OSQP_Solver::Configuration());
    osqp_solver.def("solve_quadratic_program",&OSQP_Solver::solve_quadratic_program,".");

    // Helps evaluating the wrapper when versions show any issues
    osqp_solver.def("test_vectorxd",&OSQP_Solver::test_vectorxd,".");
    osqp_solver.def("test_matrixxd",&OSQP_Solver::test_matrixxd,".");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
