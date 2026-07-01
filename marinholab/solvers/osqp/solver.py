import numpy as np
from marinholab.solvers.osqp._core import OSQP_Solver

class Solver:
    def __init__(self, configuration=OSQP_Solver.Configuration()):
        self.configuration = configuration
        self.solver = OSQP_Solver(configuration)

    def solve_quadratic_program(self, H, f, A, b, Aeq, beq):

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

        return self.solver.solve_quadratic_program(H, f, A, b, Aeq, beq)
