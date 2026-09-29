"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv2.1 License

Pure-Python enum types that mirror the OSQP enumerations accepted by
`Configuration.set()`.

These live in Python (not in the compiled extension) so the C++ public
header stays free of OSQP types. Each member's *name* is the value that
`Configuration` stores for the option (e.g. `LinsysSolverType.OSQP_DIRECT_SOLVER.name`
-> `"OSQP_DIRECT_SOLVER"`), and its *value* matches the underlying OSQP
integer. `Configuration.set()` accepts the member itself or its name.
"""
from __future__ import annotations

from enum import IntEnum


class LinsysSolverType(IntEnum):
    """OSQP linear system solvers (the ``linsys_solver`` option)."""

    OSQP_UNKNOWN_SOLVER = 0
    OSQP_DIRECT_SOLVER = 1
    OSQP_INDIRECT_SOLVER = 2


class PreconditionerType(IntEnum):
    """Preconditioners for the conjugate-gradient method (the ``cg_precond`` option)."""

    OSQP_NO_PRECONDITIONER = 0
    OSQP_DIAGONAL_PRECONDITIONER = 1


class Status(IntEnum):
    """OSQP solver status codes for the last solve."""

    OSQP_SOLVED = 1
    OSQP_SOLVED_INACCURATE = 2
    OSQP_PRIMAL_INFEASIBLE = 3
    OSQP_PRIMAL_INFEASIBLE_INACCURATE = 4
    OSQP_DUAL_INFEASIBLE = 5
    OSQP_DUAL_INFEASIBLE_INACCURATE = 6
    OSQP_MAX_ITER_REACHED = 7
    OSQP_TIME_LIMIT_REACHED = 8
    OSQP_NON_CVX = 9
    OSQP_SIGINT = 10
    OSQP_UNSOLVED = 11
