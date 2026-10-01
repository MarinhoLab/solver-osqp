#!/usr/bin/env bash
#
# Builds the OSQP optimization benchmark (bench/osqp_bench.c) with a chosen set
# of optimization flags. The same OSQP + qdldl C sources that the project links
# against are compiled here, so only the compiler optimization flags vary
# between variants.
#
# Usage:
#   ./bench/build.sh <variant>
#
# Variants:
#   o0              -O0 -g                     (debug, reference floor)
#   o1              -O1
#   o2              -O2
#   o3              -O3   (current default: CMake "Release")
#   ofast           -Ofast
#   o3_native       -O3 -march=native
#   o3_native_lto   -O3 -march=native -flto
#   ofast_native_lto -Ofast -march=native -flto
#
# The binary is written to bench/bin/<variant>.
#
# All sources are C, so they are compiled with the same include layout and
# type configuration (double precision, 64-bit indexing, builtin algebra) that
# the project's CMake build produces. The generated headers
# (osqp_configure.h / qdldl_types.h / qdldl_version.h) are emitted under
# bench/gen so the build needs no CMake.
#
# -DNDEBUG is kept constant across variants so the only thing that changes is
# the optimization level itself (debug asserts are otherwise disabled by
# -O2+ in most toolchains, and we want a clean isolation).

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
OSQP="$REPO/osqp"
QDLDL="$REPO/qdldl"
BENCH="$REPO/bench"
GEN="$BENCH/gen"
BIN="$BENCH/bin"

CC="${CC:-cc}"
ARCH_FLAG=""
if [ "$(uname -m)" = "arm64" ]; then
    ARCH_FLAG="-arch arm64"
fi

VARIANT="${1:-}"
if [ -z "$VARIANT" ]; then
    echo "usage: $0 <variant>" >&2
    echo "variants: o0 o1 o2 o3 ofast o3_native o3_native_lto ofast_native_lto" >&2
    exit 2
fi
shift || true

case "$VARIANT" in
    o0)               OPTFLAGS="-O0 -g" ;;
    o1)               OPTFLAGS="-O1" ;;
    o2)               OPTFLAGS="-O2" ;;
    o3)               OPTFLAGS="-O3" ;;
    ofast)            OPTFLAGS="-Ofast" ;;
    o3_native)        OPTFLAGS="-O3 -march=native" ;;
    o3_native_lto)    OPTFLAGS="-O3 -march=native -flto" ;;
    ofast_native_lto) OPTFLAGS="-Ofast -march=native -flto" ;;
    *) echo "unknown variant: $VARIANT" >&2; exit 2 ;;
esac
OPTFLAGS="$OPTFLAGS -DNDEBUG"

mkdir -p "$BIN" "$GEN/osqp/include/public" "$GEN/qdldl/include"

# ---- Generate the configuration headers the real build produces ----------

cat > "$GEN/osqp/include/public/osqp_configure.h" <<'EOF'
#ifndef OSQP_CONFIGURE_H
# define OSQP_CONFIGURE_H
/* OSQP_ENABLE_DEBUG */
/* #undef OSQP_ENABLE_DEBUG */
/* #undef IS_LINUX */
#define IS_MAC
/* #undef IS_WINDOWS */
#define OSQP_ALGEBRA_BUILTIN
/* #undef OSQP_ALGEBRA_MKL */
/* #undef OSQP_ALGEBRA_CUDA */
/* #undef OSQP_CODEGEN */
/* #undef OSQP_PROFILER_ANNOTATIONS */
#define OSQP_ENABLE_DERIVATIVES
/* #undef OSQP_EMBEDDED_MODE */
/* #undef OSQP_CUSTOM_MEMORY */
#define OSQP_ENABLE_PRINTING
/* #undef OSQP_CUSTOM_PRINTING */
#define OSQP_ENABLE_PROFILING
#define OSQP_ENABLE_INTERRUPT
/* #undef OSQP_USE_FLOAT */
#define OSQP_USE_LONG
/* #undef OSQP_PACK_SETTINGS */
#endif /* ifndef OSQP_CONFIGURE_H */
EOF

cat > "$GEN/qdldl/include/qdldl_types.h" <<'EOF'
#ifndef QDLDL_TYPES_H
#define QDLDL_TYPES_H
#include <limits.h>
typedef long long     QDLDL_int;
typedef double        QDLDL_float;
typedef unsigned char QDLDL_bool;
#define QDLDL_INT_MAX LLONG_MAX
/* #undef QDLDL_FLOAT */
#define QDLDL_LONG
#endif /* ifndef QDLDL_TYPES_H */
EOF

cat > "$GEN/qdldl/include/qdldl_version.h" <<'EOF'
#ifndef QDLDL_VERSION_H_
#define QDLDL_VERSION_H_
#define QDLDL_VERSION_MAJOR 0
#define QDLDL_VERSION_MINOR 1
#define QDLDL_VERSION_PATCH 8
#endif
EOF

# ---- Source list (identical to the project's static OSQP + qdldl build) ----
SRC=(
    "$BENCH/osqp_bench.c"
    "$OSQP/src/osqp_api.c"
    "$OSQP/src/auxil.c"
    "$OSQP/src/derivative.c"
    "$OSQP/src/error.c"
    "$OSQP/src/interrupt_unix.c"
    "$OSQP/src/polish.c"
    "$OSQP/src/scaling.c"
    "$OSQP/src/timing_macos.c"
    "$OSQP/src/util.c"
    "$OSQP/algebra/_common/csc_math.c"
    "$OSQP/algebra/_common/csc_utils.c"
    "$OSQP/algebra/_common/kkt.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/qdldl_interface.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_1.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_2.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_aat.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_control.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_defaults.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_info.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_order.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_post_tree.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_postorder.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_preprocess.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/amd_valid.c"
    "$OSQP/algebra/_common/lin_sys/qdldl/amd/src/SuiteSparse_config.c"
    "$OSQP/algebra/builtin/algebra_libs.c"
    "$OSQP/algebra/builtin/matrix.c"
    "$OSQP/algebra/builtin/vector.c"
    "$QDLDL/src/qdldl.c"
)

INC=(
    -I"$GEN/osqp/include/public"
    -I"$OSQP/include/public"
    -I"$OSQP/include/private"
    -I"$OSQP/algebra/_common"
    -I"$OSQP/algebra/builtin"
    -I"$OSQP/algebra/_common/lin_sys/qdldl"
    -I"$OSQP/algebra/_common/lin_sys/qdldl/amd/include"
    -I"$QDLDL/include"
    -I"$GEN/qdldl/include"
)

OUT="$BIN/$VARIANT"
echo "Building $VARIANT with: $OPTFLAGS"
# shellcheck disable=SC2086
"$CC" $OPTFLAGS $ARCH_FLAG -fPIC -std=c99 "${INC[@]}" -o "$OUT" "${SRC[@]}" -lm
echo "Built $OUT"
