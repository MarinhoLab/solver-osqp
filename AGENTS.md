# AGENTS.md

Repository conventions for the `marinholab-solvers-osqp` project — a Python
(C++ via pybind11) wrapper around [OSQP](https://github.com/osqp/osqp), a
first-order ADMM solver for quadratic programs.

## Project layout

```
marinholab/solvers/osqp/
  __init__.py            Public API re-exports (Solver, Configuration, Info, enums)
  solver.py              numpy-friendly Solver wrapper (accepts None constraints, warm-starts)
  example.py             runnable example (console script: osqp_example)
  example_kinematics.py  OPTIONAL example (needs dqrobotics + dqrobotics-pyplot)
  _core.pyi              type stub for the compiled _core extension (ships in the wheel)
  py.typed               PEP 561 marker so stubs are picked up by type checkers
include/marinholab/solvers/osqp.h C++ header (Solver + Configuration + Info): `marinholab::solvers::osqp::Solver` + `Configuration` (doxygen-documented)
src/core.cpp             pybind11 module (_core): binds `Solver` + `Configuration` + `Info` + enums
src/core_function.cpp    C++ implementation (wraps OSQP's OSQPSolver); compiled into the `marinholab_osqp` static library
CMakeLists.txt           CMake build: `marinholab_osqp` static lib, `_core` pybind11 module, optional C++ example (`-DBUILD_EXAMPLES=ON`)
example/example.cpp      standalone C++ usage example (target: `example_osqp`)
osqp/                    OSQP (git submodule)
pybind11/                pybind11 (git submodule)
setup.py                 PEP 517 build (CMake + pybind11)
pyrightconfig.json       Pyright configuration (python 3.9, mode basic)
```

## Build & install

Requires: a C++23 compiler (e.g. `g++`), CMake, Ninja, `eigen3` (dev headers),
and Python >= 3.9 with `numpy` + `setuptools`/`wheel`. On Ubuntu:
`sudo apt-get install cmake libeigen3-dev`.

```console
git submodule update --init --recursive   # if cloning without submodules
pip install -e .                          # editable install (builds _core in build_ext)
# or produce a wheel:
python setup.py bdist_wheel
pip install dist/marinholab_solvers_osqp-*.whl
```

The extension name is `marinholab.solvers.osqp._core`. Builds are slow on first
run (OSQP is compiled from its submodule); CMake reuses the `build/` cache
across builds.

## Run the example (smoke test)

```console
osqp_example
```

(or `python -m marinholab.solvers.osqp.example`). Exits 0 and prints the
optimal `x`, the `None`-constraint paths, warm-starts, a hierarchical
(task-priority) example, and `get_info()` for the final solve.

`example_kinematics.py` additionally needs the *optional* dependencies
`dqrobotics` and `dqrobotics-pyplot` (`pip install --pre dqrobotics
dqrobotics-pyplot`); it is not required for the core package to work.

The standalone C++ example (`example/example.cpp`, target `example_osqp`) is
built only when `BUILD_EXAMPLES=ON`; it is *not* built by `pip install .`:

```console
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
cmake --build build --target example_osqp
./build/example/example_osqp   # prints x ≈ [0.2, 1.0] and the residuals
```

## Type checking (Pyright)

```console
pyright
```

Configuration lives in `pyrightconfig.json` (`pythonVersion` 3.9,
`typeCheckingMode` basic). It must pass with **0 errors / 0 warnings**.

- The compiled extension `_core` is typed through the `_core.pyi` stub.
  Keep the stub in sync with the pybind11 surface in `src/core.cpp`.
- `example_kinematics.py` depends on the untyped, *optional* third-party
  `dqrobotics` package; its `import` lines carry targeted `# type: ignore`
  comments (`reportMissingImports` for `matplotlib`, `reportAttributeAccessIssue`
  for the `dqrobotics`/`dqrobotics_extensions` imports). Do not remove them
  (they are the documented reason those lines are ignored, and keep the module
  checking cleanly even when the optional deps are not installed).
- The package ships `py.typed` + `_core.pyi` in the wheel (`package_data` in
  `setup.py`) so downstream projects can be checked against it.

## Conventions

- **Defaults match OSQP.** `Configuration` defaults mirror OSQP's own
  `osqp_set_default_settings()` for a standard double-precision, direct-solver
  build (see the defaults tables in `README.md`), with one documented
  exception: `verbose` defaults to `0` (quiet) instead of OSQP's own
  `OSQP_VERBOSE = 1`. Do not silently override them here; if a particular
  problem needs a non-default option, set it on the `Configuration` in the
  *caller* (e.g. `example.py` tightens `eps_abs`/`eps_rel` and raises
  `max_iter`). Re-validate the `example.py` and `example_kinematics.py` solves
  after any change to a default.
- **`Solver` API.** `Solver.solve_quadratic_program()` accepts `None` for
  `A`/`b`/`Aeq`/`beq` (in the Python wrapper, which substitutes a single
  trivially-satisfied zero row; the C++ `Solver` always takes all six
  matrices) and optional `x0`/`y0` warm-starts. `Solver.get_info()` returns
  `obj_val`, `dual_obj_val`, `prim_res`, `dual_res`, and the `dual_solution`.
- **Style.** Match the existing style: docstrings on the public API.
  `Configuration` fields that map to OSQP `OSQPSettings` fields keep the
  library's native snake_case spelling (e.g. `eps_abs`, `max_iter`,
  `warm_starting`); the wrapper-specific field that has no OSQP counterpart is
  also snake_case (`use_hotstart`).
- **Doxygen.** C++ types and members are documented with Doxygen
  (`/** ... @brief ... @param ... @return ... @see ... */` blocks). Keep that
  when adding fields or methods.
- **Annotations.** All public Python API is fully type-annotated and must
  remain Pyright-clean. `requires-python` is `>= 3.9`; use
  `from __future__ import annotations` where PEP 604 `X | Y` is used so the
  module still parses on 3.9.

## CI

`.github/workflows/python-publish.yml` builds the wheel on `ubuntu-latest` and
`ubuntu-24.04-arm` for Python 3.12 and 3.13 and publishes to PyPI. Local
builds here mirror that (aarch64, Python 3.13).

## Version

The version is dynamic (`dynamic = ["version"]` in `pyproject.toml`) via
`setuptools-git-versioning`, computed from the git tag/commit. `setup.py`
computes a date+commit-derived fallback when the git tag isn't available, which
is why locally-built wheels may show a different distribution version — this
is pre-existing and unrelated to code changes.
