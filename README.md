# solver-osqp

A [OSQP](https://github.com/osqp/osqp) wrapper for Python that ships prebuilt
binaries. It exposes OSQP's first-order ADMM solver through a thin,
numpy-friendly, MATLAB-`quadprog`-like interface, in the C++ namespace
`marinholab::solvers::osqp` and the Python package `marinholab.solvers.osqp`.

```console
pip install marinholab-solvers-osqp
```

## Overview

Given a symmetric matrix `H`, a vector `f`, and (optionally) inequality and
equality constraint matrices, the solver solves the quadratic program

```
min_x   0.5 * x' H x + f' x
s.t.    A x <= b
        Aeq x = beq
```

Internally the inequality and equality rows are stacked into OSQP's single
`l <= A x <= u` form. Once a problem has been solved once, subsequent solves on
the same `Solver` instance reuse the OSQP solver and update its data in place
by default (`Configuration.use_hotstart = True`), which is the main performance
benefit for repeated, related QPs. OSQP additionally warm-starts from the
previous iterate between `osqp_solve()` calls
(`Configuration.warm_starting = 1`).

## Quickstart

```python
import numpy as np
from marinholab.solvers import osqp

solver = osqp.Solver()

H = np.eye(2)                          # positive definite Hessian
f = np.array([-1.0, -1.0])             # linear term
A = np.array([[1.0, 0.0]])             # x[0] <= 0.2
b = np.array([0.2])

x = solver.solve_quadratic_program(H, f, A, b,
                                   Aeq=np.zeros((1, 2)),
                                   beq=np.zeros((1,)))
# x ≈ [0.2, 1.0]
```

### Omitting constraints

Any of the four constraint arguments (`A`, `b`, `Aeq`, `beq`) can be `None`,
meaning "no such constraint". The matrix and its right-hand side must be
omitted (or provided) together:

```python
# Unconstrained
x = solver.solve_quadratic_program(H, f, None, None, None, None)

# Equality constraints only
x = solver.solve_quadratic_program(H, f, None, None, Aeq, beq)
```

### Warm-starting

`solve_quadratic_program` accepts optional `x0` and `y0` warm-starts for the
primal and dual variables. The dual solution returned by `get_info()` can be
used to warm-start the next solve:

```python
x = solver.solve_quadratic_program(H, f, A, b, None, None)
y0 = solver.get_info().dual_solution
x = solver.solve_quadratic_program(H, f, A, b, None, None, x0=x, y0=y0)
```

### Solution information

`solver.get_info()` returns an `Info` with the objective and dual-objective
values, the primal and dual residual norms, and the dual solution
(`dual_solution`):

```python
solver.solve_quadratic_program(H, f, A, b, Aeq, beq)
info = solver.get_info()
info.obj_val, info.dual_obj_val, info.prim_res, info.dual_res, info.dual_solution
```

## C++ API

The same solver is available directly in C++ (the Python wrapper is a thin
pybind11 layer over it). It is built with
[Eigen](https://eigen.tuxfamily.org/) and
[OSQP](https://github.com/osqp/osqp):

```cpp
#include <marinholab/solvers/osqp.h>

namespace osqp = marinholab::solvers::osqp;

osqp::Configuration config;
config.eps_abs = 1.0e-9;              // the rest keeps its defaults
config.polishing = 1;

osqp::Solver solver(config);

Eigen::MatrixXd H = Eigen::MatrixXd::Identity(2, 2);
Eigen::VectorXd f(2);
f << -1.0, -1.0;

Eigen::MatrixXd A(1, 2);
A << 1.0, 0.0;                            // x[0] <= 0.2
Eigen::VectorXd b(1);
b << 0.2;

Eigen::MatrixXd Aeq = Eigen::MatrixXd::Zero(1, 2);
Eigen::VectorXd beq = Eigen::VectorXd::Zero(1);

Eigen::VectorXd x = solver.solve_quadratic_program(H, f, A, b, Aeq, beq);
// x ≈ [0.2, 1.0]

osqp::Solver::Info info = solver.get_info();   // obj_val, prim_res, dual_solution, ...
```

The API mirrors the Python one: `solve_quadratic_program(H, f, A, b, Aeq, beq,
x0, y0)` solves the QP above and `get_info()` reports the solution quality.
`Configuration` exposes the same fields documented below.

A standalone, self-contained C++ program using this API lives in
[`example/example.cpp`](example/example.cpp) (target `example_osqp`). It is
built only when `BUILD_EXAMPLES=ON` so it does not affect `pip install .`:

```console
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
cmake --build build --target example_osqp
./build/example/example_osqp   # prints x ≈ [0.2, 1.0] and the residuals
```

## Type checking (Pyright)

The Python wrapper is fully type-annotated and ships a PEP 561 `py.typed`
marker plus a `_core.pyi` type stub for the compiled `_core` extension, so
downstream projects can be checked against it. To type-check the package:

```console
pyright        # configuration in pyrightconfig.json (python 3.9, mode basic)
```

This must pass with **0 errors / 0 warnings**. Keep `_core.pyi` in sync with
the pybind11 surface in `src/core.cpp`.

## Configuration

All of OSQP's `OSQPSettings` fields are exposed, plus one wrapper-specific
setting. Create a `Configuration`, tweak the fields you need, and pass it to
the solver:

```python
config = osqp.Configuration()
config.eps_abs = 1.0e-9                    # tighter absolute tolerance
config.eps_rel = 1.0e-9                    # tighter relative tolerance
config.max_iter = 20000                    # more ADMM iterations
config.polishing = 1                       # polish the ADMM solution
solver = osqp.Solver(config)
```

The enum types are re-exported for convenience: `osqp.LinsysSolverType`,
`osqp.PreconditionerType`, and `osqp.Status`.

### Wrapper-specific option

| Option | Default | Type | Description |
|---|---|---|---|
| `use_hotstart` | `True` | `bool` | Reuse the existing OSQP solver and update its data in place instead of re-running `osqp_setup()` when the problem shape is unchanged. |

### OSQP options (linear algebra & control)

These map 1:1 onto OSQP's `OSQPSettings` fields. Defaults match OSQP's own
defaults for a standard double-precision, direct-solver build (see
`osqp_set_default_settings()`), except `verbose`, which defaults to off so the
solver is quiet by default. See the
[OSQP documentation](https://osqp.org/docs/) for a full description of each
option.

| Option | Default | Type | Description |
|---|---|---|---|
| `device` | `0` | `int` | Device identifier; currently used for CUDA devices. |
| `linsys_solver` | `OSQP_DIRECT_SOLVER` | `LinsysSolverType` | Linear system solver to use. |
| `allocate_solution` | `1` | `int` | Whether the solution is allocated during `osqp_setup()`. |
| `verbose` | `0` | `int` | Whether solver progress is written out (quiet by default). |
| `profiler_level` | `0` | `int` | Level of detail for profiler annotations. |
| `warm_starting` | `1` | `int` | Warm-start from the previous solution between consecutive solves. |
| `scaling` | `10` | `int` | Heuristic data-scaling iterations; `0` disables scaling. |
| `polishing` | `0` | `int` | Whether the ADMM solution is polished to improve accuracy. |

### OSQP options (ADMM parameters)

| Option | Default | Type | Description |
|---|---|---|---|
| `rho` | `0.1` | `float` | ADMM penalty parameter (scalar). |
| `rho_is_vec` | `1` | `int` | Whether `rho` is a scalar or a vector. |
| `sigma` | `1e-06` | `float` | ADMM regularization parameter (improves conditioning). |
| `alpha` | `1.6` | `float` | ADMM relaxation parameter. |

### OSQP options (CG settings)

| Option | Default | Type | Description |
|---|---|---|---|
| `cg_max_iter` | `20` | `int` | Maximum number of CG iterations per solve. |
| `cg_tol_reduction` | `10` | `int` | Consecutive zero CG iterations before the tolerance is halved. |
| `cg_tol_fraction` | `0.15` | `float` | CG tolerance, as a fraction of the ADMM residuals. |
| `cg_precond` | `OSQP_DIAGONAL_PRECONDITIONER` | `PreconditionerType` | Preconditioner used by the CG method. |

### OSQP options (adaptive rho)

| Option | Default | Type | Description |
|---|---|---|---|
| `adaptive_rho` | `1` (`..._ITERATIONS`) | `int` | `rho` stepsize adaptation method (`0` disabled, `1` iterations, `2` time, `3` KKT error). |
| `adaptive_rho_interval` | `50` | `int` | Interval between `rho` adaptations (iterations-based method). |
| `adaptive_rho_fraction` | `0.4` | `float` | Fraction controlling when non-fixed `rho` adaptations occur. |
| `adaptive_rho_tolerance` | `5.0` | `float` | Min ratio between new and current `rho` for it to be adopted. |

### OSQP options (termination)

| Option | Default | Type | Description |
|---|---|---|---|
| `max_iter` | `4000` | `int` | Maximum number of ADMM iterations. |
| `eps_abs` | `1e-3` | `float` | Absolute solution tolerance. |
| `eps_rel` | `1e-3` | `float` | Relative solution tolerance. |
| `eps_prim_inf` | `1e-4` | `float` | Primal infeasibility detection tolerance. |
| `eps_dual_inf` | `1e-4` | `float` | Dual infeasibility detection tolerance. |
| `scaled_termination` | `0` | `int` | Whether the scaled termination criteria are used. |
| `check_termination` | `25` | `int` | Interval at which termination is checked; `0` disables the periodic check. |
| `check_dualgap` | `1` | `int` | Whether the duality-gap termination criteria are used. |
| `time_limit` | `1e10` | `float` | Maximum solve time, in seconds. |

### OSQP options (polishing)

| Option | Default | Type | Description |
|---|---|---|---|
| `delta` | `1e-6` | `float` | Regularization parameter used by polishing. |
| `polish_refine_iter` | `3` | `int` | Number of iterative refinement steps during polishing. |

### Enums

**Linear system solvers** (`LinsysSolverType`): `OSQP_UNKNOWN_SOLVER`,
`OSQP_DIRECT_SOLVER`, `OSQP_INDIRECT_SOLVER`.

**CG preconditioners** (`PreconditionerType`): `OSQP_NO_PRECONDITIONER`,
`OSQP_DIAGONAL_PRECONDITIONER`.

**Solver status** (`Status`): `OSQP_SOLVED`, `OSQP_SOLVED_INACCURATE`,
`OSQP_PRIMAL_INFEASIBLE`, `OSQP_PRIMAL_INFEASIBLE_INACCURATE`,
`OSQP_DUAL_INFEASIBLE`, `OSQP_DUAL_INFEASIBLE_INACCURATE`,
`OSQP_MAX_ITER_REACHED`, `OSQP_TIME_LIMIT_REACHED`, `OSQP_NON_CVX`,
`OSQP_SIGINT`, `OSQP_UNSOLVED`.

## Examples

* `marinholab/solvers/osqp/example.py` — positive-definite solves, the
  `None`-constraint path, warm-starting, a hierarchical (task-priority)
  example, and `get_info()`. Run it with `osqp_example` (installed as a
  console script).
* `marinholab/solvers/osqp/example_kinematics.py` — an *optional* example
  showing the solver used in a hierarchical (task-priority) controller for a
  kinematically redundant robot, built on
  [`dqrobotics`](https://pypi.org/project/dqrobotics/). It requires the
  optional dependencies `dqrobotics` and `dqrobotics-pyplot`
  (`pip install --pre dqrobotics dqrobotics-pyplot`).

## Building from source

The package builds a C++ extension (via CMake + pybind11) and vendors
[OSQP](https://github.com/osqp/osqp) and
[pybind11](https://github.com/pybind/pybind11) as git submodules.

```console
git clone --recurse-submodules <repo>
pip install .
```

Prerequisites: a C++23 compiler, CMake, an Eigen3 installation, and Python.
On Ubuntu: `sudo apt-get install cmake libeigen3-dev`.

## License

The wrapper is under the GNU Lesser General Public License v2.1 (see the
included `LICENSE` file); the bundled
[OSQP](https://github.com/osqp/osqp) library is Apache-2.0.