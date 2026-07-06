"""
Copyright (C) 2025 Murilo Marques Marinho (www.murilomarinho.info)
LGPLv3 License

This is an optional example that shows how `marinholab-solvers-osqp` can be
used, in its hierarchical (a.k.a. task-priority) mode, to control a
kinematically redundant robot manipulator using the differential kinematics
provided by `dqrobotics`. The robot and its motion are plotted with
`dqrobotics-pyplot`.

This example depends on `dqrobotics` and `dqrobotics-pyplot`, neither of
which is a dependency of this project. Install them, e.g. with:

    pip install --pre dqrobotics dqrobotics-pyplot
"""
try:
    import matplotlib.pyplot as plt
    import numpy as np
    from dqrobotics import i_, translation, vec4
    from dqrobotics.robots import KukaLw4Robot
    import dqrobotics_extensions.pyplot as dqp
except ImportError as e:
    raise ImportError(
        "This example requires the optional dependencies `dqrobotics` and "
        "`dqrobotics-pyplot`, which are not installed by default with "
        "marinholab-solvers-osqp. Install them, e.g. with:\n"
        "    pip install --pre dqrobotics dqrobotics-pyplot"
    ) from e

from marinholab.solvers import osqp


def main():
    """
    Move a Kuka LWR4 robot, in joint-space, from an initial configuration q0
    to a desired end-effector pose 20 cm away in the x-axis, using a
    two-level task-priority (hierarchical) controller built on top of
    `marinholab.solvers.osqp.Solver`:

    - Level 1 (highest priority): drive the end-effector translation to the
      desired target. Since the robot has 7 joints and the translation task
      only constrains 3 independent directions, this task alone leaves the
      remaining (redundant) directions undetermined.
    - Level 2: resolve that redundancy by minimizing the joint-velocity norm,
      constrained to reproduce exactly whatever level 1 has already
      achieved, so that it never disturbs the higher-priority task.

    This mirrors the two-level pattern of `hierarchical_example()` in
    `example.py`, applied to a robot-kinematics task.
    """
    robot = KukaLw4Robot.kinematics()

    q0 = np.array([0.0, np.pi / 3.0, 0.0, np.pi / 3.0, 0.0, np.pi / 3.0, 0.0])
    n = q0.size

    # The desired end-effector pose is the initial one, translated 20 cm
    # along the x-axis.
    x0 = robot.fkm(q0)
    td = translation(x0) + 0.2 * i_

    # Tighten the tolerances (relative to OSQP's defaults) for both levels, so
    # that the level-2 equality constraint that reproduces the level-1 task
    # (Aeq = Jt, beq = Jt @ u1) is enforced much more precisely.
    config = osqp.Configuration()
    config.eps_absolute = 1e-9
    config.eps_relative = 1e-9
    config.maximum_iterations = 20000

    solver_1 = osqp.Solver(config)  # Level 1: end-effector position control
    solver_2 = osqp.Solver(config)  # Level 2: redundancy resolution

    # Simple joint-velocity box constraint, shared by both levels.
    q_dot_max = 0.5 * np.ones(n)
    A = np.vstack((-np.eye(n), np.eye(n)))
    b = np.concatenate((q_dot_max, q_dot_max))

    gain = 2.0  # Proportional gain of the position-control task
    sampling_time = 0.01  # [s]
    maximum_iterations = 1000
    error_tolerance = 1e-4

    q = q0.copy()
    stored_qs = [q.copy()]
    stored_u1_norms = []
    stored_u2_norms = []
    error_norm = np.inf
    for _ in range(maximum_iterations):
        x = robot.fkm(q)
        J_pose = robot.pose_jacobian(q)
        Jt = robot.translation_jacobian(J_pose, x)

        e = vec4(translation(x) - td)
        error_norm = np.linalg.norm(e)
        if error_norm < error_tolerance:
            break

        # Level 1 optimizes the (rank-deficient) position-tracking objective
        # subject to the joint-velocity constraints only.
        H1 = Jt.T @ Jt
        f1 = gain * (Jt.T @ e)
        u1 = solver_1.solve_quadratic_program(H1, f1, A, b, None, None)

        # Level 2 minimizes the joint-velocity norm, restricted to the null
        # space of Jt so that it cannot affect (degrade) the level-1 result.
        H2 = np.eye(n)
        f2 = np.zeros(n)
        Aeq = Jt
        beq = Jt @ u1
        u2 = solver_2.solve_quadratic_program(H2, f2, A, b, Aeq, beq)

        # Store the norms of both levels' solutions so that their evolution
        # over the control loop can be plotted at the end.
        stored_u1_norms.append(np.linalg.norm(u1))
        stored_u2_norms.append(np.linalg.norm(u2))

        q = q + u2 * sampling_time
        stored_qs.append(q.copy())

    print(f"Converged after {len(stored_qs) - 1} iterations, "
          f"||e|| = {error_norm:.3e}")

    # Plot the initial (blue) and final (red) robot configurations, along
    # with the desired target position (green sphere).
    plt.figure()
    ax = plt.axes(projection='3d')
    ax.set_xlabel('$x$')
    ax.set_ylabel('$y$')
    ax.set_zlabel('$z$')
    plot_size = 1.0
    ax.set_xlim((-plot_size, plot_size))
    ax.set_ylim((-plot_size, plot_size))
    ax.set_zlim((0.0, plot_size))

    dqp.plot(robot, q=stored_qs[0], line_color='b', cylinder_color='b', cylinder_alpha=0.15)
    dqp.plot(robot, q=stored_qs[-1], line_color='r')
    dqp.plot(td, sphere=True, radius=0.02, color='g')
    ax.set_title("KukaLw4Robot: hierarchical position control, 20 cm along x")

    # Plot the evolution of ||u1|| and ||u2|| over the control loop, in the
    # same plot, to compare the level-1 (position control) and level-2
    # (redundancy resolution) task magnitudes at each iteration.
    plt.figure()
    time = sampling_time * np.arange(len(stored_u1_norms))
    plt.plot(time, stored_u1_norms, label="$||u_1||$")
    plt.plot(time, stored_u2_norms, label="$||u_2||$")
    plt.xlabel("time [s]")
    plt.ylabel("joint-velocity norm")
    plt.title("Level 1 and level 2 solution norms")
    plt.legend()

    plt.show()


if __name__ == "__main__":
    main()
