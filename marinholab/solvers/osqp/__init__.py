"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv2.1 License
"""
from .solver import Solver
# TODO change this mess into inheritance via trampoline class
# Interface won't change, so this will do for now
from marinholab.solvers.osqp._core import OSQP_Solver
Configuration = OSQP_Solver.Configuration
Info = OSQP_Solver.Info
