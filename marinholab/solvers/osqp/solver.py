from __future__ import annotations

import numpy as np
from marinholab.solvers.osqp._core import OSQP_Solver


class Solver:
    """Thin, numpy-friendly wrapper around the compiled OSQP solver.

    Accepts ``None`` for the constraint matrices ``A``/``b``/``Aeq``/``beq``
    and converts them into suitably sized zero matrices before calling the
    underlying solver. All matrices are given as dense, row-major numpy
    arrays (or anything numpy can treat as one).
    """

    def __init__(self, configuration: OSQP_Solver.Configuration | None = None) -> None:
        self.configuration: OSQP_Solver.Configuration = (
            configuration if configuration is not None else OSQP_Solver.Configuration()
        )
        self.solver: OSQP_Solver = OSQP_Solver(self.configuration)

    def solve_quadratic_program(
        self,
        H: np.ndarray,
        f: np.ndarray,
        A: np.ndarray | None,
        b: np.ndarray | None,
        Aeq: np.ndarray | None,
        beq: np.ndarray | None,
        x0: np.ndarray | None = None,
        y0: np.ndarray | None = None,
    ) -> np.ndarray:
        """Solves ``min(x) 0.5*x'Hx + f'x`` subject to ``Ax <= b`` and ``Aeq*x = beq``.

        Any of ``A``/``b``/``Aeq``/``beq`` may be ``None`` (meaning "no such
        constraint"), but the matrix and its right-hand side must be provided
        together. ``x0`` and ``y0`` are optional warm-starts for the primal and
        dual variables; pass ``None`` (the default) to skip warm-starting.
        Returns the optimal ``x`` as a 1-D numpy array.
        """

        if (A is None) != (b is None):
            raise ValueError(f"A={A} and b={b} must both be None or both not None.")
        if (Aeq is None) != (beq is None):
            raise ValueError(f"Aeq={Aeq} and beq={beq} must both be None or both not None.")

        # The solver requires all six matrices; replace the ``None``
        # constraints with a single trivially satisfied zero row.
        A_full = np.zeros((1, H.shape[0])) if A is None else A
        b_full = np.zeros((1,)) if b is None else b
        Aeq_full = np.zeros((1, H.shape[0])) if Aeq is None else Aeq
        beq_full = np.zeros((1,)) if beq is None else beq

        # x0 is an optional warm-start (e.g. a known feasible solution) for the primal
        # variable. When omitted, an empty array is forwarded and OSQP solves without
        # warm-starting.
        x0_full = np.zeros((0,)) if x0 is None else x0

        # y0 is an optional warm-start (e.g. a dual solution obtained from
        # get_info().dual_solution in a previous call) for the dual variable. When omitted,
        # an empty array is forwarded and OSQP solves without dual warm-starting. Its size
        # must match b.size()+beq.size() as actually sent to the solver above.
        y0_full = np.zeros((0,)) if y0 is None else y0

        return self.solver.solve_quadratic_program(H, f, A_full, b_full, Aeq_full, beq_full, x0_full, y0_full)

    def get_info(self) -> OSQP_Solver.Info:
        """
        Returns named solution-quality values (obj_val, dual_obj_val, prim_res, dual_res)
        and the dual solution (dual_solution) from the last successful call to
        solve_quadratic_program().
        """
        return self.solver.get_info()
