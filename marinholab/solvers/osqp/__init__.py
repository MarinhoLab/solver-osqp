"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv2.1 License

Public API of the `marinholab.solvers.osqp` package.

`Solver` is a thin, numpy-friendly Python wrapper around the compiled OSQP
solver (`OSQP_Solver`). The `Configuration` (keyed by option name) and the
enum types are re-exported for convenience.

The enum types are pure Python (see `_options.py`) so the compiled
extension's C++ header stays free of OSQP types.
"""
from .solver import Solver
# TODO change this mess into inheritance via trampoline class
# Interface won't change, so this will do for now
from marinholab.solvers.osqp._core import OSQP_Solver

# The configuration and info, backed by the compiled extension.
Configuration = OSQP_Solver.Configuration
Info = OSQP_Solver.Info
# The enum types, provided in pure Python so users can still write e.g.
# `osqp.LinsysSolverType.OSQP_DIRECT_SOLVER` (`Configuration.set` also accepts
# these members directly).
from marinholab.solvers.osqp._options import (
    LinsysSolverType,
    PreconditionerType,
    Status,
)

__all__ = [
    "Solver",
    "OSQP_Solver",
    "Configuration",
    "Info",
    "LinsysSolverType",
    "PreconditionerType",
    "Status",
]
