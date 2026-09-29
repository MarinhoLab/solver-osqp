"""Type stubs for the compiled `_core` pybind11 extension module.

The real module is built from ``src/core.cpp``; this file only exists so that
type checkers (e.g. Pyright) can understand the public surface of the
extension without having to parse C++.

The enum types (``LinsysSolverType``, ``PreconditionerType``, ``Status``) are
provided by the pure-Python module ``_options.py`` and re-exported by the
package ``__init__.py``; they are not part of the compiled extension.
"""

from typing import Union

from collections.abc import Mapping
from enum import Enum

import numpy as np

OptionValue = Union[bool, int, float, str]
"""An option value: bool, int, float, or an enum value name."""


class OSQP_Solver:
    """High-level, reusable solver for quadratic programs (QPs) based on OSQP."""

    class Configuration:
        """Holder of all user-configurable solver options, keyed by name.

        Every OSQP ``OSQPSettings`` field plus the wrapper-specific
        ``use_hotstart`` is exposed under its name via ``set()``/``get()``.
        Values are bool, int, float, or str: enum options take the enum value
        name (e.g. ``"OSQP_DIRECT_SOLVER"``,
        ``"OSQP_DIAGONAL_PRECONDITIONER"``) or an enum member. ``set()`` also
        converts strings such as ``"1e-9"`` or ``"false"``. Unset options use
        OSQP's own defaults for a double-precision, direct-solver build
        except ``verbose`` (``False``). See ``keys()`` and ``defaults()`` for
        the full list.
        """

        def __init__(self) -> None: ...

        def set(self, key: str, value: Union[OptionValue, Enum]) -> None:
            """Sets the option ``key`` to ``value`` (bool, int, float, str, or an
            enum member such as ``LinsysSolverType.OSQP_DIRECT_SOLVER``),
            converted to the option's kind. Raises ValueError for an unknown
            key or a value that does not convert to that kind."""
            ...

        def get(self, key: str) -> OptionValue:
            """Returns the value of ``key``, or its default if not set."""
            ...

        def has(self, key: str) -> bool:
            """True if ``key`` has been explicitly set."""
            ...

        def keys(self) -> list[str]:
            """Sorted list of all settable option names."""
            ...

        def defaults(self) -> Mapping[str, OptionValue]:
            """Mapping of option name -> default value."""
            ...

        def reset(self, key: str) -> None:
            """Reverts ``key`` to its default."""
            ...

        def reset_all(self) -> None:
            """Reverts every option to its default."""
            ...

    class Info:
        """Solution-quality values obtained from the last successful call to ``solve_quadratic_program()``."""

        #: Primal objective value.
        obj_val: float
        #: Dual objective value.
        dual_obj_val: float
        #: Norm of the primal residual.
        prim_res: float
        #: Norm of the dual residual.
        dual_res: float
        #: Dual solution, i.e. the Lagrange multiplier associated with l <= Ax <= u.
        dual_solution: np.ndarray

        def __init__(self) -> None: ...

    def __init__(self, configuration: Configuration | None = None) -> None:
        """Constructs a solver with the given configuration (defaults to the default configuration)."""
        ...

    def solve_quadratic_program(
        self,
        H: np.ndarray,
        f: np.ndarray,
        A: np.ndarray,
        b: np.ndarray,
        Aeq: np.ndarray,
        beq: np.ndarray,
        x0: np.ndarray = ...,
        y0: np.ndarray = ...,
    ) -> np.ndarray:
        """Solves ``min(x) 0.5*x'Hx + f'x s.t. Ax <= b, Aeq*x = beq``.

        Method signature is compatible with MATLAB's ``quadprog``. Returns the
        optimal ``x``. ``x0`` and ``y0`` are optional warm-starts for the primal
        and dual variables (pass an empty array to skip warm-starting).
        """
        ...

    def get_info(self) -> Info:
        """Returns the solution-quality values and dual solution from the last successful solve."""
        ...

    def test_vectorxd(self, v: np.ndarray) -> np.ndarray:
        """Round-trips a vector to help evaluate the Eigen <-> std conversions."""
        ...

    def test_matrixxd(self, m: np.ndarray) -> np.ndarray:
        """Round-trips a matrix to help evaluate the Eigen <-> std conversions."""
        ...


__version__: str
