#!/usr/bin/env python3
"""SLALOM benchmark -- pure-Python port of the C reference implementation.

This follows ``src/slalom.c``: the same radiosity problem, the same seven
timed phases, the same analytic FLOP counts, and the same fixed-time
bisection search for the problem size ``n``.  It is intended for comparison
and experimentation, not for replacing the C reference numbers: CPython is
far slower than optimized C, so the selected ``n`` is much smaller.

The port is organised around the problem domain instead of the C arrays:

* ``Patch`` / ``Face`` / ``Geometry`` dataclasses describe the box and its
  surface decomposition, and ``Geometry`` is iterable patch by patch.
* ``RadiositySystem`` owns the linear system: form-factor assembly
  (parallel and orthogonal faces), row normalisation, the LDL^T solve for
  each colour channel, and the residual check.
* Colour channels are dictionaries keyed by ``"R"``/``"G"``/``"B"`` rather
  than parallel arrays indexed 0..2.
* Per-phase timings are ``PhaseTiming`` objects, and the report is printed
  by iterating over them.

Use ``-t``, ``-l`` and ``-u`` as with the C binary, but choose bounds suited
to Python, for example::

    python3 src/slalom.py -t 10 -l 50 -u 500

The file ``geom`` is looked up next to this script, matching the C binary's
behaviour of reading ``geom`` from its own directory.
"""

import getpass
import getopt
import math
import os
import platform
import re
import socket
import sys
import time
from dataclasses import dataclass, field
from itertools import accumulate, islice
from typing import Dict, Iterator, List, Optional, Tuple

# Pure Python cannot reach the C limit in practical time.  The env var makes
# it easy to experiment without editing the source.
NMAX = int(os.environ.get("SLALOM_NMAX", "2048"))
EPS = 0.5e-8

# The three colour channels, in the order the benchmark reports them.
RGB: Tuple[str, str, str] = ("R", "G", "B")

TASKS = ("Reader", "Region", "SetUp1", "SetUp2", "SetUp3", "Solver", "Storer")


def when() -> float:
    """Wall-clock time in seconds."""
    return time.perf_counter()


def geom_path() -> str:
    """``geom`` lives next to the executable, as in the C implementation."""
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), "geom")


# ---------------------------------------------------------------------------
# Problem description (the data Reader() reads and validates)
# ---------------------------------------------------------------------------


class GeometryError(Exception):
    """Raised with the exact message the C reference prints on bad input."""


@dataclass
class Problem:
    """The parsed ``geom`` file: box dimensions and per-face properties."""

    box: List[float]                     # x, y, z, then mirrored, then x again
    reflectivities: List[Dict[str, float]]  # per face: RGB -> rho
    emissivities: List[Dict[str, float]]    # per face: RGB -> emissivity


def read_problem(path: str) -> Problem:
    """Read and validate the geometry file.

    The C code uses ``fscanf`` plus one ``fgets`` per group.  Here the float
    tokens are extracted from the whole file, which also tolerates the
    trailing comments used by the shipped ``geom`` file.
    """
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as infile:
            text = infile.read()
    except OSError:
        raise GeometryError("slalom:  'geom' geometry file not found.")

    number = re.compile(r"[-+]?(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)")
    values = [float(m.group()) for m in number.finditer(text)]
    if len(values) < 39:
        raise GeometryError("slalom:  'geom' does not contain the required 39 numbers.")

    box = [0.0] * 7
    box[0:3] = values[0:3]

    # Colour-major, exactly as the C reference reads them: one colour
    # channel after another, six faces per channel.  Slicing by colour keeps
    # the triplets together instead of walking a shared cursor.
    rho_values = [
        [values[3 + 6 * channel + face] for channel in range(3)]
        for face in range(6)
    ]
    emiss_values = [
        [values[21 + 6 * channel + face] for channel in range(3)]
        for face in range(6)
    ]

    # Sanity-check the values that were just read.
    for j in range(3):
        if box[j] < 1.0 or box[j] >= 100.0:
            raise GeometryError("Box dimensions must be between 1 and 100.")
        box[j + 3] = box[j]

        max_emiss = 0.0
        for rho, emiss in zip(
            (rho_values[face][j] for face in range(6)),
            (emiss_values[face][j] for face in range(6)),
        ):
            if rho < 0.000 or rho > 0.999:
                raise GeometryError("Reflectivities must be between .000 and .999.")
            if emiss < 0.0:
                raise GeometryError("Emissivity cannot be negative.")
            max_emiss = max(max_emiss, emiss)
        if max_emiss == 0.0:
            raise GeometryError("Emissivities are zero.  Problem is trivial.")

    box[6] = box[3]
    return Problem(
        box=box,
        reflectivities=[
            {c: rho_values[i][j] for j, c in enumerate(RGB)} for i in range(6)
        ],
        emissivities=[
            {c: emiss_values[i][j] for j, c in enumerate(RGB)} for i in range(6)
        ],
    )


# ---------------------------------------------------------------------------
# Surface decomposition (the data Region() builds)
# ---------------------------------------------------------------------------


@dataclass
class Patch:
    """One surface element of the box."""

    index: int                       # global patch number, 0-based
    place: Tuple[float, float, float]  # position in width, height, depth
    width: float
    height: float
    area: float                      # 8 * pi * area

    def extent(self, axis: int) -> float:
        """Extent along axis 0 (width) or 1 (height)."""
        return self.width if axis == 0 else self.height


@dataclass
class Face:
    """One face of the box and the patches it was subdivided into."""

    index: int
    first_patch: int                  # inclusive global patch index
    last_patch: int                   # inclusive global patch index
    reflectivity: Dict[str, float]    # RGB -> rho
    emissivity: Dict[str, float]      # RGB -> emissivity
    patches: List[Patch] = field(default_factory=list)

    @property
    def patch_count(self) -> int:
        return self.last_patch - self.first_patch + 1

    def __iter__(self) -> Iterator[Patch]:
        return iter(self.patches)


@dataclass
class Geometry:
    """The box decomposed into nearly-square patches."""

    box: List[float]
    faces: List[Face]

    @property
    def patch_count(self) -> int:
        return self.faces[-1].last_patch + 1

    def __iter__(self) -> Iterator[Patch]:
        """Iterate over every patch, in global patch order."""
        for face in self.faces:
            yield from face

    @property
    def first_non_coplanar(self) -> int:
        """Index of the first patch not on face 0 (the solver's ``non0``)."""
        return self.faces[1].first_patch


def decompose(npatch: int, problem: Problem) -> Optional[Geometry]:
    """Decompose the six faces into ``npatch`` nearly-square patches."""
    box = problem.box

    # Allocate patches to each face, proportionate to area of each face.
    total_area = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0])
    budget = float(npatch)
    covered = list(accumulate(box[iface] * box[iface + 1] for iface in range(5)))
    last_patch = [int(budget * area / total_area + 0.5) - 1 for area in covered]
    last_patch.append(npatch - 1)
    first_patch = [0] + [last + 1 for last in last_patch[:-1]]

    faces = [
        Face(
            index=iface,
            first_patch=first_patch[iface],
            last_patch=last_patch[iface],
            reflectivity=problem.reflectivities[iface],
            emissivity=problem.emissivities[iface],
        )
        for iface in range(6)
    ]

    # Subdivide each face into numpat patches.
    for iface in range(6):
        face = faces[iface]
        numpat = face.patch_count
        depth = box[iface - 1] if iface >= 3 else 0.0
        numcol = min(
            numpat,
            max(1, int(math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5)),
        )
        width = box[iface] / numcol
        row_cursor = numcol - 1
        offset_w = 0.0
        for _ in range(numcol):
            row_offset = row_cursor // numcol
            numrow = (row_cursor + numpat) // numcol - row_offset
            if numrow == 0:
                print("Eccentric box requires more patches.")
                return None
            height = box[iface + 1] / numrow
            offset_h = 0.0
            patch_area = width * height * (8.0 * math.pi)
            lead = face.first_patch + row_offset
            for ipatch in range(lead, lead + numrow):
                face.patches.append(
                    Patch(
                        index=ipatch,
                        place=(offset_w, offset_h, depth),
                        width=width,
                        height=height,
                        area=patch_area,
                    )
                )
                offset_h += height
            offset_w += width
            row_cursor += numpat

    return Geometry(box=box, faces=faces)


# ---------------------------------------------------------------------------
# The linear system (SetUp1/SetUp2/SetUp3/Solver/Verify in the C code)
# ---------------------------------------------------------------------------


def _ddot(a: List[float], b: List[float], n: int) -> float:
    """Dot product of the first ``n`` elements of two rows."""
    return sum(left * right for left, right in islice(zip(a, b), n))


def _coupling(
    distance2: float,
    squares: Tuple[Tuple[float, float], Tuple[float, float]],
    lengths: Tuple[Tuple[float, float], Tuple[float, float]],
    accumulated: float,
) -> float:
    """One 2x2 evaluation of the analytic form-factor integral.

    ``distance2`` is the squared separation along the coupling axis,
    ``squares`` the squared corner offsets and ``lengths`` the corresponding
    signed offsets.  ``accumulated`` is the running term carried through the
    caller's nested loops; the arithmetic order matches the C reference.
    """
    root = math.sqrt(distance2)
    inverse_root = 1.0 / root
    log_term = accumulated
    angle_term = 0.0
    for square_row, length_row in zip(squares, lengths):
        for square, length in zip(square_row, length_row):
            log_term = (distance2 - square) * math.log(square + distance2) - log_term
            angle_term = -length * math.atan(length * inverse_root) - angle_term
        log_term = -log_term
        angle_term = -angle_term
    return -4.0 * root * angle_term - log_term


def _parallel_corners(
    ipatch: Patch, jpatch: Patch
) -> Tuple[
    Tuple[Tuple[float, float], Tuple[float, float]],
    Tuple[Tuple[float, float], Tuple[float, float]],
]:
    """Corner offsets between two parallel patches, one 2x2 grid per axis."""
    grids = []
    for axis in range(2):
        delta = jpatch.place[axis] - ipatch.place[axis]
        own = ipatch.extent(axis)
        other = jpatch.extent(axis)
        grids.append(
            (
                (delta, delta - own),
                (delta + other, delta + other - own),
            )
        )
    return grids[0], grids[1]


def _orthogonal_corners(
    ipatch: Patch, jpatch: Patch, axis: int
) -> Tuple[
    Tuple[Tuple[float, float], Tuple[float, float]],
    Tuple[Tuple[float, float], Tuple[float, float]],
    Tuple[Tuple[float, float], Tuple[float, float]],
]:
    """Corner offsets for two orthogonal patches: ``r``, ``r**2`` and the
    squared ``s`` offsets, including the 1e-35 singularity bump."""
    r00 = jpatch.place[axis] - ipatch.place[1 - axis]
    ipatch_other = ipatch.extent(1 - axis)
    r = (
        (r00, r00 - ipatch_other),
        (r00 + jpatch.extent(axis), r00 + jpatch.extent(axis) - ipatch_other),
    )
    r2 = tuple(tuple(value * value for value in row) for row in r)

    s00 = ipatch.place[axis] - jpatch.place[2]
    s01 = jpatch.place[1 - axis] - ipatch.place[2]
    s = (
        (s00, s01),
        (s00 + ipatch.extent(axis), s01 + jpatch.extent(1 - axis)),
    )
    s2 = tuple(
        (row[0] * row[0], row[1] * row[1] + 1e-35)
        for row in s
    )
    return r, r2, s2


class RadiositySystem:
    """The radiosity linear system for one problem size.

    The matrix is stored as rows (``coeff[i]`` is patch *i*'s equation), the
    per-channel diagonal terms, right-hand sides and solutions are
    dictionaries keyed by colour.
    """

    def __init__(self, geometry: Geometry) -> None:
        n = geometry.patch_count
        self.geometry = geometry
        self.coeff: List[List[float]] = [[0.0] * n for _ in range(n)]
        self.diag: Dict[str, List[float]] = {c: [0.0] * n for c in RGB}
        self.rhs: Dict[str, List[float]] = {c: [0.0] * n for c in RGB}
        self.result: Dict[str, List[float]] = {c: [0.0] * n for c in RGB}

    # -- SetUp1: form factors for parallel (opposite) faces ----------------

    def add_parallel_form_factors(self) -> None:
        for face, opposite in zip(self.geometry.faces[:3], self.geometry.faces[3:]):
            depth = opposite.patches[0].place[2]
            depth2 = depth * depth
            for ipatch in face:
                for jpatch in opposite:
                    d = _parallel_corners(ipatch, jpatch)
                    d2 = tuple(
                        tuple(tuple(value * value for value in row) for row in grid)
                        for grid in d
                    )
                    total = 0.0
                    for j in (0, 1):
                        for m in (0, 1):
                            for i in (0, 1):
                                total = _coupling(
                                    d2[1 - j][m][i] + depth2, d2[j], d[j], total
                                )
                            total = -total
                    self.coeff[ipatch.index][jpatch.index] = total
                    self.coeff[jpatch.index][ipatch.index] = total

    # -- SetUp2: form factors for orthogonal (adjacent) faces --------------

    def add_orthogonal_form_factors(self) -> None:
        for iface, face in enumerate(self.geometry.faces):
            for m in (0, 1):
                adjacent = self.geometry.faces[(iface + m + 1) % 6]
                for ipatch in face:
                    for jpatch in adjacent:
                        # size[m] is the width direction and size[1 - m] the
                        # height direction, exactly as in the C reference.
                        r, r2, s = _orthogonal_corners(ipatch, jpatch, m)
                        total = 0.0
                        for i in (0, 1):
                            for j in (0, 1):
                                total = _coupling(s[i][0] + s[j][1], r2, r, total)
                            total = -total
                        total = abs(total)
                        self.coeff[ipatch.index][jpatch.index] = total
                        self.coeff[jpatch.index][ipatch.index] = total

    # -- SetUp3: normalise rows and fold in rho/emissivities ---------------

    def normalise(self) -> bool:
        """Make every row sum to the patch area and set diag/rhs. False on
        a form-factor check failure."""
        for face in self.geometry.faces:
            inverse_rho = {c: 1.0 / face.reflectivity[c] for c in RGB}
            scaled_emiss = {c: face.emissivity[c] * inverse_rho[c] for c in RGB}

            for patch in face:
                row = self.coeff[patch.index]
                total = sum(islice(row, 0, face.first_patch))
                total += sum(islice(row, face.last_patch + 1, None))

                # Make sure the row sum (total form factor) is close to 1:
                if abs(total - patch.area) > (0.5e-9 * total):
                    print("Total form factor is too far from unity.")
                    return False

                total = -total
                # Set coplanar patch interactions to zero.
                row[face.first_patch : face.last_patch + 1] = [0.0] * face.patch_count
                # Assign diagonal entries and right-hand sides.
                for colour in RGB:
                    self.diag[colour][patch.index] = inverse_rho[colour] * total
                    self.rhs[colour][patch.index] = scaled_emiss[colour] * total
        return True

    # -- Solver: LDL^T factorisation and backsolve, per colour channel -----

    def solve(self) -> None:
        """Factor and backsolve the system; no pivoting is needed because
        the matrix is diagonally dominant."""
        non0 = self.geometry.first_non_coplanar
        npatch = self.geometry.patch_count
        coeff = self.coeff

        for colour in RGB:
            diag = self.diag[colour]
            rhs = self.rhs[colour]
            solution = self.result[colour]

            # Load the lower triangle of coefficients, diagonal, and
            # solution vector.
            for i in range(non0, npatch):
                coeff[i][i] = diag[i]
                solution[i] = rhs[i]
                for j in range(i):
                    coeff[i][j] = coeff[j][i]

            for j in range(non0):
                coeff[j][j] = 1.0 / diag[j]
                solution[j] = rhs[j]

            # Factor the matrix, writing factors over the original matrix.
            for j in range(non0, npatch):
                for k in range(non0, j):
                    coeff[j][k] -= _ddot(coeff[k], coeff[j], k)
                for k in range(j):
                    tmp1 = coeff[j][k]
                    coeff[j][k] = tmp1 * coeff[k][k]
                    coeff[j][j] -= tmp1 * coeff[j][k]
                coeff[j][j] = 1.0 / coeff[j][j]

            # Backsolve, in three stages (for L, D, and L transpose).
            for k in range(non0, npatch):
                solution[k] -= _ddot(solution, coeff[k], k)

            for k in range(npatch):
                solution[k] *= coeff[k][k]

            # Column access across rows (the C code uses stride NMAX):
            for k in range(npatch - 2, non0 - 1, -1):
                solution[k] -= sum(
                    solution[j] * coeff[j][k] for j in range(k + 1, npatch)
                )

            for k in range(non0 - 1, -1, -1):
                solution[k] -= sum(
                    solution[j] * coeff[j][k] for j in range(non0, npatch)
                )

    # -- Verify: residual check, run after the clock has stopped -----------

    def verify(self) -> bool:
        total = 0.0
        for colour in RGB:
            diag = self.diag[colour]
            solution = self.result[colour]
            for i in range(self.geometry.patch_count):
                self.coeff[i][i] = diag[i]
                for j in range(i):
                    self.coeff[i][j] = self.coeff[j][i]

            # The norms are per channel (only the last channel's survive
            # into the residual denominator); the residual total itself
            # accumulates across all three channels.
            anorm = 0.0
            xnorm = 0.0
            for j in range(self.geometry.patch_count):
                tmp2 = self.rhs[colour][j]
                row = self.coeff[j]
                for i in range(self.geometry.patch_count):
                    tmp2 -= row[i] * solution[i]
                    anorm = max(anorm, abs(row[i]))
                xnorm = max(xnorm, abs(solution[j]))
                total += abs(tmp2)

        denom = anorm * xnorm
        residual = total / denom if denom != 0.0 else float("inf")
        if not math.isfinite(residual) or residual > 3.0 * EPS:
            print("Residual is too large: %g" % residual)
            return False
        return True


# ---------------------------------------------------------------------------
# Output (Storer in the C code)
# ---------------------------------------------------------------------------


def store(geometry: Geometry, result: Dict[str, List[float]]) -> bool:
    """Write patch geometry and radiosities to ``answer``."""
    try:
        with open("answer", "w", encoding="utf-8") as outfile:
            outfile.write("%d patches:\n" % geometry.patch_count)
            outfile.write(
                " Patch  Face       Position in w, h, d              Width     Height\n"
            )
            for face in geometry.faces:
                for patch in face:
                    outfile.write(
                        "%5d   %4d%11.5f%11.5f%11.5f  %11.5f%11.5f\n"
                        % (
                            patch.index + 1,
                            face.index + 1,
                            patch.place[0],
                            patch.place[1],
                            patch.place[2],
                            patch.width,
                            patch.height,
                        )
                    )

            outfile.write("\n Patch  Face  Radiosities\n")
            for face in geometry.faces:
                for patch in face:
                    outfile.write(
                        "%5d   %4d%12.8f%12.8f%12.8f\n"
                        % (
                            patch.index + 1,
                            face.index + 1,
                            result["R"][patch.index],
                            result["G"][patch.index],
                            result["B"][patch.index],
                        )
                    )
    except OSError:
        print("Unable to open 'answer' file.")
        return False
    return True


# ---------------------------------------------------------------------------
# Meter: time one complete run of the benchmark at size npatch
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class PhaseTiming:
    """One timed phase of the benchmark."""

    name: str
    seconds: float
    operations: float

    @property
    def mflops(self) -> float:
        return (self.operations / self.seconds) * 1e-6


def _operation_counts(npatch: int, geometry: Geometry, non0: int) -> List[float]:
    """The analytic LLNL-weighted operation count of each phase.

    The counts follow src/slalom.h's weights and must not be changed.
    """
    box = geometry.box
    p = [face.patch_count for face in geometry.faces]
    tmp1 = 0.0
    columns = 0
    for i in range(6):
        tmp1 += p[i] * p[i]
        columns += int(math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5)
    opposite = p[0] * p[3] + p[1] * p[4] + p[2] * p[5]

    ops = [0.0] * 7
    ops[0] = 258.0
    ops[1] = 154.0 + 8.0 * columns + npatch
    ops[2] = 3.0 + 840.0 * opposite
    ops[3] = 430.0 * (npatch * npatch - tmp1 - 2.0 * opposite) / 2.0
    ops[4] = 72.0 + 9.0 * npatch + npatch * npatch - tmp1
    ops[5] = (
        npatch * (npatch * (npatch + 7.5) - 2.5)
        - 21.0
        + (non0 + 1) * ((non0 + 1) * (2.0 * (non0 + 1) - 16.5) + 35.5)
        + (non0 + 1) * npatch * (9.0 - 3.0 * (non0 + 1))
    )
    ops[6] = 48.0 * npatch
    return ops


def Meter(npatch: int) -> Tuple[bool, float, float]:
    """Time one complete solve at problem size ``npatch``.

    Returns ``(ok, timing, work)``.
    """
    if npatch < 6:
        print("Must be at least 6, the number of faces.")
        return False, 0.0, 0.0
    if npatch > NMAX:
        print("Exceeds %d = maximum for this system." % NMAX)
        return False, 0.0, 0.0

    try:
        os.unlink("answer")
    except OSError:
        pass

    stamps = [when()]
    try:
        problem = read_problem(geom_path())
    except GeometryError as error:
        print(error)
        return False, 0.0, 0.0
    stamps.append(when())

    geometry = decompose(npatch, problem)
    if geometry is None:
        return False, 0.0, 0.0
    stamps.append(when())

    system = RadiositySystem(geometry)
    system.add_parallel_form_factors()
    stamps.append(when())
    system.add_orthogonal_form_factors()
    stamps.append(when())
    if not system.normalise():
        return False, 0.0, 0.0
    stamps.append(when())

    system.solve()
    stamps.append(when())

    if not store(geometry, system.result):
        return False, 0.0, 0.0
    stamps.append(when())

    timing = stamps[-1] - stamps[0]
    seconds = [later - earlier for earlier, later in zip(stamps, stamps[1:])]
    operations = _operation_counts(npatch, geometry, geometry.first_non_coplanar)
    work = sum(operations)

    print_report(npatch, timing, seconds, operations, work)
    system.verify()
    return True, timing, work


def print_report(
    npatch: int, timing: float, seconds: List[float], operations: List[float], work: float
) -> None:
    """Display the timing-work-speed breakdown by routine."""
    phases = [
        PhaseTiming(name=task, seconds=sec or 0.001, operations=ops)
        for task, sec, ops in zip(TASKS, seconds, operations)
    ]
    phases.append(PhaseTiming(name="TOTALS", seconds=timing, operations=work))

    print("%d patches:" % npatch)
    print(" Task  Seconds       Operations        MFLOPS    % of Time")
    fmt = "%6.6s%8.3f%17.0f%14.6f%10.1f %%"
    for phase in phases:
        print(
            fmt
            % (
                phase.name,
                phase.seconds,
                phase.operations,
                phase.mflops,
                100.0 * phase.seconds / timing,
            )
        )


# ---------------------------------------------------------------------------
# Machine information
# ---------------------------------------------------------------------------


def _read_first_field(path, key):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith(key):
                    parts = line.split(":", 1)
                    if len(parts) == 2:
                        value = parts[1].strip()
                        return value or None
    except OSError:
        pass
    return None


def _read_text_file(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read().strip("\x00\r\n ")
    except OSError:
        return None


def _sysconf(name):
    try:
        return os.sysconf(name)
    except (ValueError, OSError):
        return None


def _detect_processor():
    proc = _read_text_file("/sys/firmware/devicetree/base/model")
    if not proc:
        proc = _read_first_field("/proc/cpuinfo", "model name")
    if not proc:
        proc = _read_first_field("/proc/cpuinfo", "Hardware")
    if not proc:
        proc = _read_first_field("/proc/cpuinfo", "cpu model")
    if not proc:
        proc = platform.processor() or platform.machine()

    clock = 0.0
    for path in (
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
    ):
        value = _read_text_file(path)
        if value:
            try:
                clock = float(value) / 1.0e6
                break
            except ValueError:
                pass
    if clock <= 0.0:
        mhz = _read_first_field("/proc/cpuinfo", "cpu MHz")
        if mhz:
            try:
                clock = float(mhz) / 1.0e3
            except ValueError:
                pass

    return proc, clock


def _cache_kib(value):
    if not value:
        return 0
    value = value.strip()
    multiplier = 1
    if value.endswith("K"):
        value = value[:-1]
    elif value.endswith("M"):
        value = value[:-1]
        multiplier = 1024
    try:
        return int(float(value) * multiplier)
    except ValueError:
        return 0


def _detect_caches():
    """L1 instruction and L2 cache sizes in KiB, from sysfs when possible."""
    base = "/sys/devices/system/cpu/cpu0/cache"
    l1i = 0
    l2 = 0
    try:
        entries = os.listdir(base)
    except OSError:
        entries = []
    for entry in entries:
        path = os.path.join(base, entry)
        level = _read_text_file(os.path.join(path, "level"))
        kind = _read_text_file(os.path.join(path, "type"))
        size = _cache_kib(_read_text_file(os.path.join(path, "size")))
        if level == "1" and kind and kind.lower().startswith("instruction"):
            l1i = size
        elif level == "2":
            l2 = size
    if not l1i:
        l1i = (_sysconf("SC_LEVEL1_ICACHE_SIZE") or 0) // 1024
    if not l2:
        l2 = (_sysconf("SC_LEVEL2_CACHE_SIZE") or 0) // 1024
    return l1i, l2


def _detect_cores():
    """Physical core count when /proc/cpuinfo exposes it, else logical CPUs."""
    text = _read_text_file("/proc/cpuinfo") or ""
    pairs = set()
    for block in text.split("\n\n"):
        fields = {}
        for line in block.splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                fields[key.strip()] = value.strip()
        if "physical id" in fields and "core id" in fields:
            pairs.add((fields["physical id"], fields["core id"]))
    return len(pairs) if pairs else (os.cpu_count() or 1)


def _memory_gib():
    pages = _sysconf("SC_PHYS_PAGES")
    page_size = _sysconf("SC_PAGE_SIZE")
    if pages and page_size:
        return pages * page_size / (1024.0 ** 3)
    return 0.0


def _free_memory_gib():
    pages = _sysconf("SC_AVPHYS_PAGES")
    page_size = _sysconf("SC_PAGE_SIZE")
    if pages and page_size:
        return pages * page_size / (1024.0 ** 3)
    return 0.0


def system_info():
    proc, clock = _detect_processor()
    l1i, l2 = _detect_caches()
    total = _memory_gib()
    free = _free_memory_gib()
    mem_usage = ((total - free) / total * 100.0) if total > 0.0 else 0.0
    cores = _detect_cores()
    arch = platform.machine() or "unknown"
    user = getpass.getuser()
    node = socket.gethostname()
    osname = platform.system() or sys.platform

    print("Machine:%s\t\t Processor:  %s" % (arch, proc))
    print("Memory: %3.2f%% \t\t # of procs: %d" % (mem_usage, cores))
    print("L1 iCache:%d KB\t\t # used:1" % l1i)
    if clock > 0.0:
        print("NMAX:%5d \t\t Clock:%5.2fGHz" % (NMAX, clock))
    else:
        print("NMAX:%5d \t\t Clock:   N/A" % NMAX)
    print("OS:%s\t\t Timer:Wall, time.perf_counter()" % osname)
    print("L2 Cache:%d KB\t\t Node: %s" % (l2, node))
    print("Language:Python\t\t Version:%s" % platform.python_version())
    print("Compiler:%s\t\t Run by:%s" % (platform.python_implementation(), user))
    print(
        "Options:pure Python\t Date:%s"
        % time.strftime("%d/%m/%Y (dd/mm) %H:%M:%S")
    )


def What(n, timing, work):
    print()
    system_info()
    print()
    print("M ops:    %-13g        Time:       %.3f seconds" % (work * 1e-6, timing))
    print("n:        %-6d               MFLOPS:     %.5g" % (n, (work / timing) * 1e-6))
    print("Approximate data memory use: %d bytes." % (8 * n * n + 120 * n + 800))


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def errmsg(msg):
    print("Error: %s" % msg, file=sys.stderr)
    return False


def help_message():
    print("Usage:(All paramters below are required)", file=sys.stderr)
    print("\tslalom.py -t <seconds> -l <integer> -u <integer>", file=sys.stderr)
    print(
        "\tslalom.py --time <seconds> --lower-bound integer --upper-bound integer",
        file=sys.stderr,
    )
    return False


def command_line(argv):
    if len(argv) == 1:
        return help_message()

    try:
        opts, args = getopt.getopt(
            argv[1:],
            "t:l:u:h",
            ["time=", "lower-bound=", "upper-bound=", "help"],
        )
    except getopt.GetoptError as exc:
        return errmsg(str(exc))

    goal = 0.0
    l_bound = 0
    u_bound = 0
    try:
        for opt, arg in opts:
            if opt in ("-t", "--time"):
                goal = float(arg)
            elif opt in ("-l", "--lower-bound"):
                l_bound = int(arg)
            elif opt in ("-u", "--upper-bound"):
                u_bound = int(arg)
            elif opt in ("-h", "--help"):
                return help_message()
    except ValueError:
        return errmsg("Invalid or incomplete parameters:\n\ttime, lower-bound, upper-bound")

    if args:
        return errmsg("Extra or missed paramters.\n\tCheck -t, -l or -u values.")
    if goal <= 0 or l_bound <= 0 or u_bound <= l_bound:
        return errmsg(
            "Invalid or incomplete parameters:\n\ttime, lower-bound, upper-bound"
        )
    return goal, l_bound, u_bound


def set_lower_bound(goal, lower_bound):
    ok, timing, _work = Meter(lower_bound)
    if not ok:
        return False
    if timing >= goal:
        print(
            "Must take less than %g seconds. Took %g." % (goal, timing),
            file=sys.stderr,
        )
        return False
    return True


def set_upper_bound(goal, upper_bound):
    ok, timing, _work = Meter(upper_bound)
    if not ok:
        return False
    if timing < goal:
        print(
            "Must take atleast %g seconds. Took %g." % (goal, timing),
            file=sys.stderr,
        )
        return False
    return True


def slalom_main_loop(goal, lower_bound, upper_bound):
    while upper_bound - lower_bound > 1:
        mean = (lower_bound + upper_bound) // 2
        ok, timing, _work = Meter(mean)
        if not ok:
            return False
        if timing < goal:
            lower_bound = mean
        else:
            upper_bound = mean
        print("New interval: [%d,%d]" % (lower_bound, upper_bound))

    ok, timing, work = Meter(lower_bound)
    if not ok:
        return False
    What(lower_bound, timing, work)
    return True


def main(argv):
    params = command_line(argv)
    if not params:
        return 1
    goal, lower_bound, upper_bound = params
    if not set_lower_bound(goal, lower_bound):
        return 1
    if not set_upper_bound(goal, upper_bound):
        return 1
    return 0 if slalom_main_loop(goal, lower_bound, upper_bound) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
