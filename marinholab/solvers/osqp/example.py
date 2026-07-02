"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv3 License
"""
import numpy as np
from marinholab.solvers import osqp

def positivedefinite():
    solver = osqp.Solver()

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.eye(4)
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    A = np.array([x[0], x[1], x[2], x[3]]).reshape((1, 4))
    b = np.array([0.0]).reshape((1, 1))

    # No constraints
    u = solver.solve_quadratic_program(H,
                                       f,
                                       np.array([0.0, 0.0, 0.0, 0.0]).reshape((1, 4)),
                                       np.array([0.0]),
                                       np.array([0.0, 0.0, 0.0, 0.0]).reshape((1, 4)),
                                       np.array([0.0])
                                       )
    # Equality only
    u_eq = solver.solve_quadratic_program(H,
                                          f,
                                          np.array([0.0, 0.0, 0.0, 0.0]).reshape((1, 4)),
                                          np.array([0.0]),
                                          A,
                                          b
                                          )

    # Inequality only
    u_ineq = solver.solve_quadratic_program(H,
                                            f,
                                            A,
                                            b,
                                            np.array([0.0, 0.0, 0.0, 0.0]).reshape((1, 4)),
                                            np.array([0.0])
                                            )

    # Both
    u_both = solver.solve_quadratic_program(H,
                                            f,
                                            A,
                                            b,
                                            A,
                                            b
                                            )

    print(u)
    print(u_eq)
    print(u_ineq)
    print(u_both)

def configuration_example():
    config = osqp.Configuration()
    config.eps_absolute = 1e-5
    config.eps_relative = 1e-5
    config.maximum_iterations = 10000
    config.verbose = 0
    solver = osqp.Solver(config)

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 1.0, 0.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.diag([1.0, 1.0, 1.0, 0.0])
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    Wl = -np.eye(4, 4)
    wl = -np.ones(4, )
    Wu = np.eye(4, 4)
    wu = np.ones(4, )

    W = np.vstack((Wl, Wu))
    w = np.concatenate((wl, wu))

    # No constraints
    u = solver.solve_quadratic_program(H,
                                       f,
                                       W,
                                       w,
                                       np.array([0.0, 0.0, 0.0, 0.0]).reshape((1, 4)),
                                       np.array([0.0])
                                       )
    print(u)

def nones():
    solver = osqp.Solver()

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.eye(4)
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    A = np.array([x[0], x[1], x[2], x[3]]).reshape((1, 4))
    b = np.array([0.0]).reshape((1, 1))

    # No constraints
    u = solver.solve_quadratic_program(H,
                                       f,
                                       None,
                                       None,
                                       None,
                                       None
                                       )

    # Equality only
    u_eq = solver.solve_quadratic_program(H,
                                          f,
                                          None,
                                          None,
                                          A,
                                          b
                                          )

    # Inequality only
    u_ineq = solver.solve_quadratic_program(H,
                                            f,
                                            A,
                                            b,
                                            None,
                                            None
                                            )

def warmstart_example():
    solver = osqp.Solver()

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.eye(4)
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    A = np.array([x[0], x[1], x[2], x[3]]).reshape((1, 4))
    b = np.array([0.0]).reshape((1, 1))

    # Solve once, without a warm-start, to obtain a first solution.
    u = solver.solve_quadratic_program(H, f, A, b, None, None)
    print(f"Solution without warm-start: {u}")

    # A known feasible solution (here, the solution found above) can be used to
    # warm-start the next solve, which typically reduces the number of ADMM
    # iterations OSQP needs to converge.
    x0 = u
    u_warmstarted = solver.solve_quadratic_program(H, f, A, b, None, None, x0)
    print(f"Solution with warm-start:    {u_warmstarted}")

def info_example():
    solver = osqp.Solver()

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.eye(4)
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    A = np.array([x[0], x[1], x[2], x[3]]).reshape((1, 4))
    b = np.array([0.0]).reshape((1, 1))

    u = solver.solve_quadratic_program(H, f, A, b, None, None)
    print(f"Solution: {u}")

    # After a successful solve_quadratic_program() call, get_info() returns
    # named solution-quality values, e.g. objective/dual objective values and
    # primal/dual residuals, as well as the dual solution.
    info = solver.get_info()
    print(f"obj_val:       {info.obj_val}")
    print(f"dual_obj_val:  {info.dual_obj_val}")
    print(f"prim_res:      {info.prim_res}")
    print(f"dual_res:      {info.dual_res}")
    print(f"dual_solution: {info.dual_solution}")

def main():
    positivedefinite()
    configuration_example()
    nones()
    warmstart_example()
    info_example()


if __name__ == "__main__":
    main()
