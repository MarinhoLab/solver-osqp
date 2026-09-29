"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv2.1 License

Tests for `Solver` (in particular reusing one instance for problems whose
number of variables or constraints changes between calls) and for the
name-keyed `Configuration` (`set`/`get`/`has`/`keys`/`defaults`/`reset`).
"""
from __future__ import annotations

import numpy as np
import pytest

from marinholab.solvers import osqp

H2 = np.eye(2)
F2 = np.array([-1.0, -1.0])
A_THREE_ROWS = np.array([[1.0, 0.0], [0.0, 1.0], [1.0, 1.0]])
B_THREE_ROWS = np.array([0.2, 0.3, 10.0])
A_ONE_ROW = np.array([[1.0, 0.0]])
B_ONE_ROW = np.array([0.2])


def _configuration(use_hotstart: bool = True, eps_abs: float = 1.0e-10, eps_rel: float = 1.0e-10, max_iter: int = 20000, polishing: bool = True) -> osqp.Configuration:
    configuration = osqp.Configuration()
    configuration.set("use_hotstart", use_hotstart)
    configuration.set("eps_abs", eps_abs)
    configuration.set("eps_rel", eps_rel)
    configuration.set("max_iter", max_iter)
    configuration.set("polishing", polishing)
    return configuration


def _random_problem(rng: np.random.Generator, n: int, m: int):
    """A strictly convex QP for which x = 0 is feasible."""
    M = rng.standard_normal((n, n))
    H = M @ M.T + np.eye(n)
    f = rng.standard_normal(n)
    A = rng.standard_normal((m, n))
    b = rng.uniform(0.1, 1.0, m)
    return H, f, A, b


def test_single_solve():
    x = osqp.Solver(_configuration()).solve_quadratic_program(H2, F2, A_ONE_ROW, B_ONE_ROW, None, None)
    assert np.allclose(x, [0.2, 1.0], atol=1e-6)


@pytest.mark.parametrize("use_hotstart", [True, False])
def test_fewer_constraints_after_first_solve(use_hotstart: bool):
    solver = osqp.Solver(_configuration(use_hotstart))
    x = solver.solve_quadratic_program(H2, F2, A_THREE_ROWS, B_THREE_ROWS, None, None)
    assert np.allclose(x, [0.2, 0.3], atol=1e-6)
    x = solver.solve_quadratic_program(H2, F2, A_ONE_ROW, B_ONE_ROW, None, None)
    assert np.allclose(x, [0.2, 1.0], atol=1e-6)


@pytest.mark.parametrize("use_hotstart", [True, False])
def test_more_constraints_after_first_solve(use_hotstart: bool):
    solver = osqp.Solver(_configuration(use_hotstart))
    x = solver.solve_quadratic_program(H2, F2, A_ONE_ROW, B_ONE_ROW, None, None)
    assert np.allclose(x, [0.2, 1.0], atol=1e-6)
    x = solver.solve_quadratic_program(H2, F2, A_THREE_ROWS, B_THREE_ROWS, None, None)
    assert np.allclose(x, [0.2, 0.3], atol=1e-6)


@pytest.mark.parametrize("use_hotstart", [True, False])
def test_different_number_of_variables(use_hotstart: bool):
    solver = osqp.Solver(_configuration(use_hotstart))
    x = solver.solve_quadratic_program(H2, F2, A_ONE_ROW, B_ONE_ROW, None, None)
    assert np.allclose(x, [0.2, 1.0], atol=1e-6)
    H3 = np.eye(3)
    f3 = np.array([-1.0, -1.0, -1.0])
    x = solver.solve_quadratic_program(H3, f3, np.array([[0.0, 0.0, 1.0]]), np.array([0.5]), None, None)
    assert np.allclose(x, [1.0, 1.0, 0.5], atol=1e-6)


def test_same_size_warm_start():
    """Consecutive problems of the same size, as in a control loop."""
    rng = np.random.default_rng(1)
    solver = osqp.Solver(_configuration())
    H, f, A, b = _random_problem(rng, 5, 8)
    for _ in range(50):
        f = f + 0.05 * rng.standard_normal(5)
        b = np.clip(b + 0.05 * rng.standard_normal(8), 0.1, None)
        x_reused = solver.solve_quadratic_program(H, f, A, b, None, None)
        x_fresh = osqp.Solver(_configuration()).solve_quadratic_program(H, f, A, b, None, None)
        assert np.allclose(x_reused, x_fresh, atol=1e-6)


@pytest.mark.parametrize("use_hotstart", [True, False])
def test_reused_solver_matches_fresh_solver_when_sizes_change(use_hotstart: bool):
    rng = np.random.default_rng(0)
    solver = osqp.Solver(_configuration(use_hotstart))
    for _ in range(200):
        n = int(rng.integers(2, 8))
        m = int(rng.integers(1, 12))
        H, f, A, b = _random_problem(rng, n, m)
        x_fresh = osqp.Solver(_configuration()).solve_quadratic_program(H, f, A, b, None, None)
        x_reused = solver.solve_quadratic_program(H, f, A, b, None, None)
        assert np.allclose(x_reused, x_fresh, atol=1e-6)


# OSQP's own defaults for its tolerances/parameters (double precision,
# direct-solver build), from osqp_set_default_settings().
_OSQP_TOLERANCE_DEFAULTS = {
    "eps_abs": 1.0e-3,
    "eps_rel": 1.0e-3,
    "eps_prim_inf": 1.0e-4,
    "eps_dual_inf": 1.0e-4,
    "sigma": 1.0e-6,
    "delta": 1.0e-6,
    "rho": 0.1,
    "alpha": 1.6,
    "cg_tol_fraction": 0.15,
    "adaptive_rho_tolerance": 5.0,
    "adaptive_rho_fraction": 0.4,
    "time_limit": 1.0e10,
}


@pytest.mark.parametrize("key,expected", sorted(_OSQP_TOLERANCE_DEFAULTS.items()))
def test_defaults_match_osqp(key: str, expected: float):
    value = osqp.Configuration().get(key)
    assert isinstance(value, float)
    assert value == pytest.approx(expected, rel=1e-12, abs=0.0)


def test_defaults_ints_and_booleans():
    configuration = osqp.Configuration()
    assert configuration.get("max_iter") == 4000
    assert type(configuration.get("max_iter")) is int
    assert configuration.get("use_hotstart") is True
    # `verbose` defaults to quiet (off) instead of OSQP's own default (on).
    assert configuration.get("verbose") is False
    assert configuration.get("linsys_solver") == "OSQP_DIRECT_SOLVER"
    assert configuration.get("cg_precond") == "OSQP_DIAGONAL_PRECONDITIONER"


def test_values_are_typed():
    configuration = osqp.Configuration()
    assert type(configuration.get("use_hotstart")) is bool
    assert type(configuration.get("max_iter")) is int
    assert type(configuration.get("eps_abs")) is float
    assert configuration.get("linsys_solver") == "OSQP_DIRECT_SOLVER"
    defaults = configuration.defaults()
    assert sorted(defaults) == configuration.keys()
    assert {type(v) for v in defaults.values()} == {bool, int, float, str}


@pytest.mark.parametrize("key,value,expected", [
    ("eps_abs", 1.0e-9, 1.0e-9),
    ("eps_abs", "1.0e-9", 1.0e-9),
    ("eps_abs", 2, 2.0),
    ("eps_abs", np.float64(3.0e-9), 3.0e-9),
    ("max_iter", 3, 3),
    ("max_iter", "3", 3),
    ("max_iter", np.int64(4), 4),
    ("verbose", False, False),
    ("verbose", "false", False),
    ("linsys_solver", osqp.LinsysSolverType.OSQP_DIRECT_SOLVER, "OSQP_DIRECT_SOLVER"),
    ("linsys_solver", "OSQP_DIRECT_SOLVER", "OSQP_DIRECT_SOLVER"),
    ("cg_precond", osqp.PreconditionerType.OSQP_NO_PRECONDITIONER, "OSQP_NO_PRECONDITIONER"),
])
def test_set_converts_to_the_option_kind(key: str, value, expected):
    configuration = osqp.Configuration()
    configuration.set(key, value)
    assert configuration.has(key)
    got = configuration.get(key)
    assert got == expected and type(got) is type(expected)


@pytest.mark.parametrize("key,value", [
    ("max_iter", 1.5),
    ("max_iter", "1.5"),
    ("verbose", 1.0),
    ("verbose", "maybe"),
    ("eps_abs", True),
    ("eps_abs", "small"),
    ("linsys_solver", 4),
    ("linsys_solver", "OSQP_NOPE"),
    ("no_such_option", 1),
])
def test_set_rejects_values_of_the_wrong_kind(key: str, value):
    configuration = osqp.Configuration()
    with pytest.raises(ValueError):
        configuration.set(key, value)
    if key != "no_such_option":
        assert not configuration.has(key)


def test_string_and_number_give_the_same_solution():
    rng = np.random.default_rng(2)
    H, f, A, b = _random_problem(rng, 5, 8)
    solutions = []
    for value in (1.0e-9, "1.0e-9"):
        configuration = osqp.Configuration()
        configuration.set("eps_abs", value)
        solutions.append(osqp.Solver(configuration).solve_quadratic_program(H, f, A, b, None, None))
    assert np.array_equal(solutions[0], solutions[1])
