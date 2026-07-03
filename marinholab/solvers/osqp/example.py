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
    # iterations OSQP needs to converge. Similarly, the dual solution obtained
    # from get_info().dual_solution can be used to warm-start the dual variable y0.
    x0 = u
    y0 = solver.get_info().dual_solution
    u_warmstarted = solver.solve_quadratic_program(H, f, A, b, None, None, x0, y0)
    print(f"Solution with warm-start:    {u_warmstarted}")

def dual_warmstart_example():
    solver = osqp.Solver()

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])

    x_tilde = (x - xd).reshape((4, 1))

    J = np.eye(4)
    H = J.T @ J
    f = 1.0 * J.T @ x_tilde

    A = np.array([x[0], x[1], x[2], x[3]]).reshape((1, 4))
    b = np.array([0.0]).reshape((1, 1))

    # Solve once, without any warm-start, to obtain a first solution and its
    # associated dual solution (Lagrange multipliers), via get_info().dual_solution.
    u = solver.solve_quadratic_program(H, f, A, b, None, None)
    y0 = solver.get_info().dual_solution
    print(f"Solution without warm-start:          {u}")
    print(f"Dual solution:                         {y0}")

    # y0 can be used on its own (i.e. without a primal x0) to warm-start only the
    # dual variable y of the next solve.
    u_dual_warmstarted = solver.solve_quadratic_program(H, f, A, b, None, None, None, y0)
    print(f"Solution with dual warm-start only:    {u_dual_warmstarted}")

    # x0 and y0 can also be combined to warm-start both the primal and dual
    # variables at the same time.
    u_both_warmstarted = solver.solve_quadratic_program(H, f, A, b, None, None, u, y0)
    print(f"Solution with primal+dual warm-start:  {u_both_warmstarted}")

def hierarchical_example():
    """
    Two-level hierarchical (a.k.a. task-priority) quadratic program, a common
    pattern in robot control: solve a higher-priority (level 1) task first,
    then use whatever redundancy is left (i.e. its null space) to optimize a
    lower-priority (level 2) objective without disturbing the level-1 task.
    """
    # Tighten the tolerances (relative to OSQP's defaults) for both levels, so
    # that the level-2 equality constraint that reproduces the level-1 task
    # value (Aeq = J1, beq = J1@u1) is enforced much more precisely, i.e.
    # J1@u2 ends up much closer to J1@u1 than with the default tolerances.
    config = osqp.Configuration()
    config.eps_absolute = 1e-9
    config.eps_relative = 1e-9
    config.maximum_iterations = 20000

    solver_1 = osqp.Solver(config)
    solver_2 = osqp.Solver(config)

    x = np.array([1.0, 0.0, 0.0, 0.0])
    xd = np.array([0.0, 0.0, 0.0, 1.0])
    x_tilde = (x - xd).reshape((4, 1))

    # Inequality constraints shared by both levels: a loose box, -2 <= u <= 2,
    # plus a row that requires u[1]+u[2]+u[3] >= 1. This last row does not
    # involve u[0] at all, so it only restricts the redundant/null-space
    # directions of the level-1 task defined by J1 below.
    Abox = np.vstack((-np.eye(4), np.eye(4)))
    bbox = np.concatenate((2.0*np.ones((4,)), 2.0*np.ones((4,))))
    Arow = np.array([0.0, -1.0, -1.0, -1.0]).reshape((1, 4))
    brow = np.array([-1.0])
    A = np.vstack((Abox, Arow))
    b = np.concatenate((bbox, brow))

    # Level 1: a rank-deficient task Jacobian J1 that only "sees" the first
    # decision-variable coordinate. H1 = J1.T @ J1 is only positive
    # semi-definite (rank 1 out of 4): the other 3 decision-variable
    # directions lie in H1's null space and are therefore left completely
    # undetermined by the level-1 objective alone (any value satisfying the
    # inequality constraints is equally optimal for level 1).
    J1 = np.array([1.0, 0.0, 0.0, 0.0]).reshape((1, 4))
    e1 = J1 @ x_tilde
    H1 = J1.T @ J1
    f1 = J1.T @ e1

    # Level 1 optimizes the (semi-definite) task objective subject to the
    # inequality constraints only. Note that u[1:] end up satisfying the
    # inequality constraint but are otherwise arbitrary, since the level-1
    # objective is blind to them.
    u1 = solver_1.solve_quadratic_program(H1, f1, A, b, None, None)
    print(f"Level 1 solution:            {u1}")
    print(f"Level 1 task value (J1@u1):  {J1 @ u1}")

    # Level 2 minimizes the Euclidean norm of the decision variables
    # (H2 = I, f2 = 0), subject to the same inequality constraints as level 1,
    # plus an equality constraint that fixes the level-1 task value at the
    # optimum found above (Aeq = J1, beq = J1@u1). This restricts the level-2
    # solution to move only within the null space of J1, guaranteeing that it
    # cannot affect (degrade) the level-1 solution while it optimizes a
    # lower-priority objective: here, u[1:] converge to the minimum-norm
    # values that still satisfy u[1]+u[2]+u[3] >= 1, instead of the arbitrary
    # values level 1 left them at.
    H2 = np.eye(4)
    f2 = np.zeros((4,))
    Aeq = J1
    beq = J1 @ u1

    u2 = solver_2.solve_quadratic_program(H2, f2, A, b, Aeq, beq)
    print(f"Level 2 solution:            {u2}")
    print(f"Level 2 task value (J1@u2):  {J1 @ u2}")
    print(f"|J1@u2 - J1@u1|:             {np.abs(J1 @ u2 - J1 @ u1)}")
    print(f"||u1|| = {np.linalg.norm(u1):.6f}, ||u2|| = {np.linalg.norm(u2):.6f}")

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
    dual_warmstart_example()
    hierarchical_example()
    info_example()


if __name__ == "__main__":
    main()
