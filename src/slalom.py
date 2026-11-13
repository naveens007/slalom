#!/usr/bin/env python3
"""SLALOM benchmark -- pure-Python port of the C reference implementation.

This follows ``src/slalom.c`` closely: the same radiosity problem, the same
seven timed phases, the same analytic FLOP counts, and the same fixed-time
bisection search for the problem size ``n``.  It is intended for comparison
and experimentation, not for replacing the C reference numbers: CPython is
far slower than optimized C, so the selected ``n`` is much smaller.

Use ``-t``, ``-l`` and ``-u`` as with the C binary, but choose bounds suited
to Python, for example::

    python3 src/slalom.py -t 10 -l 50 -u 500

The file ``geom`` is looked up next to this script, matching the C binary's
behaviour of reading ``geom`` from its own directory.
"""

import getopt
import getpass
import math
import os
import platform
import re
import socket
import sys
import time
from operator import mul

# Pure Python cannot reach the C limit in practical time.  The env var makes
# it easy to experiment without editing the source.
NMAX = int(os.environ.get("SLALOM_NMAX", "2048"))
EPS = 0.5e-8

TASKS = ("Reader", "Region", "SetUp1", "SetUp2", "SetUp3", "Solver", "Storer")


def _ddot_prefix(a, b, n):
    """Double dot product of the first ``n`` elements of two rows."""
    if n <= 0:
        return 0.0
    return sum(map(mul, a[:n], b[:n]))


def When():
    """Wall-clock time in seconds."""
    return time.perf_counter()


def geom_path():
    """``geom`` lives next to the executable, as in the C implementation."""
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), "geom")


def Reader(box, rho, emiss):
    """Read and validate the geometry file.

    The C code uses ``fscanf`` plus one ``fgets`` per group.  Here the float
    tokens are extracted from the whole file, which also tolerates the
    trailing comments used by the shipped ``geom`` file.
    """
    try:
        with open(geom_path(), "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        print("slalom:  'geom' geometry file not found.")
        return False

    number = re.compile(r"[-+]?(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)")
    values = [float(m.group()) for m in number.finditer(text)]
    if len(values) < 39:
        print("slalom:  'geom' does not contain the required 39 numbers.")
        return False

    box[0], box[1], box[2] = values[0], values[1], values[2]
    idx = 3
    for j in range(3):
        for i in range(6):
            rho[i][j] = values[idx]
            idx += 1
    for j in range(3):
        for i in range(6):
            emiss[i][j] = values[idx]
            idx += 1

    for j in range(3):
        if box[j] < 1.0 or box[j] >= 100.0:
            print("Box dimensions must be between 1 and 100.")
            return False
        box[j + 3] = box[j]

        tmp1 = 0.0
        for i in range(6):
            if rho[i][j] < 0.0 or rho[i][j] > 0.999:
                print("Reflectivities must be between .000 and .999.")
                return False
            if emiss[i][j] < 0.0:
                print("Emissivity cannot be negative.")
                return False
            if tmp1 < emiss[i][j]:
                tmp1 = emiss[i][j]
        if tmp1 == 0.0:
            print("Emissivities are zero.  Problem is trivial.")
            return False

    box[6] = box[3]
    return True


def Region(npatch, box):
    """Decompose the six faces into ``npatch`` nearly-square patches."""
    loop = [[0, 0] for _ in range(6)]
    place = [[0.0] * npatch for _ in range(3)]
    size = [[0.0] * npatch for _ in range(2)]
    area = [0.0] * npatch

    tmp1 = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0])
    tmp2 = 0.0
    tmp3 = float(npatch)
    loop[0][0] = 0
    for iface in range(5):
        tmp2 += box[iface] * box[iface + 1]
        loop[iface][1] = int(tmp3 * tmp2 / tmp1 + 0.5) - 1
        loop[iface + 1][0] = loop[iface][1] + 1
    loop[5][1] = npatch - 1

    for iface in range(6):
        numpat = loop[iface][1] - loop[iface][0] + 1
        tmp3 = box[iface - 1] if iface >= 3 else 0.0
        numcol = int(math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5)
        if numcol > numpat:
            numcol = numpat
        if numcol == 0:
            numcol = 1
        width = box[iface] / numcol
        itmp1 = numcol - 1
        tmp1 = 0.0
        for _icol in range(numcol):
            itmp2 = itmp1 // numcol
            numrow = (itmp1 + numpat) // numcol - itmp2
            if numrow == 0:
                print("Eccentric box requires more patches.")
                return None
            height = box[iface + 1] / numrow
            tmp2 = 0.0
            tmp4 = width * height * (8.0 * math.pi)
            lead = loop[iface][0] + itmp2
            last = lead + numrow
            for ipatch in range(lead, last):
                size[0][ipatch] = width
                size[1][ipatch] = height
                place[0][ipatch] = tmp1
                place[1][ipatch] = tmp2
                place[2][ipatch] = tmp3
                area[ipatch] = tmp4
                tmp2 += height
            tmp1 += width
            itmp1 += numpat

    return loop, place, size, area


def SetUp1(npatch, loop, coeff, place, size):
    """Form factors for parallel (opposite) faces."""
    sqrt = math.sqrt
    log = math.log
    atan = math.atan

    for iface in range(3):
        jface = iface + 3
        tmp1 = place[2][loop[jface][0]] ** 2
        d = [[[0.0, 0.0], [0.0, 0.0]] for _ in range(2)]
        d2 = [[[0.0, 0.0], [0.0, 0.0]] for _ in range(2)]
        for ipatch in range(loop[iface][0], loop[iface][1] + 1):
            for jpatch in range(loop[jface][0], loop[jface][1] + 1):
                for j in range(2):
                    base = place[j][jpatch] - place[j][ipatch]
                    sj = size[j][jpatch]
                    si = size[j][ipatch]
                    d[0][0][j] = base
                    d[1][0][j] = base + sj
                    d[0][1][j] = base - si
                    d[1][1][j] = base + sj - si
                    d2[0][0][j] = d[0][0][j] * d[0][0][j]
                    d2[1][0][j] = d[1][0][j] * d[1][0][j]
                    d2[0][1][j] = d[0][1][j] * d[0][1][j]
                    d2[1][1][j] = d[1][1][j] * d[1][1][j]

                tmp2 = 0.0
                for j in range(2):
                    for m in range(2):
                        for i in range(2):
                            tmp3 = d2[m][i][1 - j] + tmp1
                            tmp4 = sqrt(tmp3)
                            tmp5 = 1.0 / tmp4
                            tmp8 = 0.0
                            for k in range(2):
                                for n in range(2):
                                    tmp6 = d2[k][n][j]
                                    tmp7 = d[k][n][j]
                                    tmp2 = (tmp3 - tmp6) * log(tmp6 + tmp3) - tmp2
                                    tmp8 = -tmp7 * atan(tmp7 * tmp5) - tmp8
                                tmp2 = -tmp2
                                tmp8 = -tmp8
                            tmp2 = -4.0 * tmp4 * tmp8 - tmp2
                        tmp2 = -tmp2
                coeff[ipatch][jpatch] = tmp2
                coeff[jpatch][ipatch] = tmp2


def SetUp2(npatch, loop, coeff, place, size):
    """Form factors for orthogonal (adjacent) faces."""
    sqrt = math.sqrt
    log = math.log
    atan = math.atan

    for iface in range(6):
        for m in range(2):
            jface = (iface + m + 1) % 6
            r = [[0.0, 0.0], [0.0, 0.0]]
            r2 = [[0.0, 0.0], [0.0, 0.0]]
            s = [[0.0, 0.0], [0.0, 0.0]]
            for ipatch in range(loop[iface][0], loop[iface][1] + 1):
                for jpatch in range(loop[jface][0], loop[jface][1] + 1):
                    r[0][0] = place[m][jpatch] - place[1 - m][ipatch]
                    r[1][0] = r[0][0] + size[m][jpatch]
                    r[0][1] = r[0][0] - size[1 - m][ipatch]
                    r[1][1] = r[1][0] - size[1 - m][ipatch]
                    s[0][0] = place[m][ipatch] - place[2][jpatch]
                    s[1][0] = s[0][0] + size[m][ipatch]
                    s[0][1] = place[1 - m][jpatch] - place[2][ipatch]
                    s[1][1] = s[0][1] + size[1 - m][jpatch]

                    for i in range(2):
                        for j in range(2):
                            s[i][j] = s[i][j] * s[i][j]
                            r2[i][j] = r[i][j] * r[i][j]
                        s[i][1] += 1e-35

                    tmp2 = 0.0
                    for i in range(2):
                        for j in range(2):
                            tmp3 = s[i][0] + s[j][1]
                            tmp4 = sqrt(tmp3)
                            tmp5 = 1.0 / tmp4
                            tmp8 = 0.0
                            for k in range(2):
                                for n in range(2):
                                    tmp6 = r2[k][n]
                                    tmp7 = r[k][n]
                                    tmp2 = (tmp3 - tmp6) * log(tmp6 + tmp3) - tmp2
                                    tmp8 = -tmp7 * atan(tmp7 * tmp5) - tmp8
                                tmp2 = -tmp2
                                tmp8 = -tmp8
                            tmp2 = -4.0 * tmp4 * tmp8 - tmp2
                        tmp2 = -tmp2

                    tmp2 = abs(tmp2)
                    coeff[ipatch][jpatch] = tmp2
                    coeff[jpatch][ipatch] = tmp2


def SetUp3(npatch, loop, area, rho, emiss, coeff, diag, rhs):
    """Normalise rows and fold in reflectivities and emissivities."""
    for iface in range(6):
        vtmp1 = [1.0 / rho[iface][j] for j in range(3)]
        vtmp2 = [emiss[iface][j] * vtmp1[j] for j in range(3)]

        for ipatch in range(loop[iface][0], loop[iface][1] + 1):
            row = coeff[ipatch]
            total = 0.0
            for jpatch in range(loop[iface][0]):
                total += row[jpatch]
            for jpatch in range(loop[iface][1] + 1, npatch):
                total += row[jpatch]

            if abs(total - area[ipatch]) > (0.5e-9 * total):
                print("Total form factor is too far from unity.")
                return False

            total = -total
            for jpatch in range(loop[iface][0], loop[iface][1] + 1):
                row[jpatch] = 0.0
            for j in range(3):
                diag[j][ipatch] = vtmp1[j] * total
                rhs[j][ipatch] = vtmp2[j] * total

    return True


def Solver(npatch, non0, coeff, diag, rhs, result):
    """LDL^T factorisation and backsolve for the three colour channels."""
    for m in range(3):
        for i in range(non0, npatch):
            coeff[i][i] = diag[m][i]
            result[m][i] = rhs[m][i]
            ci = coeff[i]
            for j in range(i):
                ci[j] = coeff[j][i]

        for j in range(non0):
            coeff[j][j] = 1.0 / diag[m][j]
            result[m][j] = rhs[m][j]

        for j in range(non0, npatch):
            cj = coeff[j]
            for k in range(non0, j):
                cj[k] -= _ddot_prefix(coeff[k], cj, k)
            for k in range(j):
                tmp1 = cj[k]
                cj[k] = tmp1 * coeff[k][k]
                cj[j] -= tmp1 * cj[k]
            cj[j] = 1.0 / cj[j]

        rm = result[m]
        for k in range(non0, npatch):
            rm[k] -= _ddot_prefix(rm, coeff[k], k)

        for k in range(npatch):
            rm[k] *= coeff[k][k]

        for k in range(npatch - 2, non0 - 1, -1):
            n = npatch - (k + 1)
            total = 0.0
            for t in range(n):
                total += rm[k + 1 + t] * coeff[k + 1 + t][k]
            rm[k] -= total

        for k in range(non0 - 1, -1, -1):
            n = npatch - non0
            total = 0.0
            for t in range(n):
                total += rm[non0 + t] * coeff[non0 + t][k]
            rm[k] -= total


def Storer(npatch, loop, result, place, size):
    """Write patch geometry and radiosities to ``answer``."""
    try:
        outfile = open("answer", "w", encoding="utf-8")
    except OSError:
        print("Unable to open 'answer' file.")
        return False

    with outfile:
        outfile.write("%d patches:\n" % npatch)
        outfile.write(
            " Patch  Face       Position in w, h, d              Width     Height\n"
        )
        for iface in range(6):
            for ipatch in range(loop[iface][0], loop[iface][1] + 1):
                outfile.write(
                    "%5d   %4d%11.5f%11.5f%11.5f  %11.5f%11.5f\n"
                    % (
                        ipatch + 1,
                        iface + 1,
                        place[0][ipatch],
                        place[1][ipatch],
                        place[2][ipatch],
                        size[0][ipatch],
                        size[1][ipatch],
                    )
                )

        outfile.write("\n Patch  Face  Radiosities\n")
        for iface in range(6):
            for ipatch in range(loop[iface][0], loop[iface][1] + 1):
                outfile.write(
                    "%5d   %4d%12.8f%12.8f%12.8f\n"
                    % (
                        ipatch + 1,
                        iface + 1,
                        result[0][ipatch],
                        result[1][ipatch],
                        result[2][ipatch],
                    )
                )
    return True


def Verify(npatch, coeff, diag, rhs, result):
    """Check the residual after timing has stopped."""
    total = 0.0
    anorm = 0.0
    xnorm = 0.0

    for m in range(3):
        for i in range(npatch):
            coeff[i][i] = diag[m][i]
            ci = coeff[i]
            for j in range(i):
                ci[j] = coeff[j][i]

        anorm = 0.0
        xnorm = 0.0
        for j in range(npatch):
            tmp2 = rhs[m][j]
            row = coeff[j]
            for i in range(npatch):
                tmp2 -= row[i] * result[m][i]
                anorm = max(anorm, abs(row[i]))
            xnorm = max(xnorm, abs(result[m][j]))
            total += abs(tmp2)

    denom = anorm * xnorm
    residual = total / denom if denom != 0.0 else float("inf")
    if not math.isfinite(residual) or residual > 3.0 * EPS:
        print("Residual is too large: %g" % residual)
        return False
    return True


def Meter(npatch):
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

    box = [0.0] * 7
    rho = [[0.0] * 3 for _ in range(6)]
    emiss = [[0.0] * 3 for _ in range(6)]
    area = [0.0] * npatch
    coeff = [[0.0] * npatch for _ in range(npatch)]
    diag = [[0.0] * npatch for _ in range(3)]
    place = [[0.0] * npatch for _ in range(3)]
    result = [[0.0] * npatch for _ in range(3)]
    rhs = [[0.0] * npatch for _ in range(3)]
    size = [[0.0] * npatch for _ in range(2)]

    sec = [0.0] * 8
    sec[0] = When()
    if not Reader(box, rho, emiss):
        return False, 0.0, 0.0
    sec[1] = When()

    region = Region(npatch, box)
    if region is None:
        return False, 0.0, 0.0
    loop, place, size, area = region
    sec[2] = When()

    SetUp1(npatch, loop, coeff, place, size)
    sec[3] = When()
    SetUp2(npatch, loop, coeff, place, size)
    sec[4] = When()
    if not SetUp3(npatch, loop, area, rho, emiss, coeff, diag, rhs):
        return False, 0.0, 0.0
    sec[5] = When()

    non0 = loop[1][0]
    Solver(npatch, non0, coeff, diag, rhs, result)
    sec[6] = When()

    if not Storer(npatch, loop, result, place, size):
        return False, 0.0, 0.0
    sec[7] = When()

    timing = sec[7] - sec[0]
    secs = [sec[i + 1] - sec[i] for i in range(7)]

    p = [loop[i][1] - loop[i][0] + 1 for i in range(6)]
    itmp1 = 0
    tmp1 = 0.0
    for i in range(6):
        tmp1 += p[i] * p[i]
        itmp1 += int(math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5)
    tmp2 = p[0] * p[3] + p[1] * p[4] + p[2] * p[5]

    ops = [0.0] * 7
    ops[0] = 258.0
    ops[1] = 154.0 + 8.0 * itmp1 + npatch
    ops[2] = 3.0 + 840.0 * tmp2
    ops[3] = 430.0 * (npatch * npatch - tmp1 - 2.0 * tmp2) / 2.0
    ops[4] = 72.0 + 9.0 * npatch + npatch * npatch - tmp1
    ops[5] = (
        npatch * (npatch * (npatch + 7.5) - 2.5)
        - 21.0
        + (non0 + 1) * ((non0 + 1) * (2.0 * (non0 + 1) - 16.5) + 35.5)
        + (non0 + 1) * npatch * (9.0 - 3.0 * (non0 + 1))
    )
    ops[6] = 48.0 * npatch
    work = sum(ops)

    print("%d patches:" % npatch)
    print(" Task  Seconds       Operations        MFLOPS    % of Time")
    fmt = "%6.6s%8.3f%17.0f%14.6f%10.1f %%"
    for i in range(7):
        if secs[i] == 0.0:
            secs[i] = 0.001
        mflops = (ops[i] / secs[i]) * 1e-6
        pct = 100.0 * secs[i] / timing
        print(fmt % (TASKS[i], secs[i], ops[i], mflops, pct))
    print(fmt % ("TOTALS", timing, work, (work / timing) * 1e-6, 100.0))

    Verify(npatch, coeff, diag, rhs, result)
    return True, timing, work


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
