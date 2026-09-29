/**
(C) Copyright 2025-26 Murilo Marinho (murilomarinho@ieee.org)

pybind11 bindings for marinholab::solvers::osqp::Solver.

The `Configuration` is exposed with accessors keyed by option name (`set`,
`get`, `has`, `keys`, `defaults`, `reset`). Values are native Python bool,
int, float, or str (enumeration values by name), so the Python surface is
free of OSQP enum types. `set()` also accepts Python enum members (e.g. a
`LinsysSolverType`), which are passed by name.
*/

#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <marinholab/solvers/osqp.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace marinholab::solvers::osqp;

namespace
{

/**
 * @brief Converts a Python value into an `OptionValue` for
 * `Configuration::set()`, which then converts it to the option's kind.
 *
 * Accepts:
 *  - `bool`             -> bool (checked before int, since bool is an int)
 *  - an enum member     -> its `.name` (e.g. `LinsysSolverType.OSQP_DIRECT_SOLVER` ->
 *                          `"OSQP_DIRECT_SOLVER"`; checked before int, since an
 *                          `IntEnum` is an int)
 *  - `int`, or anything with `__index__` (e.g. numpy integers) -> long long
 *  - `float`, or anything with `__float__` (e.g. numpy floats) -> double
 *  - `str`              -> str
 */
OptionValue to_option_value(py::handle value)
{
    if(py::isinstance<py::bool_>(value))
        return value.cast<bool>();

    // Leaked on purpose: a static py::object would be destroyed after the
    // interpreter has shut down.
    static const py::object* enum_type = new py::object(py::module_::import("enum").attr("Enum"));
    if(py::isinstance(value, *enum_type))
        return py::str(value.attr("name")).cast<std::string>();

    if(py::isinstance<py::str>(value))
        return value.cast<std::string>();

    if(py::isinstance<py::int_>(value) || py::hasattr(value, "__index__"))
        return py::int_(py::reinterpret_borrow<py::object>(value)).cast<long long>();

    if(py::isinstance<py::float_>(value) || py::hasattr(value, "__float__"))
        return py::float_(py::reinterpret_borrow<py::object>(value)).cast<double>();

    throw py::type_error(
        "Option value must be a bool, an int, a float, a string, or an enum member; "
        "got " + py::str(value.get_type()).cast<std::string>());
}

} // namespace

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
        "default (see the `use_hotstart` option on the Configuration).");

    py::class_<Configuration> configuration(osqp_solver, "Configuration",
        "Holder of all user-configurable solver options, keyed by name.\n\n"
        "Every OSQP `OSQPSettings` field plus the wrapper-specific `use_hotstart`\n"
        "is exposed under its name via `set()`/`get()`. Values are bool, int,\n"
        "float, or str: enum options take the enum value name (e.g.\n"
        "\"OSQP_DIRECT_SOLVER\", \"OSQP_DIAGONAL_PRECONDITIONER\") or an enum\n"
        "member. `set()` also converts strings such as \"1e-9\" or \"false\".\n"
        "Unset options use OSQP's own defaults for a double-precision,\n"
        "direct-solver build except `verbose`, which defaults to 0. See\n"
        "`keys()` and `defaults()` for the full list.");

    configuration.def(py::init<>());
    configuration.def("set",
        [](Configuration& self, const std::string& key, py::handle value) {
            self.set(key, to_option_value(value));
        },
        py::arg("key"), py::arg("value"),
        "Sets the option `key` to `value` (bool, int, float, str, or an enum "
        "member), converted to the option's kind. Raises ValueError for an "
        "unknown key or a value that does not convert to that kind.");
    configuration.def("get",
        &Configuration::get,
        py::arg("key"),
        "Returns the value of `key` (bool, int, float, or str), or its default "
        "if not set.");
    configuration.def("has",
        &Configuration::has,
        py::arg("key"),
        "True if `key` has been explicitly set.");
    configuration.def("keys",
        &Configuration::keys,
        "Sorted list of all settable option names.");
    configuration.def("defaults",
        &Configuration::defaults,
        "Mapping of option name -> default value.");
    configuration.def("reset",
        &Configuration::reset,
        py::arg("key"),
        "Reverts `key` to its default.");
    configuration.def("reset_all",
        &Configuration::reset_all,
        "Reverts every option to its default.");

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
                    py::arg("x0") = Eigen::VectorXd(), py::arg("y0") = Eigen::VectorXd(),
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
