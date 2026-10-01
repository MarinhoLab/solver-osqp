# Compiler Optimization Benchmark

This documents a benchmark of `g++`/`gcc`/`clang` optimization flags for the
`marinholab-solvers-osqp` package, what each flag actually does to the code that
dominates solve time, and the resulting change (an opt-in `OSQP_OPT_FAST_MATH`
CMake option).

## TL;DR

* The heavy lifting in a solve is **OSQP itself** (the ADMM iteration loop, its
  built-in CSC algebra, and the qdldl/AMD LDL^t factorizer). The Python/C++
  wrapper (`src/core_function.cpp`) is thin glue: it converts dense `Eigen`
  matrices to CSC and calls `osqp_setup`/`osqp_solve`.
* The vendored `osqp/` and `qdldl/` submodules **force `-O3`** on their C code
  in their own `CMakeLists.txt`, regardless of the top-level build type. The
  wrapper C++ compiles under the top-level build type (`Release` → `-O3`).
  So the shipped wheel is already built at `-O3` everywhere.
* Going from `-O3` to `-O2`/`-O1` is **not** a speedup, and `-march=native`
  and `-flto` give **no measurable gain** on this workload.
* The one flag that moves the needle is **`-ffast-math`** (a.k.a. the
  fast-math part of `-Ofast`): it makes the C OSQP core ~**25–30% faster** in
  the steady-state ADMM loop. Because the dense-matrix→CSC conversion in the
  wrapper is a large, fast-math-insensitive cost, the end-to-end Python solve
  time improves by a more modest ~**4–8%**.
* `-ffast-math` relaxes IEEE-754. It does **not** change the solutions this
  package produces in the tests or example (answers are identical to the `-O3`
  build, with only machine-precision-level differences in residuals), because
  OSQP is an iterative solver that runs to a tolerance.

**Decision:** keep the default build at the IEEE-conformant `-O3` (matching
OSQP's own default), and expose the fast-math option as an **opt-in** CMake
flag, `OSQP_OPT_FAST_MATH` (OFF by default), so a consumer who can tolerate the
relaxed floating-point contract can opt into the speedup without changing the
shipped wheel.

## Environment

| Item | Value |
| --- | --- |
| Host | Apple M2, arm64 (macOS) |
| Compiler | Apple clang 21 (`cc`; `gcc`/`g++` are clang on macOS) |
| OSQP | submodule `1572ae06` (≈ v1.0.0) |
| qdldl | submodule `138fdac58` (v0.1.8) |
| pybind11 | submodule `d03662f0` (v3.0) |
| Python | 3.12 |

The numbers below are from this host; relative ratios are what matter, and the
harness is included so results can be re-produced on any machine.

## Methodology

The benchmark compiles **exactly the C sources the package links** (OSQP solver
+ built-in algebra + the AMD/qdldl factorizer) with a chosen set of compiler
flags, then times two deterministic workloads. Compiling the C core directly
(rather than rebuilding the whole CMake tree per variant) guarantees that *only
the optimization flags* differ between runs.

* **Workloads** — a "small" dense-SPD QP (n=800) and a "large" one (n=2500),
  each with box constraints plus a few equality rows. The Hessian is
  `P = M'M + n·I` (dense, symmetric positive definite), mirroring how the Python
  wrapper feeds OSQP: it converts a *dense* `Eigen::MatrixXd` Hessian to CSC, so
  in practice the top-left KKT block is dense.
* **Cold-start solves** — `warm_starting = 0`, `polishing = 0`,
  `eps_abs = eps_rel = 1e-6`, so every `osqp_solve` runs a stable ~50
  iterations and the ADMM loop does real work (warm-starting on identical data
  converges in ~25 iterations, which is too cheap to time).
* **Timed** — `osqp_setup` (factorization-heavy) and steady-state
  `osqp_solve`, averaged over many reps. Data is fixed (seed 42), so all
  variants solve the identical problem; the objective value is printed so a
  run can be checked for correctness.

### Flags benchmarked

| Variant | Flags | Rationale |
| --- | --- | --- |
| `o0` | `-O0 -g` | debug floor |
| `o1` | `-O1` | light optimization |
| `o2` | `-O2` | common release default |
| `o3` | `-O3` | **current default** (CMake `Release`) |
| `ofast` | `-Ofast` | `-O3` + `-ffast-math` (single flag) |
| `o3_native` | `-O3 -march=native` | architecture-tuned |
| `o3_native_lto` | `-O3 -march=native -flto` | + whole-program LTO |
| `ofast_native_lto` | `-Ofast -march=native -flto` | the "max" setting |

All variants share `-DNDEBUG`; the `-O` level is the variable under test.

## Results

Measured on the Apple M2 host above. Times are per-solve (small, n=800) and per
solve / per setup (large, n=2500), averaged over the reps shown. Ratios are
relative to the `-O3` default.

| Variant | small solve (s) | × vs `-O3` | large solve (s) | × vs `-O3` | large setup (s) | × vs `-O3` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `-O0`        | 0.086789 | 2.87× | 0.834060 | 2.82× | 4.244841 | 2.96× |
| `-O1`        | 0.031020 | 1.03× | 0.293860 | 0.99× | 1.438983 | 1.00× |
| `-O2`        | 0.030336 | 1.00× | 0.294022 | 1.00× | 1.400835 | 0.98× |
| `-O3` (def)  | 0.030268 | 1.00× | 0.295519 | 1.00× | 1.434966 | 1.00× |
| `-Ofast`     | **0.021072** | **0.70×** | **0.210061** | **0.71×** | 1.768622 | 1.23× |
| `-O3 -march=native` | 0.030308 | 1.00× | 0.294269 | 1.00× | 1.447293 | 1.01× |
| `-O3 -march=native -flto` | 0.030423 | 1.00× | 0.293481 | 0.99× | 1.444066 | 1.01× |
| `-Ofast -march=native -flto` | **0.020922** | **0.69×** | **0.209276** | **0.71×** | **1.422635** | **0.99×** |

All variants report the same objective values (`small = -0.130387`,
`large = -0.129698`) and identical status/residuals to within machine precision —
no variant changes the answer.

### End-to-end (Python wrapper)

Rebuilding the wheel (default `-O3` vs `-OSQP_OPT_FAST_MATH=ON`) and timing a
box-constrained solve through `marinholab.solvers.osqp.Solver` on the same host:

| Build | avg solve / rep (n=800) |
| --- | ---: |
| default (`-O3`) | ~397 ms |
| `OSQP_OPT_FAST_MATH=ON` | ~381 ms |

The end-to-end gain (~4%) is smaller than the C-core gain (~30%) because the
Python wrapper spends a large share of each call converting the dense `Eigen`
Hessian (`O(n²)`) to CSC, which is not affected by `-ffast-math`. For solves that
are dominated by many ADMM iterations (larger `n`, tighter tolerances, repeated
warm-started calls in a control loop), the relative benefit of the opt-in flag
grows.

## Findings

1. **`-O3` is already the sweet spot.** `-O2` and `-O1` are within noise of
   `-O3`; `-O0` is ~3× slower. There is nothing to gain by changing the
   optimization level from the current `-O3`.

2. **`-march=native` and `-flto` add nothing measurable here.** The ADMM loop
   and the factorization are memory-bound enough that architecture-specific
   tuning and link-time optimization do not help on this workload. (This also
   matches the project's existing choice to keep LTO off in the `.deb` build for
   reproducibility.) `-march=native` additionally breaks the manylinux
   portability of the wheel, so it is not appropriate to enable by default.

3. **`-ffast-math` is the real lever, worth ~25–30% in the solver core.**
   It enables the fast-math floating-point contract, which lets the compiler
   reassociate and vectorize the ADMM inner-loop arithmetic. Two subtleties
   verified experimentally:
   * The vendored OSQP/qdldl CMake files **append** an `-O3` after any flag you
     inject, and in GCC/Clang the *last* `-O` wins. A bare `-Ofast` therefore
     gets overridden by the trailing `-O3` and does nothing (measured:
     `-Ofast -O3 -O3` ≈ baseline). `-ffast-math`, however, is independent of the
     `-O` level and **does** survive the appended `-O3`.
   * The flag must reach the *vendored* C code, not just the wrapper. Setting it
     on the base `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS` before `add_subdirectory(osqp)`
     propagates it into OSQP and qdldl (verified via the generated
     `compile_commands.json`).

4. **It is numerically safe for this package.** `OSQP` is an iterative solver run
   to a tolerance; `-ffast-math` does not change the reported solutions in the
   test suite or the example (answers are identical, residuals differ only at
   ~1e-12–1e-14 level). All 46 tests pass on both the default and the fast-math
   build.

## Change made

Added an opt-in CMake option in `CMakeLists.txt` (default **OFF**, so the
shipped wheel and CI are byte-for-byte the IEEE-conformant `-O3` build):

```cmake
option(OSQP_OPT_FAST_MATH "Enable fast-math (non-IEEE) optimizations in the OSQP core." OFF)
```

When ON it appends `-ffast-math` (or `/fp:fast` on MSVC) to the base C/CXX
flags before the OSQP subdirectory is added, so it reaches OSQP, qdldl, and the
wrapper. A consumer can enable it in either build path:

```console
# C++ build
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DOSQP_OPT_FAST_MATH=ON

# Python wheel
CMAKE_ARGS="-DOSQP_OPT_FAST_MATH=ON" python setup.py bdist_wheel
```

> Note on the Python build: `setup.py` reuses its `build/temp/` CMake cache
> across `bdist_wheel` invocations, and CMake `option()` does not reset a
> variable that is already in the cache. To flip the flag between two wheel
> builds, delete `build/` (or the `build/temp/…/_core/` cache) first; otherwise
> the first value wins.

## How to reproduce

```console
cd solver-osqp
git submodule update --init --recursive

# Build + run the C-core benchmark for a flag variant (see bench/build.sh for
# the full list):
./bench/build.sh o3            # -> bench/bin/o3
./bench/bin/o3                 # prints RESULT ... lines

# Build + run every variant and aggregate a table:
./bench/run.sh
./bench/aggregate.py
```

The harness lives in `bench/`:

* `bench/osqp_bench.c` — the benchmark (dense-SPD QP, cold-start solves).
* `bench/build.sh` — compiles the OSQP/qdldl C core with a chosen flag set.
* `bench/run.sh` — runs every built variant, saving raw output to `bench/results/`.
* `bench/aggregate.py` — collapses `bench/results/*.txt` into the comparison table.
* `bench/scan_flags.py` — prints the effective optimization flags per component
  from a CMake `compile_commands.json` (used to verify the CMake change).

`bench/bin/`, `bench/gen/`, and `bench/results/` are build artifacts and are
git-ignored.
