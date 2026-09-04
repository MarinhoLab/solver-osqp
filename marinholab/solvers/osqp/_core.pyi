"""Type stubs for the compiled `_core` pybind11 extension module.

The real module is built from ``src/core.cpp``; this file only exists so that
type checkers (e.g. Pyright) can understand the public surface of the
extension without having to parse C++.
"""

from enum import IntEnum

import numpy as np


class LinsysSolverType(IntEnum):
    """OSQP linear system solvers."""

    OSQP_UNKNOWN_SOLVER: LinsysSolverType
    OSQP_DIRECT_SOLVER: LinsysSolverType
    OSQP_INDIRECT_SOLVER: LinsysSolverType


class PreconditionerType(IntEnum):
    """Preconditioners for the conjugate-gradient method."""

    OSQP_NO_PRECONDITIONER: PreconditionerType
    OSQP_DIAGONAL_PRECONDITIONER: PreconditionerType


class Status(IntEnum):
    """OSQP solver status codes returned for the last solve."""

    OSQP_SOLVED: Status
    OSQP_SOLVED_INACCURATE: Status
    OSQP_PRIMAL_INFEASIBLE: Status
    OSQP_PRIMAL_INFEASIBLE_INACCURATE: Status
    OSQP_DUAL_INFEASIBLE: Status
    OSQP_DUAL_INFEASIBLE_INACCURATE: Status
    OSQP_MAX_ITER_REACHED: Status
    OSQP_TIME_LIMIT_REACHED: Status
    OSQP_NON_CVX: Status
    OSQP_SIGINT: Status
    OSQP_UNSOLVED: Status


class OSQP_Solver:
    """High-level, reusable solver for quadratic programs (QPs) based on OSQP.

    Solves ``min(x) 0.5*x'Hx + f'x`` subject to ``Ax <= b`` and
    ``Aeq*x = beq`` (MATLAB `quadprog`-like signature). Once the problem has
    been solved once, subsequent solves are warm-started by default (see
    ``Configuration.use_hotstart``).
    """

    # Nested aliases so the enums are also reachable as
    # ``OSQP_Solver.LinsysSolverType`` etc. (matching the runtime layout, where
    # ``export_values()`` binds them onto the class).
    LinsysSolverType: type[LinsysSolverType] = LinsysSolverType
    PreconditionerType: type[PreconditionerType] = PreconditionerType
    Status: type[Status] = Status

    class Configuration:
        """All user-configurable solver options.

        Members are a 1:1 mapping of OSQP's ``OSQPSettings`` fields (plus the
        wrapper-specific ``use_hotstart``). Defaults match OSQP's own defaults
        for a standard double-precision, direct-solver build (see
        ``osqp_set_default_settings()``); see the C++ header
        (``include/marinholab/solvers/osqp.h``) and the OSQP documentation
        (https://osqp.org/docs/) for the meaning of each option.
        """

        #: Reuse the existing OSQP solver and update its data in place instead of re-running osqp_setup() when the problem shape is unchanged.
        use_hotstart: bool
        #: Device identifier; currently used for CUDA devices.
        device: int
        #: Linear system solver to use.
        linsys_solver: LinsysSolverType
        #: Whether the solution is allocated during osqp_setup().
        allocate_solution: int
        #: Whether solver progress is written out (0 = quiet, the default).
        verbose: int
        #: Level of detail for profiler annotations.
        profiler_level: int
        #: Whether OSQP warm-starts from the previous solution between consecutive osqp_solve() calls.
        warm_starting: int
        #: Number of heuristic data-scaling iterations; 0 disables scaling.
        scaling: int
        #: Whether the ADMM solution is polished to improve accuracy.
        polishing: int
        #: ADMM penalty parameter (scalar).
        rho: float
        #: Whether rho is a scalar or a vector.
        rho_is_vec: int
        #: ADMM regularization parameter (improves conditioning).
        sigma: float
        #: ADMM relaxation parameter.
        alpha: float
        #: Maximum number of CG iterations per solve.
        cg_max_iter: int
        #: Number of consecutive zero CG iterations before the tolerance is halved.
        cg_tol_reduction: int
        #: CG tolerance, as a fraction of the ADMM residuals.
        cg_tol_fraction: float
        #: Preconditioner used by the CG method.
        cg_precond: PreconditionerType
        #: ADMM rho stepsize adaptation method.
        adaptive_rho: int
        #: Interval between rho adaptations (used with the iterations-based method).
        adaptive_rho_interval: int
        #: Adaptation parameter controlling when non-fixed rho adaptations occur.
        adaptive_rho_fraction: float
        #: Tolerance applied when adapting rho (min ratio between new and current rho).
        adaptive_rho_tolerance: float
        #: Maximum number of ADMM iterations.
        max_iter: int
        #: Absolute solution tolerance.
        eps_abs: float
        #: Relative solution tolerance.
        eps_rel: float
        #: Primal infeasibility detection tolerance.
        eps_prim_inf: float
        #: Dual infeasibility detection tolerance.
        eps_dual_inf: float
        #: Whether the scaled termination criteria are used.
        scaled_termination: int
        #: Interval at which termination is checked; 0 disables the periodic check.
        check_termination: int
        #: Whether the duality-gap termination criteria are used.
        check_dualgap: int
        #: Maximum time to solve the problem, in seconds.
        time_limit: float
        #: Regularization parameter used by polishing.
        delta: float
        #: Number of iterative refinement steps performed during polishing.
        polish_refine_iter: int

        def __init__(self) -> None: ...

    class Info:
        """Solution-quality values obtained from the last successful call to ``solve_quadratic_program()``."""

        #: Primal objective value.
        obj_val: float
        #: Dual objective value.
        dual_obj_val: float
        #: Norm of the primal residual.
        prim_res: float
        #: Norm of the dual residual.
        dual_res: float
        #: Dual solution, i.e. the Lagrange multiplier associated with l <= Ax <= u.
        dual_solution: np.ndarray

        def __init__(self) -> None: ...

    def __init__(self, configuration: Configuration | None = None) -> None:
        """Constructs a solver with the given configuration (defaults to the default configuration)."""
        ...

    def solve_quadratic_program(
        self,
        H: np.ndarray,
        f: np.ndarray,
        A: np.ndarray,
        b: np.ndarray,
        Aeq: np.ndarray,
        beq: np.ndarray,
        x0: np.ndarray = ...,
        y0: np.ndarray = ...,
    ) -> np.ndarray:
        """Solves ``min(x) 0.5*x'Hx + f'x s.t. Ax <= b, Aeq*x = beq``.

        Method signature is compatible with MATLAB's ``quadprog``. Returns the
        optimal ``x``. ``x0`` and ``y0`` are optional warm-starts for the primal
        and dual variables (pass an empty array to skip warm-starting).
        """
        ...

    def get_info(self) -> Info:
        """Returns the solution-quality values and dual solution from the last successful solve."""
        ...

    def test_vectorxd(self, v: np.ndarray) -> np.ndarray:
        """Round-trips a vector to help evaluate the Eigen <-> std conversions."""
        ...

    def test_matrixxd(self, m: np.ndarray) -> np.ndarray:
        """Round-trips a matrix to help evaluate the Eigen <-> std conversions."""
        ...


__version__: str
