/*
 *                               S L A L O M
 *
 *     Scalable Language-independent Ames Laboratory One-minute Measurement
 *
 * TypeScript port of the C reference implementation in ../slalom.c.  It
 * follows the reference closely: the same radiosity problem, the same seven
 * timed phases (Reader -> Region -> SetUp1/2/3 -> Solver -> Storer), the same
 * analytic FLOP counts, the same fixed-time bisection search for the problem
 * size n, and the same console and 'answer' file output formats.
 *
 * Command line (identical to the C driver):
 *
 *     node Slalom.js -t <seconds> -l <integer> -u <integer>
 *     node Slalom.js --time <seconds> --lower-bound integer --upper-bound integer
 *
 * The 'geom' file is looked up next to the compiled JavaScript file (with a
 * fallback to the parent directory, so an in-tree build finds ../src/geom).
 * The 'answer' file is written to the current working directory, matching the
 * C convention.
 *
 * John Gustafson, Diane Rover, Michael Carter, and Stephen Elbert
 * Ames Laboratory, 3/18/90 (reference C implementation)
 */

import * as fs from "fs";
import * as os from "os";
import * as path from "path";

/* NMAX = Largest npatch for this computer; overridable for experiments. */
const NMAX: number = parseEnvInt("SLALOM_NMAX", 8192);
const EPS: number = 0.5e-8;

const TASKS: string[] = [
    "Reader", "Region", "SetUp1", "SetUp2", "SetUp3", "Solver", "Storer"];

function parseEnvInt(name: string, fallback: number): number {
    const raw = process.env[name];
    if (raw === undefined || raw.length === 0) {
        return fallback;
    }
    const parsed = parseInt(raw, 10);
    return Number.isNaN(parsed) ? fallback : parsed;
}

/* --------------------------------------------------------------------- */
/*  C-compatible printf formatting.                                      */
/*                                                                       */
/*  JavaScript has no printf, and Number.prototype.toFixed keeps         */
/*  trailing zeros while C's %g strips them, so the conversions used by   */
/*  the benchmark output are re-implemented here to print identical       */
/*  text to the C reference.                                             */
/* --------------------------------------------------------------------- */

/** Formats like C's "%.{precision}g" (default precision 6). */
export function g(v: number, precision = 6): string {
    if (v === 0.0) {
        return "0";
    }
    if (Number.isNaN(v)) {
        return "NaN";
    }
    if (!Number.isFinite(v)) {
        return v > 0.0 ? "Inf" : "-Inf";
    }
    const p = precision <= 0 ? 1 : precision;

    /* Round to p significant digits (C rounds to nearest). */
    const sci = v.toExponential(p - 1);
    const expVal = parseInt(sci.substring(sci.indexOf("e") + 1), 10);

    if (expVal < -4 || expVal >= p) {
        /* Scientific notation: strip trailing zeros from the mantissa,
         * exponent with at least two digits. */
        let mant = sci.substring(0, sci.indexOf("e"));
        if (mant.indexOf(".") >= 0) {
            mant = mant.replace(/0+$/, "");
            if (mant.endsWith(".")) {
                mant = mant.substring(0, mant.length - 1);
            }
        }
        let ePart = sci.substring(sci.indexOf("e") + 1);
        const negativeExponent = ePart.startsWith("-");
        ePart = ePart.replace(/^[+-]/, "");
        while (ePart.length < 2) {
            ePart = "0" + ePart;
        }
        return mant + "e" + (negativeExponent ? "-" : "+") + ePart;
    }

    /* Decimal notation: p significant digits, trailing zeros gone. */
    let fixed = v.toPrecision(p);
    if (fixed.indexOf(".") >= 0) {
        fixed = fixed.replace(/0+$/, "");
        if (fixed.endsWith(".")) {
            fixed = fixed.substring(0, fixed.length - 1);
        }
    }
    return fixed;
}

/** Formats like C's "%<width>.<decimals>f" (right-aligned). */
export function f(v: number, width: number, decimals: number): string {
    return v.toFixed(decimals).padStart(width);
}

/** Formats like C's "%<width>d" (right-aligned); padToWidth<0 left-aligns. */
export function d(v: number, width: number): string {
    return String(v).padStart(width);
}

/* --------------------------------------------------------------------- */
/*  Wall clock in seconds (like gettimeofday in the C reference).         */
/* --------------------------------------------------------------------- */

function when(): number {
    const t = process.hrtime.bigint();
    /* Split so the conversion to double stays exact for long uptimes. */
    const MILLION = BigInt(1000000);
    return Number(t / MILLION) * 1e-3 + Number(t % MILLION) * 1e-9;
}

/* --------------------------------------------------------------------- */
/*  Reader: reads the problem description from the 'geom' file.           */
/* --------------------------------------------------------------------- */

function geomPath(): string {
    try {
        /* Directory of the compiled JavaScript file (ts-node: the source
         * file). Like the C binary, 'geom' lives next to the executable;
         * one level up is also accepted so an in-tree build finds
         * ../src/geom. */
        for (const candidate of [path.join(__dirname, "geom"),
                                 path.join(__dirname, "..", "geom")]) {
            if (fs.existsSync(candidate)) {
                return candidate;
            }
        }
    } catch (e) {
        /* Fall through to the current working directory. */
    }
    return "geom";
}

/** Returns false on failure; box/rho/emiss are filled on success. */
function reader(box: number[], rho: number[][], emiss: number[][]): boolean {
    let text: string;
    try {
        text = fs.readFileSync(geomPath(), "utf8");
    } catch (e) {
        console.log("slalom:  'geom' geometry file not found.");
        return false;
    }

    /* The C code reads with fscanf() and discards the remainder of the box
     * line and of the third reflectivity/emissivity line.  Here the numeric
     * tokens are extracted from the whole file, which also tolerates the
     * trailing comments used by the shipped geom file. */
    const number = /[-+]?(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)/g;
    const values: number[] = [];
    let match: RegExpExecArray | null;
    while (values.length < 39 && (match = number.exec(text)) !== null) {
        values.push(parseFloat(match[0]));
    }
    if (values.length < 39) {
        console.log("slalom:  'geom' does not contain the required 39 numbers.");
        return false;
    }

    box[0] = values[0];
    box[1] = values[1];
    box[2] = values[2];
    let idx = 3;
    for (let j = 0; j < 3; j++) {
        for (let i = 0; i < 6; i++) {
            rho[i][j] = values[idx++];
        }
    }
    for (let j = 0; j < 3; j++) {
        for (let i = 0; i < 6; i++) {
            emiss[i][j] = values[idx++];
        }
    }

    /* Sanity-check the values that were just read. */
    for (let j = 0; j < 3; j++) {
        if (box[j] < 1.0 || box[j] >= 100.0) {
            console.log("Box dimensions must be between 1 and 100.");
            return false;
        }
        box[j + 3] = box[j];

        let maxEmiss = 0.0;
        for (let i = 0; i < 6; i++) {
            if (rho[i][j] < 0.000 || rho[i][j] > 0.999) {
                console.log("Reflectivities must be between .000 and .999.");
                return false;
            }
            if (emiss[i][j] < 0.0) {
                console.log("Emissivity cannot be negative.");
                return false;
            }
            if (maxEmiss < emiss[i][j]) {
                maxEmiss = emiss[i][j];
            }
        }
        if (maxEmiss === 0.0) {
            console.log("Emissivities are zero.  Problem is trivial.");
            return false;
        }
    }
    box[6] = box[3];
    return true;
}

/* --------------------------------------------------------------------- */
/*  Region: decomposes the surface of the box into patches that are       */
/*  as nearly equal in size and square as possible.                       */
/* --------------------------------------------------------------------- */

/** Result of Region(); null means the decomposition failed. */
interface RegionResult {
    loop: number[][];
    place: number[][];
    size: number[][];
    area: number[];
}

function region(npatch: number, box: number[]): RegionResult | null {
    const loop: number[][] = [];
    for (let i = 0; i < 6; i++) {
        loop.push([0, 0]);
    }
    const place: number[][] = [
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0)];
    const size: number[][] = [
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0)];
    const area: number[] = new Array(npatch).fill(0.0);

    /* Allocate patches to each face, proportionate to area of each face. */
    let tmp1 = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
    let tmp2 = 0.0;
    let tmp3 = npatch;
    loop[0][0] = 0;
    for (let iface = 0; iface < 5; iface++) {
        tmp2 = tmp2 + box[iface] * box[iface + 1];
        loop[iface][1] = Math.trunc(tmp3 * tmp2 / tmp1 + 0.5) - 1;
        loop[iface + 1][0] = loop[iface][1] + 1;
    }
    loop[5][1] = npatch - 1;

    /* Subdivide each face into numpat patches. */
    for (let iface = 0; iface < 6; iface++) {
        const numpat = loop[iface][1] - loop[iface][0] + 1;
        tmp3 = 0.0;
        if (iface >= 3) {
            tmp3 = box[iface - 1];
        }
        const numcol = Math.min(numpat,
                Math.max(1, Math.trunc(
                    Math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5)));
        const width = box[iface] / numcol;
        let itmp1 = numcol - 1;
        tmp1 = 0.0;
        for (let icol = 0; icol < numcol; icol++) {
            const itmp2 = Math.trunc(itmp1 / numcol);
            const numrow = Math.trunc((itmp1 + numpat) / numcol) - itmp2;
            if (numrow === 0) {
                console.log("Eccentric box requires more patches.");
                return null;
            }
            const height = box[iface + 1] / numrow;
            tmp2 = 0.0;
            const tmp4 = width * height * (8.0 * Math.PI);
            const lead = loop[iface][0] + itmp2;
            const last = lead + numrow;
            for (let ipatch = lead; ipatch < last; ipatch++) {
                size[0][ipatch] = width;
                size[1][ipatch] = height;
                place[0][ipatch] = tmp1;
                place[1][ipatch] = tmp2;
                place[2][ipatch] = tmp3;
                area[ipatch] = tmp4;
                tmp2 = tmp2 + height;
            }
            tmp1 = tmp1 + width;
            itmp1 = itmp1 + numpat;
        }
    }
    return { loop, place, size, area };
}

/* --------------------------------------------------------------------- */
/*  SetUp1: form factors for parallel patches.                            */
/* --------------------------------------------------------------------- */

function setUp1(npatch: number, loop: number[][], coeff: number[][],
        place: number[][], size: number[][]): void {
    const d: number[][][] = [[[0, 0], [0, 0]], [[0, 0], [0, 0]]];
    const d2: number[][][] = [[[0, 0], [0, 0]], [[0, 0], [0, 0]]];

    for (let iface = 0; iface < 3; iface++) {
        const jface = iface + 3;
        const tmp1 = place[2][loop[jface][0]] * place[2][loop[jface][0]];
        for (let ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
            for (let jpatch = loop[jface][0]; jpatch <= loop[jface][1]; jpatch++) {
                for (let j = 0; j < 2; j++) {
                    d[0][0][j] = place[j][jpatch] - place[j][ipatch];
                    d[1][0][j] = d[0][0][j] + size[j][jpatch];
                    d[0][1][j] = d[0][0][j] - size[j][ipatch];
                    d[1][1][j] = d[1][0][j] - size[j][ipatch];
                    d2[0][0][j] = d[0][0][j] * d[0][0][j];
                    d2[1][0][j] = d[1][0][j] * d[1][0][j];
                    d2[0][1][j] = d[0][1][j] * d[0][1][j];
                    d2[1][1][j] = d[1][1][j] * d[1][1][j];
                }
                let tmp2 = 0.0;
                for (let j = 0; j < 2; j++) {
                    for (let m = 0; m < 2; m++) {
                        for (let i = 0; i < 2; i++) {
                            const tmp3 = d2[m][i][1 - j] + tmp1;
                            const tmp4 = Math.sqrt(tmp3);
                            const tmp5 = 1.0 / tmp4;
                            let tmp8 = 0.0;
                            for (let k = 0; k < 2; k++) {
                                for (let n = 0; n < 2; n++) {
                                    const tmp6 = d2[k][n][j];
                                    const tmp7 = d[k][n][j];
                                    tmp2 = (tmp3 - tmp6) * Math.log(tmp6 + tmp3) - tmp2;
                                    tmp8 = -tmp7 * Math.atan(tmp7 * tmp5) - tmp8;
                                }
                                tmp2 = -tmp2;
                                tmp8 = -tmp8;
                            }
                            tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                        }
                        tmp2 = -tmp2;
                    }
                }
                coeff[ipatch][jpatch] = tmp2;
                coeff[jpatch][ipatch] = tmp2;
            }
        }
    }
}

/* --------------------------------------------------------------------- */
/*  SetUp2: form factors for orthogonal patches.                          */
/* --------------------------------------------------------------------- */

function setUp2(npatch: number, loop: number[][], coeff: number[][],
        place: number[][], size: number[][]): void {
    const r: number[][] = [[0, 0], [0, 0]];
    const r2: number[][] = [[0, 0], [0, 0]];
    const s: number[][] = [[0, 0], [0, 0]];

    for (let iface = 0; iface < 6; iface++) {
        for (let m = 0; m < 2; m++) {
            const jface = (iface + m + 1) % 6;
            for (let ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                for (let jpatch = loop[jface][0]; jpatch <= loop[jface][1]; jpatch++) {
                    r[0][0] = place[m][jpatch] - place[1 - m][ipatch];
                    r[1][0] = r[0][0] + size[m][jpatch];
                    r[0][1] = r[0][0] - size[1 - m][ipatch];
                    r[1][1] = r[1][0] - size[1 - m][ipatch];
                    s[0][0] = place[m][ipatch] - place[2][jpatch];
                    s[1][0] = s[0][0] + size[m][ipatch];
                    s[0][1] = place[1 - m][jpatch] - place[2][ipatch];
                    s[1][1] = s[0][1] + size[1 - m][jpatch];
                    for (let i = 0; i < 2; i++) {
                        for (let j = 0; j < 2; j++) {
                            s[i][j] = s[i][j] * s[i][j];
                            r2[i][j] = r[i][j] * r[i][j];
                        }
                        /* Bump the term by a small real to avoid
                         * singularities in the coupling function: */
                        s[i][1] = s[i][1] + 1e-35;
                    }
                    let tmp2 = 0.0;
                    for (let i = 0; i < 2; i++) {
                        for (let j = 0; j < 2; j++) {
                            const tmp3 = s[i][0] + s[j][1];
                            const tmp4 = Math.sqrt(tmp3);
                            const tmp5 = 1.0 / tmp4;
                            let tmp8 = 0.0;
                            for (let k = 0; k < 2; k++) {
                                for (let n = 0; n < 2; n++) {
                                    const tmp6 = r2[k][n];
                                    const tmp7 = r[k][n];
                                    tmp2 = (tmp3 - tmp6) * Math.log(tmp6 + tmp3) - tmp2;
                                    tmp8 = -tmp7 * Math.atan(tmp7 * tmp5) - tmp8;
                                }
                                tmp2 = -tmp2;
                                tmp8 = -tmp8;
                            }
                            tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                        }
                        tmp2 = -tmp2;
                    }
                    tmp2 = Math.abs(tmp2);
                    coeff[ipatch][jpatch] = tmp2;
                    coeff[jpatch][ipatch] = tmp2;
                }
            }
        }
    }
}

/* --------------------------------------------------------------------- */
/*  SetUp3: normalise row sums to 1 and fold in rho and emissivities.     */
/* --------------------------------------------------------------------- */

function setUp3(npatch: number, loop: number[][], area: number[],
        rho: number[][], emiss: number[][], coeff: number[][],
        diag: number[][], rhs: number[][]): boolean {
    const vtmp1: number[] = [0, 0, 0];
    const vtmp2: number[] = [0, 0, 0];

    for (let iface = 0; iface < 6; iface++) {
        for (let j = 0; j < 3; j++) {
            vtmp1[j] = 1.0 / rho[iface][j];
            vtmp2[j] = emiss[iface][j] * vtmp1[j];
        }
        for (let ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
            let tmp1 = 0.0;
            for (let jpatch = 0; jpatch < loop[iface][0]; jpatch++) {
                tmp1 += coeff[ipatch][jpatch];
            }
            for (let jpatch = loop[iface][1] + 1; jpatch < npatch; jpatch++) {
                tmp1 += coeff[ipatch][jpatch];
            }
            /* Make sure the row sum (total form factor) is close to 1: */
            if (Math.abs(tmp1 - area[ipatch]) > (0.5e-9 * tmp1)) {
                console.log("Total form factor is too far from unity.");
                return false;
            }
            tmp1 = -tmp1;
            /* Set coplanar patch interactions to zero. */
            for (let jpatch = loop[iface][0]; jpatch <= loop[iface][1]; jpatch++) {
                coeff[ipatch][jpatch] = 0.0;
            }
            /* Assign diagonal entries and right-hand sides. */
            for (let j = 0; j < 3; j++) {
                diag[j][ipatch] = vtmp1[j] * tmp1;
                rhs[j][ipatch] = vtmp2[j] * tmp1;
            }
        }
    }
    return true;
}

/* --------------------------------------------------------------------- */
/*  Solver: LDL^T factorisation and backsolve, per colour channel.        */
/*  No pivoting; the matrix is diagonally dominant.                       */
/* --------------------------------------------------------------------- */

/** Dot product of the first n elements of two vectors (unit stride). */
function ddot(a: number[], b: number[], n: number): number {
    let sum = 0.0;
    for (let i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

function solver(npatch: number, non0: number, coeff: number[][],
        diag: number[][], rhs: number[][], result: number[][]): void {
    for (let m = 0; m < 3; m++) {
        /* Load the lower triangle of coefficients, diagonal, and
         * solution vector. */
        for (let i = non0; i < npatch; i++) {
            coeff[i][i] = diag[m][i];
            result[m][i] = rhs[m][i];
            for (let j = 0; j < i; j++) {
                coeff[i][j] = coeff[j][i];
            }
        }

        /* Factor the matrix, writing factors over the original matrix. */
        for (let j = 0; j < non0; j++) {
            coeff[j][j] = 1.0 / diag[m][j];
            result[m][j] = rhs[m][j];
        }

        for (let j = non0; j < npatch; j++) {
            for (let k = non0; k < j; k++) {
                coeff[j][k] -= ddot(coeff[k], coeff[j], k);
            }
            for (let k = 0; k < j; k++) {
                const tmp1 = coeff[j][k];
                coeff[j][k] = tmp1 * coeff[k][k];
                coeff[j][j] -= tmp1 * coeff[j][k];
            }
            coeff[j][j] = 1.0 / coeff[j][j];
        }

        /* Backsolve, in three stages (for L, D, and L transpose). */
        for (let k = non0; k < npatch; k++) {
            result[m][k] -= ddot(result[m], coeff[k], k);
        }

        for (let k = 0; k < npatch; k++) {
            result[m][k] *= coeff[k][k];
        }

        /* Column access across rows (the C code uses stride NMAX): */
        for (let k = npatch - 2; k >= non0; k--) {
            let sum = 0.0;
            for (let j = k + 1; j < npatch; j++) {
                sum += result[m][j] * coeff[j][k];
            }
            result[m][k] -= sum;
        }

        for (let k = non0 - 1; k >= 0; k--) {
            let sum = 0.0;
            for (let j = non0; j < npatch; j++) {
                sum += result[m][j] * coeff[j][k];
            }
            result[m][k] -= sum;
        }
    }
}

/* --------------------------------------------------------------------- */
/*  Storer: writes the answer to the 'answer' file.                       */
/* --------------------------------------------------------------------- */

function storer(npatch: number, loop: number[][], result: number[][],
        place: number[][], size: number[][]): boolean {
    try {
        const lines: string[] = [];
        lines.push(d(npatch, 0) + " patches:");
        lines.push(" Patch  Face       Position in w, h, d              Width     Height");
        for (let iface = 0; iface < 6; iface++) {
            for (let ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                lines.push(
                    d(ipatch + 1, 5) + "   " + d(iface + 1, 4)
                    + f(place[0][ipatch], 11, 5) + f(place[1][ipatch], 11, 5)
                    + f(place[2][ipatch], 11, 5) + "  "
                    + f(size[0][ipatch], 11, 5) + f(size[1][ipatch], 11, 5));
            }
        }
        lines.push("");
        lines.push(" Patch  Face  Radiosities");
        for (let iface = 0; iface < 6; iface++) {
            for (let ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                lines.push(
                    d(ipatch + 1, 5) + "   " + d(iface + 1, 4)
                    + f(result[0][ipatch], 12, 8) + f(result[1][ipatch], 12, 8)
                    + f(result[2][ipatch], 12, 8));
            }
        }
        fs.writeFileSync("answer", lines.join("\n") + "\n", "utf8");
    } catch (e) {
        console.log("Unable to open 'answer' file.");
        return false;
    }
    return true;
}

/* --------------------------------------------------------------------- */
/*  Verify: checks that the computed radiosities satisfy the equations.   */
/*  Runs after the clock has stopped.                                     */
/* --------------------------------------------------------------------- */

function verify(npatch: number, coeff: number[][], diag: number[][],
        rhs: number[][], result: number[][]): boolean {
    let tmp1 = 0.0;
    let anorm = 0.0;
    let xnorm = 0.0;

    for (let m = 0; m < 3; m++) {
        /* Copy the lower triangle of the coefficients to the upper
         * triangle, and load the diagonal. */
        for (let i = 0; i < npatch; i++) {
            coeff[i][i] = diag[m][i];
            for (let j = 0; j < i; j++) {
                coeff[i][j] = coeff[j][i];
            }
        }
        /* Multiply the matrix by the solution vector, and accumulate
         * the norm of the residual. */
        anorm = 0.0;
        xnorm = 0.0;
        for (let j = 0; j < npatch; j++) {
            let tmp2 = rhs[m][j];
            for (let i = 0; i < npatch; i++) {
                tmp2 -= coeff[j][i] * result[m][i];
                anorm = Math.max(anorm, Math.abs(coeff[j][i]));
            }
            xnorm = Math.max(xnorm, Math.abs(result[m][j]));
            tmp1 += Math.abs(tmp2);
        }
    }
    tmp1 /= (anorm * xnorm);
    if (tmp1 > 3.0 * EPS) {
        console.log("Residual is too large: " + g(tmp1));
        return false;
    }
    return true;
}

/* --------------------------------------------------------------------- */
/*  Meter: times one complete run of the benchmark at size npatch.        */
/* --------------------------------------------------------------------- */

/** Result of Meter(); ok is false when the run was rejected. */
export interface MeterResult {
    ok: boolean;
    timing: number;
    work: number;
}

export function meter(npatch: number): MeterResult {
    /* First check that npatch lies between 6 and NMAX: */
    if (npatch < 6) {
        console.log("Must be at least 6, the number of faces.");
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    if (npatch > NMAX) {
        console.log("Exceeds " + NMAX + " = maximum for this system.");
        return { ok: false, timing: 0.0, work: 0.0 };
    }

    /* Ensure that a previous 'answer' file is deleted: */
    try {
        fs.unlinkSync("answer");
    } catch (e) {
        /* Ignore; Storer overwrites it anyway. */
    }

    const box: number[] = [0, 0, 0, 0, 0, 0, 0];
    const rho: number[][] = [
        [0, 0, 0], [0, 0, 0], [0, 0, 0],
        [0, 0, 0], [0, 0, 0], [0, 0, 0]];
    const emiss: number[][] = [
        [0, 0, 0], [0, 0, 0], [0, 0, 0],
        [0, 0, 0], [0, 0, 0], [0, 0, 0]];
    const coeff: number[][] = [];
    for (let i = 0; i < npatch; i++) {
        coeff.push(new Array(npatch).fill(0.0));
    }
    const diag: number[][] = [
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0)];
    const result: number[][] = [
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0)];
    const rhs: number[][] = [
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0),
        new Array(npatch).fill(0.0)];

    /* Time the tasks, individually and collectively. */
    const sec: number[] = [0, 0, 0, 0, 0, 0, 0, 0];
    sec[0] = when();
    if (!reader(box, rho, emiss)) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    sec[1] = when();

    const reg = region(npatch, box);
    if (reg === null) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    const loop = reg.loop;
    sec[2] = when();

    setUp1(npatch, loop, coeff, reg.place, reg.size);
    sec[3] = when();
    setUp2(npatch, loop, coeff, reg.place, reg.size);
    sec[4] = when();
    if (!setUp3(npatch, loop, reg.area, rho, emiss, coeff, diag, rhs)) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    sec[5] = when();
    const non0 = loop[1][0];
    solver(npatch, non0, coeff, diag, rhs, result);
    sec[6] = when();
    if (!storer(npatch, loop, result, reg.place, reg.size)) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    sec[7] = when();
    const timing = sec[7] - sec[0];
    const secs: number[] = [];
    for (let i = 0; i < 7; i++) {
        secs.push(sec[i + 1] - sec[i]);
    }

    /* Assess the floating-point work done by each routine, and total.
     * The counts are tallied into doubles to prevent integer overflow. */
    const p: number[] = [0, 0, 0, 0, 0, 0];
    let itmp1 = 0;
    let tmp1 = 0.0;
    for (let i = 0; i < 6; i++) {
        p[i] = loop[i][1] - loop[i][0] + 1;
        tmp1 += p[i] * p[i];
        /* int addition truncates, exactly like the C reference. */
        itmp1 += Math.trunc(Math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5);
    }
    const tmp2 = p[0] * p[3] + p[1] * p[4] + p[2] * p[5];
    const ops: number[] = [0, 0, 0, 0, 0, 0, 0];
    ops[0] = 258.0;
    ops[1] = 154.0 + 8.0 * itmp1 + npatch;
    ops[2] = 3.0 + 840.0 * tmp2;
    ops[3] = 430.0 * (npatch * npatch - tmp1 - 2.0 * tmp2) / 2.0;
    ops[4] = 72.0 + 9.0 * npatch + npatch * npatch - tmp1;
    ops[5] = npatch * (npatch * (npatch + 7.5) - 2.5) - 21.0
        + (non0 + 1.0) * ((non0 + 1.0) * (2.0 * (non0 + 1.0) - 16.5) + 35.5)
        + (non0 + 1.0) * npatch * (9.0 - 3.0 * (non0 + 1.0));
    ops[6] = 48.0 * npatch;
    const work = ops[0] + ops[1] + ops[2] + ops[3] + ops[4] + ops[5] + ops[6];

    /* Display the timing-work-speed breakdown by routine. */
    console.log(npatch + " patches:");
    console.log(" Task  Seconds       Operations        MFLOPS    % of Time");
    for (let i = 0; i < 7; i++) {
        if (secs[i] === 0.0) {
            secs[i] = 0.001;
        }
        console.log(meterRow(TASKS[i], secs[i], ops[i],
                (ops[i] / secs[i]) * 1e-6, 100.0 * secs[i] / timing));
    }
    console.log(meterRow("TOTALS", timing, work, (work / timing) * 1e-6, 100.0));
    verify(npatch, coeff, diag, rhs, result);

    return { ok: true, timing, work };
}

/* --------------------------------------------------------------------- */
/*  Machine information (the TypeScript counterpart of system_info.c).    */
/* --------------------------------------------------------------------- */

/** Reads the value of a "key: value" line from a text file, or null. */
function readFirstField(file: string, key: string): string | null {
    try {
        const text = fs.readFileSync(file, "utf8");
        for (const line of text.split("\n")) {
            if (line.startsWith(key)) {
                const colon = line.indexOf(":");
                if (colon >= 0) {
                    const value = line.substring(colon + 1).trim();
                    if (value.length > 0) {
                        return value;
                    }
                }
            }
        }
    } catch (e) {
        /* Fall through. */
    }
    return null;
}

/** Reads a whole (usually one line) text file, trimmed, or null. */
function readTextFile(file: string): string | null {
    try {
        /* The device tree strings are NUL padded. */
        return fs.readFileSync(file, "utf8").replace(/\u0000/g, "").trim();
    } catch (e) {
        return null;
    }
}

/** Best-effort human readable processor name. */
function detectProcessorName(): string {
    let name = readTextFile("/sys/firmware/devicetree/base/model");
    if (name === null) {
        name = readFirstField("/proc/cpuinfo", "Hardware");
    }
    if (name === null) {
        name = readFirstField("/proc/cpuinfo", "model name");
    }
    if (name === null) {
        name = readFirstField("/proc/cpuinfo", "cpu model");
    }
    if (name === null) {
        try {
            name = os.cpus()[0].model.trim();
        } catch (e) {
            name = null;
        }
    }
    if (name === null || name.length === 0) {
        name = os.arch();
    }
    return name;
}

/** Best-effort CPU frequency in GHz; 0.0 when unavailable. */
function detectClockSpeed(): number {
    const freqFiles = [
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"];
    for (const file of freqFiles) {
        const value = readTextFile(file);
        if (value !== null) {
            const khz = parseFloat(value.split(/\s+/)[0]);
            if (khz > 0.0) {
                return khz / 1.0e6; /* kHz -> GHz */
            }
        }
    }
    const mhz = readFirstField("/proc/cpuinfo", "cpu MHz");
    if (mhz !== null) {
        const value = parseFloat(mhz);
        if (value > 0.0) {
            return value / 1.0e3; /* MHz -> GHz */
        }
    }
    return 0.0;
}

/** Parses a sysfs cache size like "32K" or "1M" into KiB. */
function cacheKib(value: string | null): number {
    if (value === null || value.length === 0) {
        return 0;
    }
    value = value.trim();
    let multiplier = 1.0;
    if (value.endsWith("K") || value.endsWith("k")) {
        value = value.substring(0, value.length - 1);
    } else if (value.endsWith("M") || value.endsWith("m")) {
        value = value.substring(0, value.length - 1);
        multiplier = 1024.0;
    }
    const parsed = parseFloat(value);
    return Number.isNaN(parsed) ? 0 : Math.trunc(parsed * multiplier);
}

/** {L1i, L2} cache sizes in KiB, from sysfs when possible. */
function detectCaches(): number[] {
    let l1i = 0;
    let l2 = 0;
    const base = "/sys/devices/system/cpu/cpu0/cache";
    let entries: string[] = [];
    try {
        entries = fs.readdirSync(base);
    } catch (e) {
        /* No sysfs cache data. */
    }
    for (const entry of entries) {
        const level = readTextFile(base + "/" + entry + "/level");
        const kind = readTextFile(base + "/" + entry + "/type");
        const size = cacheKib(readTextFile(base + "/" + entry + "/size"));
        if (level === "1" && kind !== null
                && kind.toLowerCase().startsWith("instruction")) {
            l1i = size;
        } else if (level === "2") {
            l2 = size;
        }
    }
    return [l1i, l2];
}

function userName(): string {
    try {
        return os.userInfo().username;
    } catch (e) {
        return process.env.USER || process.env.USERNAME || "unknown";
    }
}

function two(n: number): string {
    return String(n).padStart(2, "0");
}

/** Prints the machine report, mirroring system_info()'s layout. */
function systemInfo(): void {
    const proc = detectProcessorName();
    const clock = detectClockSpeed();
    const caches = detectCaches();
    const total = os.totalmem() / (1024 * 1024 * 1024);
    const used = (os.totalmem() - os.freemem()) / (1024 * 1024 * 1024);
    const memUsage = total > 0.0 ? (used / total) * 100.0 : 0.0;
    const cores = os.cpus().length > 0 ? os.cpus().length : 1;
    const now = new Date();
    const date = two(now.getDate()) + "/" + two(now.getMonth() + 1) + "/"
        + String(now.getFullYear()).padStart(4, "0") + " (dd/mm) "
        + two(now.getHours()) + ":" + two(now.getMinutes()) + ":"
        + two(now.getSeconds());

    console.log("Machine:" + os.arch() + "\t\t Processor:  " + proc);
    console.log("Memory: " + f(memUsage, 3, 2) + "% \t\t # of procs: " + cores);
    console.log("L1 iCache:" + caches[0] + " KB\t\t # used:1");
    if (clock > 0.0) {
        console.log("NMAX:" + d(NMAX, 5) + " \t\t Clock:" + f(clock, 5, 2) + "GHz");
    } else {
        /* The frequency cannot always be determined; do not pretend
         * it is 0.00GHz. */
        console.log("NMAX:" + d(NMAX, 5) + " \t\t Clock:   N/A");
    }
    console.log("OS:" + os.type() + "\t\t Timer:Wall, process.hrtime.bigint()");
    console.log("L2 Cache:" + caches[1] + " KB\t\t Node: " + os.hostname());
    console.log("Language:TypeScript\t\t Version:" + process.version);
    console.log("Compiler:tsc\t\t Run by:" + userName());
    console.log("Options:standard TypeScript\t Date:" + date);
}

/* --------------------------------------------------------------------- */
/*  What: prints the machine report and the final statistics.             */
/* --------------------------------------------------------------------- */

/** One row of the per-task table: C format "%6.6s%8.3f%17.0f%14.6f%10.1f %%".
 *  Exported so the formatting can be tested against the C reference. */
export function meterRow(task: string, sec: number, ops: number,
        mflops: number, pct: number): string {
    return task.slice(0, 6).padStart(6)
        + f(sec, 8, 3) + f(ops, 17, 0)
        + f(mflops, 14, 6) + f(pct, 10, 1) + " %";
}

/** The final statistics block printed by What(), without the machine
 *  report. Exported so the formatting can be tested against the C
 *  reference. */
export function resultLines(n: number, timing: number, work: number): string {
    return "M ops:    " + g(work * 1e-6).padEnd(13)
        + "        Time:       " + f(timing, 0, 3) + " seconds\n"
        + "n:        " + String(n).padEnd(6) + "               MFLOPS:     "
        + g((work / timing) * 1e-6, 5) + "\n"
        + "Approximate data memory use: "
        + Math.trunc(8 * n * n + 120 * n + 800) + " bytes.\n";
}

function what(n: number, timing: number, work: number): void {
    console.log();
    systemInfo();

    console.log();
    process.stdout.write(resultLines(n, timing, work));
}

/* --------------------------------------------------------------------- */
/*  Driver (the TypeScript counterpart of sl-driver.c).                   */
/* --------------------------------------------------------------------- */

interface Params {
    goal: number;
    lBound: number;
    uBound: number;
}

function errmsg(msg: string): void {
    process.stderr.write("Error: " + msg + "\n");
}

function helpMessage(): void {
    process.stderr.write("Usage:(All paramters below are required)\n");
    process.stderr.write("\tSlalom -t <seconds> -l <integer> -u <integer>\n");
    process.stderr.write("\tSlalom --time <seconds> --lower-bound integer "
        + "--upper-bound integer\n");
}

/** Returns parsed parameters, or null after printing an error/help. */
function commandLine(argv: string[]): Params | null {
    /* No arguments specified. */
    if (argv.length === 0) {
        helpMessage();
        return null;
    }

    const params: Params = { goal: 0.0, lBound: 0, uBound: 0 };
    let sawTime = false;
    let sawLower = false;
    let sawUpper = false;
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        let value: string | null = null;
        let option: string;
        if (arg === "-h" || arg === "--help") {
            helpMessage();
            return null;
        } else if (arg.startsWith("--")) {
            const eq = arg.indexOf("=");
            if (eq >= 0) {
                option = arg.substring(0, eq);
                value = arg.substring(eq + 1);
            } else {
                option = arg;
            }
        } else {
            option = arg;
        }

        if (value === null) {
            /* Option and value may also come as two arguments. */
            if (i + 1 >= argv.length) {
                return null;
            }
            value = argv[++i];
        }

        let parsed: number;
        try {
            if (option === "-t" || option === "--time") {
                parsed = parseFloat(value);
            } else if (option === "-l" || option === "--lower-bound"
                    || option === "-u" || option === "--upper-bound") {
                parsed = parseInt(value, 10);
            } else {
                errmsg("unknown option: " + option);
                return null;
            }
        } catch (e) {
            parsed = NaN;
        }
        if (Number.isNaN(parsed)) {
            errmsg("Invalid or incomplete parameters:\n"
                + "\ttime, lower-bound, upper-bound");
            return null;
        }
        if (option === "-t" || option === "--time") {
            params.goal = parsed;
            sawTime = true;
        } else if (option === "-l" || option === "--lower-bound") {
            params.lBound = parsed;
            sawLower = true;
        } else {
            params.uBound = parsed;
            sawUpper = true;
        }
    }
    if (!(sawTime && sawLower && sawUpper)) {
        errmsg("Invalid or incomplete parameters:\n"
            + "\ttime, lower-bound, upper-bound");
        return null;
    }
    if (params.goal <= 0 || params.lBound <= 0
            || params.uBound <= params.lBound) {
        errmsg("Invalid or incomplete parameters:\n"
            + "\ttime, lower-bound, upper-bound");
        return null;
    }
    return params;
}

/**
 * Checks the lower bound provided by the user: the time taken to solve
 * the problem must be less than the goal.
 */
function setLowerBound(goal: number, lowerBound: number): boolean {
    const run = meter(lowerBound);
    if (!run.ok) {
        return false;
    }
    if (run.timing >= goal) {
        process.stderr.write("Must take less than " + g(goal)
            + " seconds. Took " + g(run.timing) + ".\n");
        return false;
    }
    return true;
}

/**
 * Checks the upper bound provided by the user: the time taken to solve
 * the problem must be at least the goal.
 */
function setUpperBound(goal: number, upperBound: number): boolean {
    const run = meter(upperBound);
    if (!run.ok) {
        return false;
    }
    if (run.timing < goal) {
        process.stderr.write("Must take atleast " + g(goal)
            + " seconds. Took " + g(run.timing) + ".\n");
        return false;
    }
    return true;
}

/**
 * Bisection over the problem size: while the [n, nupper] interval is
 * larger than 1, bisect it and pick a half.
 */
function slalomMainLoop(goal: number, lowerBound: number,
        upperBound: number): boolean {
    while (upperBound - lowerBound > 1) {
        const mean = Math.trunc((lowerBound + upperBound) / 2);
        const run = meter(mean);
        if (!run.ok) {
            return false;
        }
        if (run.timing < goal) {
            lowerBound = mean;
        } else {
            upperBound = mean;
        }
        console.log("New interval: [" + lowerBound + "," + upperBound + "]");
    }

    /* Ensure that the most recent run was for lowerBound. */
    const run = meter(lowerBound);
    if (!run.ok) {
        return false;
    }
    what(lowerBound, run.timing, run.work);
    return true;
}

function main(argv: string[]): void {
    const params = commandLine(argv);
    if (params === null) {
        process.exit(1);
    }
    if (!setLowerBound(params.goal, params.lBound)) {
        process.exit(1);
    }
    if (!setUpperBound(params.goal, params.uBound)) {
        process.exit(1);
    }
    process.exit(slalomMainLoop(params.goal, params.lBound, params.uBound)
        ? 0 : 1);
}

/* Run the driver only when this file is the entry point, so the module can
 * also be imported (e.g. by tests calling meter() at a fixed size). */
const isDirectRun =
    typeof require !== "undefined" && require.main === module;
if (isDirectRun) {
    main(process.argv.slice(2));
}
