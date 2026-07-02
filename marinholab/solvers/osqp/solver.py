import numpy as np
from marinholab.solvers.osqp._core import OSQP_Solver

class Solver:
    def __init__(self, configuration=OSQP_Solver.Configuration()):
        self.configuration = configuration
        self.solver = OSQP_Solver(configuration)

    def solve_quadratic_program(self, H, f, A, b, Aeq, beq, x0=None, y0=None):

        if (A is None and b is not None) or (b is None and A is not None):
            raise ValueError(f"A={A} and b={b} must both be None or both not None.")
        if (Aeq is None and beq is not None) or (beq is None and Aeq is not None):
            raise ValueError(f"Aeq={Aeq} and beq={beq} must both be None or both not None.")

        if A is None:
            A = np.zeros((1,H.shape[0]))
            b = np.zeros((1,))
        if Aeq is None:
            Aeq = np.zeros((1,H.shape[0]))
            beq = np.zeros((1,))

        # x0 is an optional warm-start (e.g. a known feasible solution) for the primal
        # variable. When omitted, an empty array is forwarded and OSQP solves without
        # warm-starting.
        if x0 is None:
            x0 = np.zeros((0,))

        # y0 is an optional warm-start (e.g. a dual solution obtained from
        # get_info().dual_solution in a previous call) for the dual variable. When omitted,
        # an empty array is forwarded and OSQP solves without dual warm-starting. Its size
        # must match b.size()+beq.size() as actually sent to the solver above.
        if y0 is None:
            y0 = np.zeros((0,))

        return self.solver.solve_quadratic_program(H, f, A, b, Aeq, beq, x0, y0)

    def get_info(self):
        """
        Returns named solution-quality values (obj_val, dual_obj_val, prim_res, dual_res)
        and the dual solution (dual_solution) from the last successful call to
        solve_quadratic_program().
        """
        return self.solver.get_info()
