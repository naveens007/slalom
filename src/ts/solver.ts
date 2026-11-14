/*
 * The radiosity linear system: SetUp1 (parallel form factors), SetUp2
 * (orthogonal form factors), SetUp3 (normalisation), Solver (LDL^T) and
 * Verify (residual check) in the C reference.
 *
 * The matrix is stored as rows (coeff[i] is patch i's equation); the
 * per-channel diagonal terms, right-hand sides and solutions are vectors
 * keyed by colour channel.  Vectors and matrix rows are Float64Arrays:
 * typed numeric buffers are the natural fit for this kernel and keep the
 * hot loops monomorphic.  The arithmetic order of every loop is kept
 * identical to the C reference so the results agree bit for bit.
 */

import { g } from "./format";
import { CHANNELS, Channel, Geometry, Patch } from "./geometry";

const EPS = 0.5e-8;

/** A dense vector of doubles. */
export type Vector = Float64Array;

/** Row-major matrix: one vector per row. */
export type Matrix = Vector[];

/** A 2 x 2 grid of corner offsets. */
type Grid = [[number, number], [number, number]];

function zeros(n: number): Vector {
    return new Float64Array(n);
}

function matrix(n: number): Matrix {
    const rows: Matrix = [];
    for (let i = 0; i < n; i++) {
        rows.push(zeros(n));
    }
    return rows;
}

/** Dot product of the first n elements of two vectors (unit stride). */
function ddot(a: ArrayLike<number>, b: ArrayLike<number>, n: number): number {
    let sum = 0.0;
    for (let i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/** Element-wise squares of a 2 x 2 corner grid. */
function squareGrid(grid: Grid): Grid {
    return [
        [grid[0][0] * grid[0][0], grid[0][1] * grid[0][1]],
        [grid[1][0] * grid[1][0], grid[1][1] * grid[1][1]],
    ];
}

/** Corner offsets between two parallel patches: one grid per axis. */
function parallelGrids(ipatch: Patch, jpatch: Patch): [Grid, Grid] {
    const gridFor = (axis: number): Grid => {
        const delta = jpatch.placeAt(axis) - ipatch.placeAt(axis);
        const own = ipatch.extent(axis);
        const other = jpatch.extent(axis);
        return [
            [delta, delta - own],
            [delta + other, delta + other - own],
        ];
    };
    return [gridFor(0), gridFor(1)];
}

/** Corner offsets, squares and bumped squares of two orthogonal patches:
 *  {r, r squared, s squared}. */
function orthogonalGrids(ipatch: Patch, jpatch: Patch,
        axis: number): [Grid, Grid, Grid] {
    const r00 = jpatch.placeAt(axis) - ipatch.placeAt(1 - axis);
    const ownOther = ipatch.extent(1 - axis);
    const r: Grid = [
        [r00, r00 - ownOther],
        [r00 + jpatch.extent(axis), r00 + jpatch.extent(axis) - ownOther],
    ];
    const r2 = squareGrid(r);

    const s00 = ipatch.placeAt(axis) - jpatch.placeAt(2);
    const s01 = jpatch.placeAt(1 - axis) - ipatch.placeAt(2);
    const s: Grid = [
        [s00, s01],
        [s00 + ipatch.extent(axis), s01 + jpatch.extent(1 - axis)],
    ];
    /* Bump the term by a small real to avoid singularities in the coupling
     * function. */
    const s2: Grid = [
        [s[0][0] * s[0][0], s[0][1] * s[0][1] + 1e-35],
        [s[1][0] * s[1][0], s[1][1] * s[1][1] + 1e-35],
    ];
    return [r, r2, s2];
}

/** One 2 x 2 evaluation of the analytic form-factor integral.
 *
 *  `distance2` is the squared separation along the coupling axis, `squares`
 *  the squared corner offsets and `lengths` the corresponding signed
 *  offsets.  `accumulated` is the running term carried through the caller's
 *  nested loops; the arithmetic order matches the C reference. */
function coupling(distance2: number, squares: Grid, lengths: Grid,
        accumulated: number): number {
    const root = Math.sqrt(distance2);
    const inverseRoot = 1.0 / root;
    let logTerm = accumulated;
    let angleTerm = 0.0;
    for (let k = 0; k < 2; k++) {
        for (let n = 0; n < 2; n++) {
            const square = squares[k][n];
            const length = lengths[k][n];
            logTerm = (distance2 - square) * Math.log(square + distance2) - logTerm;
            angleTerm = -length * Math.atan(length * inverseRoot) - angleTerm;
        }
        logTerm = -logTerm;
        angleTerm = -angleTerm;
    }
    return -4.0 * root * angleTerm - logTerm;
}

export class RadiositySystem {
    /** coeff[i] holds patch i's equation. */
    readonly coeff: Matrix;
    /** Per-channel diagonal terms, right-hand sides and solutions. */
    readonly diag: Record<Channel, Vector>;
    readonly rhs: Record<Channel, Vector>;
    readonly result: Record<Channel, Vector>;

    constructor(readonly geometry: Geometry) {
        const n = geometry.patchCount;
        this.coeff = matrix(n);
        this.diag = emptyChannelVectors(n);
        this.rhs = emptyChannelVectors(n);
        this.result = emptyChannelVectors(n);
    }

    /** SetUp1: form factors for parallel (opposite) patches. */
    addParallelFormFactors(): void {
        for (let iface = 0; iface < 3; iface++) {
            const face = this.geometry.face(iface);
            const opposite = this.geometry.face(iface + 3);
            const firstOpposite = opposite.patches[0];
            const depth = firstOpposite.placeAt(2);
            const depth2 = depth * depth;
            for (const ipatch of face) {
                for (const jpatch of opposite) {
                    const [d0, d1] = parallelGrids(ipatch, jpatch);
                    const d: [Grid, Grid] = [d0, d1];
                    const d2: [Grid, Grid] = [squareGrid(d0), squareGrid(d1)];
                    let total = 0.0;
                    for (let j = 0; j < 2; j++) {
                        for (let m = 0; m < 2; m++) {
                            for (let i = 0; i < 2; i++) {
                                total = coupling(d2[1 - j][m][i] + depth2,
                                    d2[j], d[j], total);
                            }
                            total = -total;
                        }
                    }
                    this.coeff[ipatch.index][jpatch.index] = total;
                    this.coeff[jpatch.index][ipatch.index] = total;
                }
            }
        }
    }

    /** SetUp2: form factors for orthogonal (adjacent) patches. */
    addOrthogonalFormFactors(): void {
        for (let iface = 0; iface < 6; iface++) {
            const face = this.geometry.face(iface);
            for (let m = 0; m < 2; m++) {
                const adjacent = this.geometry.face((iface + m + 1) % 6);
                for (const ipatch of face) {
                    for (const jpatch of adjacent) {
                        const [r, r2, s] = orthogonalGrids(ipatch, jpatch, m);
                        let total = 0.0;
                        for (let i = 0; i < 2; i++) {
                            for (let j = 0; j < 2; j++) {
                                total = coupling(s[i][0] + s[j][1], r2, r, total);
                            }
                            total = -total;
                        }
                        total = Math.abs(total);
                        this.coeff[ipatch.index][jpatch.index] = total;
                        this.coeff[jpatch.index][ipatch.index] = total;
                    }
                }
            }
        }
    }

    /** SetUp3: normalise row sums to 1 and fold in rho and emissivities.
     *  @return false on a form-factor check failure. */
    normalise(): boolean {
        const n = this.geometry.patchCount;
        for (const face of this.geometry.faces) {
            const inverseRho: Record<Channel, number> = { R: 0.0, G: 0.0, B: 0.0 };
            const scaledEmiss: Record<Channel, number> = { R: 0.0, G: 0.0, B: 0.0 };
            for (const channel of CHANNELS) {
                inverseRho[channel] = 1.0 / face.reflectivity[channel];
                scaledEmiss[channel] = face.emissivity[channel] * inverseRho[channel];
            }
            for (const patch of face) {
                const row = this.coeff[patch.index];
                let rowSum = 0.0;
                for (let jpatch = 0; jpatch < face.firstPatch; jpatch++) {
                    rowSum += row[jpatch];
                }
                for (let jpatch = face.lastPatch + 1; jpatch < n; jpatch++) {
                    rowSum += row[jpatch];
                }
                /* Make sure the row sum (total form factor) is close to 1: */
                if (Math.abs(rowSum - patch.area) > (0.5e-9 * rowSum)) {
                    console.log("Total form factor is too far from unity.");
                    return false;
                }
                rowSum = -rowSum;
                /* Set coplanar patch interactions to zero. */
                row.fill(0.0, face.firstPatch, face.lastPatch + 1);
                /* Assign diagonal entries and right-hand sides. */
                for (const channel of CHANNELS) {
                    this.diag[channel][patch.index] = inverseRho[channel] * rowSum;
                    this.rhs[channel][patch.index] = scaledEmiss[channel] * rowSum;
                }
            }
        }
        return true;
    }

    /** Solver: LDL^T factorisation and backsolve, per colour channel.  No
     *  pivoting; the matrix is diagonally dominant. */
    solve(): void {
        const n = this.geometry.patchCount;
        const non0 = this.geometry.firstNonCoplanar;

        for (const channel of CHANNELS) {
            const diagChannel = this.diag[channel];
            const rhsChannel = this.rhs[channel];
            const solution = this.result[channel];

            /* Load the lower triangle of coefficients, diagonal, and
             * solution vector. */
            for (let i = non0; i < n; i++) {
                this.coeff[i][i] = diagChannel[i];
                solution[i] = rhsChannel[i];
                for (let j = 0; j < i; j++) {
                    this.coeff[i][j] = this.coeff[j][i];
                }
            }

            for (let j = 0; j < non0; j++) {
                this.coeff[j][j] = 1.0 / diagChannel[j];
                solution[j] = rhsChannel[j];
            }

            /* Factor the matrix, writing factors over the original matrix. */
            for (let j = non0; j < n; j++) {
                const row = this.coeff[j];
                for (let k = non0; k < j; k++) {
                    row[k] -= ddot(this.coeff[k], row, k);
                }
                for (let k = 0; k < j; k++) {
                    const factor = row[k];
                    row[k] = factor * this.coeff[k][k];
                    row[j] -= factor * row[k];
                }
                row[j] = 1.0 / row[j];
            }

            /* Backsolve, in three stages (for L, D, and L transpose). */
            for (let k = non0; k < n; k++) {
                solution[k] -= ddot(solution, this.coeff[k], k);
            }

            for (let k = 0; k < n; k++) {
                solution[k] *= this.coeff[k][k];
            }

            /* Column access across rows (the C code uses stride NMAX): */
            for (let k = n - 2; k >= non0; k--) {
                let sum = 0.0;
                for (let j = k + 1; j < n; j++) {
                    sum += solution[j] * this.coeff[j][k];
                }
                solution[k] -= sum;
            }

            for (let k = non0 - 1; k >= 0; k--) {
                let sum = 0.0;
                for (let j = non0; j < n; j++) {
                    sum += solution[j] * this.coeff[j][k];
                }
                solution[k] -= sum;
            }
        }
    }

    /** Verify: checks that the computed radiosities satisfy the equations.
     *  Runs after the clock has stopped. */
    verify(): boolean {
        const n = this.geometry.patchCount;
        let residualTotal = 0.0;
        let anorm = 0.0;
        let xnorm = 0.0;

        for (const channel of CHANNELS) {
            const diagChannel = this.diag[channel];
            const solution = this.result[channel];

            /* Copy the lower triangle of the coefficients to the upper
             * triangle, and load the diagonal. */
            for (let i = 0; i < n; i++) {
                this.coeff[i][i] = diagChannel[i];
                for (let j = 0; j < i; j++) {
                    this.coeff[i][j] = this.coeff[j][i];
                }
            }
            /* Multiply the matrix by the solution vector, and accumulate
             * the norm of the residual.  The norms are per channel (only
             * the last channel's survive into the denominator); the
             * residual total accumulates across all three channels. */
            anorm = 0.0;
            xnorm = 0.0;
            for (let j = 0; j < n; j++) {
                let residual = this.rhs[channel][j];
                for (let i = 0; i < n; i++) {
                    residual -= this.coeff[j][i] * solution[i];
                    anorm = Math.max(anorm, Math.abs(this.coeff[j][i]));
                }
                xnorm = Math.max(xnorm, Math.abs(solution[j]));
                residualTotal += Math.abs(residual);
            }
        }
        residualTotal /= (anorm * xnorm);
        if (residualTotal > 3.0 * EPS) {
            console.log("Residual is too large: " + g(residualTotal));
            return false;
        }
        return true;
    }
}

function emptyChannelVectors(n: number): Record<Channel, Vector> {
    return {
        R: zeros(n),
        G: zeros(n),
        B: zeros(n),
    };
}
