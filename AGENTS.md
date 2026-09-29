# AGENTS.md

Repository conventions for the `marinholab-solvers-osqp` project — a Python
(C++ via pybind11) wrapper around [OSQP](https://github.com/osqp/osqp), a
first-order ADMM solver for quadratic programs.

## Project layout

```
marinholab/solvers/osqp/
  __init__.py            Public API re-exports (Solver, Configuration, Info, enums)
  solver.py              numpy-friendly Solver wrapper (accepts None constraints, warm-starts)
  _options.py            Pure-Python IntEnum types (LinsysSolverType,
                         PreconditionerType, Status) that mirror the OSQP
                         enums accepted by Configuration.set()
  example.py             runnable example (console script: osqp_example)
  example_kinematics.py  OPTIONAL example (needs dqrobotics + dqrobotics-pyplot)
  _core.pyi              type stub for the compiled _core extension (ships in the wheel)
  py.typed               PEP 561 marker so stubs are picked up by type checkers
include/marinholab/solvers/osqp.h C++ header (Solver + Configuration + Info) — OSQP-free (pimpl Solver; Configuration keyed by name with typed `OptionValue`s)
src/core.cpp             pybind11 module (_core): binds `Solver` + `Configuration` (set/get/has/keys/defaults/reset) + `Info` with native Python values
src/core_function.cpp    C++ implementation (wraps OSQP's OSQPSolver via pimpl); option name->kind map; compiled into the `marinholab_osqp` library (shared by default, `-DBUILD_SHARED_LIBS=OFF` for static)
CMakeLists.txt           CMake build: `marinholab_osqp` lib (shared by default), `_core` pybind11 module (`-DBUILD_PYTHON=ON`), optional C++ example (`-DBUILD_EXAMPLES=ON`). The vendored OSQP is always built **static** (its linear-solver, qdldl, embedded via the `qdldl` submodule) and linked **PRIVATE** into `marinholab_osqp`; consumers only link `marinholab::solvers::osqp`.
example/example.cpp      standalone C++ usage example (target: `example_osqp`)
tests/test_solver.py     pytest tests for `Solver` + `Configuration` (see "Tests")
cmake/GetVersion.cmake    rolling version (YY.MM.NN) computed at configure time
cmake/marinholab_solver_osqp-config.cmake.in  CMake package config (find_package consumers)
debian/                  Debian packaging for the C++ part (libmarinholab-solver-osqp)
tools/version.sh         compute the rolling version (YY.MM.NN)
tools/bump-changelog.sh  bring debian/changelog in line with the rolling version
docker/build-deb.sh      build the .deb (optionally + test it) in an Ubuntu noble container
docker/deb.Dockerfile    Dockerfile for the .deb build
osqp/                    OSQP (git submodule)
qdldl/                   qdldl linear-solver (git submodule, used by OSQP's built-in algebra)
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
./build/example_osqp   # prints x ≈ [0.2, 1.0] and the residuals
```

## Tests

```console
pip install pytest
cd tests && python -m pytest
```

Run pytest from inside `tests/`, not from the repository root. From the root,
the source `marinholab/` directory (which has no compiled `_core`) shadows the
installed package. CI runs the tests in the Docker job (`docker/compose.yml`)
before `osqp_example`.

`tests/test_solver.py` checks that one `Solver` instance gives the same
answers as a fresh one when the number of variables or constraints changes
between calls (with `use_hotstart` on and off), that the name-keyed
`Configuration` converts values to each option's kind (and rejects the wrong
kind), and that `Configuration.defaults()` matches OSQP's own defaults.

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
  build (see `detail::default_values` in `src/core_function.cpp`), with one
  documented exception: `verbose` defaults to `false` (quiet) instead of
  OSQP's own `OSQP_VERBOSE = 1`. Do not silently override them here; if a
  particular problem needs a non-default option, set it on the `Configuration`
  in the *caller* (e.g. `example.py` tightens `eps_abs`/`eps_rel` and raises
  `max_iter`). Re-validate the `example.py` and `example_kinematics.py` solves
  after any change to a default.
- **Option values are typed, never round-tripped through text.**
  `Configuration` stores `OptionValue = std::variant<bool, long long, double,
  std::string>`, converted to the option's kind in `set()`
  (`detail::normalize`). Defaults come straight from OSQP's `OSQPSettings`
  (`detail::default_values`), and `settings_from()` starts from OSQP's
  `osqp_set_default_settings()`, applies the quieter `verbose`, then only the
  options that were set. Enum options hold the enum value name (e.g.
  `OSQP_DIRECT_SOLVER`, `OSQP_DIAGONAL_PRECONDITIONER`).
- **`Configuration` keyed by name.** Options are set by name
  (`set(key, value)`); the C++ public header exposes no OSQP types (OSQP is
  reached only through the pimpl in `src/core_function.cpp`), so enumeration
  values are held by name. The OSQP option keys keep the library's native
  snake_case spelling (e.g. `eps_abs`, `max_iter`, `warm_starting`); the
  wrapper-specific key that has no OSQP counterpart is also snake_case
  (`use_hotstart`). Enum options take the enum value name or, in Python, an
  enum member; `set()` also converts strings for the other kinds (`"1e-9"`,
  `"false"`). The option name->kind map (`detail::option_kinds`) and the
  pure-Python enums in `_options.py` must stay in sync with the
  `Configuration` options.
- **`Solver` API.** `Solver.solve_quadratic_program()` accepts `None` for
  `A`/`b`/`Aeq`/`beq` (in the Python wrapper, which substitutes a single
  trivially-satisfied zero row; the C++ `Solver` always takes all six
  matrices) and optional `x0`/`y0` warm-starts. `Solver.get_info()` returns
  `obj_val`, `dual_obj_val`, `prim_res`, `dual_res`, and the `dual_solution`.
- **Style.** Match the existing style: docstrings on the public API.
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

## Debian build (libmarinholab-solver-osqp)

The `.deb` is **not** built by this repo's CI — it is built by the
SmartArmStack `build_ros2.sh` (SmartArmStack/smart_arm_stack_ROS2), which
clones `main` with submodules, runs `bash tools/bump-changelog.sh` and
`dpkg-buildpackage -us -uc -b` on this `debian/` directory, then `dpkg -i`
installs it. `build_ros2.sh`'s matrix is `ubuntu-24.04` (amd64) +
`ubuntu-24.04-arm` (arm64); the `debian/` packaging is shared, so it must
build on **both** architectures.

```console
bash docker/build-deb.sh            # build the .deb -> ./.deb-out/
bash docker/build-deb.sh --test     # + install it and build/run a
                                    # find_package consumer inside the container
```

Only the C++ part is packaged (the `marinholab::solvers::osqp` library). The
vendored OSQP (with its linear-solver, qdldl, embedded) is built **static**
and bundled into the same package, so the `.deb` depends only on the C++
runtime (`libc6`, `libgcc-s1`, `libstdc++6`). `debian/rules` appends `-fno-lto`
after the `dpkg-buildflags` defaults: Ubuntu noble enables `-flto=auto`, and
LTO is disabled for reproducibility.

The `.deb` carries `libmarinholab_solver_osqp.so`, the bundled
`libosqpstatic.a`, the OSQP-free public header, the CMake package config
(`find_package(marinholab_solver_osqp)`) and the vendored OSQP/qdldl headers
(which are only needed to build the archive; they are not a consumer-facing
API). The pybind11 Python extension is intentionally NOT part of this
package; it is produced by `pip install .` from the same source tree.

## Version

The Python wheel version is dynamic (`dynamic = ["version"]` in `pyproject.toml`)
via `setuptools-git-versioning`, computed from the git tag/commit. `setup.py`
computes a date+commit-derived fallback when the git tag isn't available, which
is why locally-built wheels may show a different distribution version — this
is pre-existing and unrelated to code changes.

The C++/Debian version is a rolling `YY.MM.NN` computed at configure time by
`cmake/GetVersion.cmake` (from `tools/version.sh`; it falls back to a
hardcoded version when `tools/` isn't present). `tools/bump-changelog.sh`
keeps `debian/changelog` in line with it before `dpkg-buildpackage`.
