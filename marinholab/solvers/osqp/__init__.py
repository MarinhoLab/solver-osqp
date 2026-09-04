"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv2.1 License

Public API of the `marinholab.solvers.osqp` package.

`Solver` is a thin, numpy-friendly Python wrapper around the compiled OSQP
solver (`OSQP_Solver`). The configuration and enum types are re-exported for
convenience.
"""
from .solver import Solver
# TODO change this mess into inheritance via trampoline class
# Interface won't change, so this will do for now
from marinholab.solvers.osqp._core import OSQP_Solver

# Re-exported for convenience so users can write e.g. `osqp.Configuration`
# and `osqp.OSQP_Solver.Status.OSQP_SOLVED`.
Configuration = OSQP_Solver.Configuration
Info = OSQP_Solver.Info
LinsysSolverType = OSQP_Solver.LinsysSolverType
PreconditionerType = OSQP_Solver.PreconditionerType
Status = OSQP_Solver.Status

__all__ = [
    "Solver",
    "OSQP_Solver",
    "Configuration",
    "Info",
    "LinsysSolverType",
    "PreconditionerType",
    "Status",
]
