"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv3 License

Basic evaluation script that solves a handful of quadratic programs with both
the qpOASES-based solver (solver-qpoases) and the OSQP-based solver
(solver-osqp) Python bindings, and checks that both solvers agree on the
optimal solution.

Prerequisites
-------------
Both Python packages must be installed in the current environment, e.g.

    pip install -e /path/to/solver-qpoases
    pip install -e /path/to/solver-osqp

Usage
-----
    python evaluation/compare_qpoases_osqp.py

The script exits with code 0 if all test cases agree (within tolerance)
and with code 1 if any test case disagrees or if a solver raised an
exception that the other solver did not.
"""
import sys

import numpy as np

try:
    from marinholab.solvers import qpoases
except ImportError as e:
    raise ImportError(
        "Could not import marinholab.solvers.qpoases. "
        "Install it with: pip install -e /path/to/solver-qpoases"
    ) from e

try:
    from marinholab.solvers import osqp
except ImportError as e:
    raise ImportError(
        "Could not import marinholab.solvers.osqp. "
        "Install it with: pip install -e /path/to/solver-osqp"
    ) from e

# Absolute/relative tolerance used when comparing the two solutions.
ATOL = 1e-3
RTOL = 1e-3


def _make_random_qp(n, m_ineq, seed):
    """Builds a random, strictly convex QP with n variables, m_ineq box-type
    inequality constraints and a single equality constraint (sum(x) == 0)."""
    rng = np.random.default_rng(seed)

    M = rng.standard_normal((n, n))
    H = M.T @ M + np.eye(n)  # Guaranteed symmetric positive definite.
    f = rng.standard_normal(n)

    # Box constraints -5 <= x <= 5, expressed as inequalities A x <= b.
    A = np.vstack([np.eye(n), -np.eye(n)])[:m_ineq, :]
    b = np.full((m_ineq,), 5.0)

    Aeq = np.ones((1, n))
    beq = np.array([0.0])

    return H, f, A, b, Aeq, beq


def _test_cases():
    cases = []

    # 1. Unconstrained.
    H = np.eye(4)
    f = np.array([1.0, -2.0, 0.5, 3.0])
    cases.append(("unconstrained", H, f, None, None, None, None))

    # 2. Equality only: sum(x) == 1.
    H = np.eye(3)
    f = np.array([1.0, 1.0, 1.0])
    Aeq = np.array([[1.0, 1.0, 1.0]])
    beq = np.array([1.0])
    cases.append(("equality_only", H, f, None, None, Aeq, beq))

    # 3. Inequality only: x <= 0.7, x >= 0 (i.e. -x <= 0).
    H = np.array([[4.0, 1.0], [1.0, 2.0]])
    f = np.array([1.0, 1.0])
    A = np.array([[1.0, 0.0],
                  [0.0, 1.0],
                  [-1.0, 0.0],
                  [0.0, -1.0]])
    b = np.array([0.7, 0.7, 0.0, 0.0])
    cases.append(("inequality_only", H, f, A, b, None, None))

    # 4. Both equality and inequality (classic OSQP documentation example).
    H = np.array([[4.0, 1.0], [1.0, 2.0]])
    f = np.array([1.0, 1.0])
    A = np.array([[1.0, 0.0],
                  [0.0, 1.0],
                  [-1.0, 0.0],
                  [0.0, -1.0]])
    b = np.array([0.7, 0.7, 0.0, 0.0])
    Aeq = np.array([[1.0, 1.0]])
    beq = np.array([1.0])
    cases.append(("equality_and_inequality", H, f, A, b, Aeq, beq))

    # 5. Larger, randomized QPs for a bit more coverage.
    for seed, n, m_ineq in [(0, 4, 8), (1, 6, 8), (2, 8, 4)]:
        H, f, A, b, Aeq, beq = _make_random_qp(n, m_ineq, seed)
        cases.append((f"random_n{n}_m{m_ineq}_seed{seed}", H, f, A, b, Aeq, beq))

    return cases


def _objective(H, f, x):
    return 0.5 * x @ H @ x + f @ x


def main():
    osqp_config = osqp.Configuration()
    # Tighten OSQP's tolerances/enable polishing so that its ADMM solution is
    # directly comparable to qpOASES' active-set solution.
    osqp_config.eps_absolute = 1e-7
    osqp_config.eps_relative = 1e-7
    osqp_config.maximum_iterations = 20000
    osqp_config.polishing = 1

    all_passed = True

    for name, H, f, A, b, Aeq, beq in _test_cases():
        print(f"--- {name} ---")

        # A fresh solver instance is created per test case, since the test cases have
        # differing problem/constraint dimensions and neither solver's hotstart/warm-start
        # machinery supports reusing an instance across a change in problem shape.
        qpoases_solver = qpoases.Solver()
        osqp_solver = osqp.Solver(osqp_config)

        try:
            x_qpoases = np.asarray(qpoases_solver.solve_quadratic_program(H, f, A, b, Aeq, beq))
            qpoases_error = None
        except Exception as e:  # noqa: BLE001
            x_qpoases = None
            qpoases_error = e

        try:
            x_osqp = np.asarray(osqp_solver.solve_quadratic_program(H, f, A, b, Aeq, beq))
            osqp_error = None
        except Exception as e:  # noqa: BLE001
            x_osqp = None
            osqp_error = e

        if qpoases_error is not None or osqp_error is not None:
            print(f"  qpOASES error: {qpoases_error}")
            print(f"  OSQP error:    {osqp_error}")
            all_passed = False
            continue

        matches = np.allclose(x_qpoases, x_osqp, atol=ATOL, rtol=RTOL)
        max_diff = np.max(np.abs(x_qpoases - x_osqp))

        print(f"  qpOASES x: {x_qpoases}")
        print(f"  OSQP    x: {x_osqp}")
        print(f"  max |diff|: {max_diff:.3e}")
        print(f"  qpOASES objective: {_objective(H, f, x_qpoases):.6f}")
        print(f"  OSQP    objective: {_objective(H, f, x_osqp):.6f}")
        print(f"  RESULT: {'PASS' if matches else 'FAIL'}")

        all_passed = all_passed and matches

    print()
    if all_passed:
        print("All test cases PASSED: solver-qpoases and solver-osqp agree.")
    else:
        print("Some test cases FAILED: solver-qpoases and solver-osqp disagree.")

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
