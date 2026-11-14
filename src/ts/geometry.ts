/*
 * The problem description and the surface decomposition: Reader and Region
 * in the C reference.
 *
 * The 'geom' file is looked up next to the compiled JavaScript file (with a
 * fallback to the parent directory, so an in-tree build finds ../src/geom).
 */

import * as fs from "fs";
import * as path from "path";

/** The three colour channels, in the order the benchmark reports them. */
export type Channel = "R" | "G" | "B";
export const CHANNELS: readonly Channel[] = ["R", "G", "B"];

/** A position in width, height and depth. */
export type Place = readonly [number, number, number];

/** One surface element of the box. */
export class Patch {
    constructor(
        /** Global patch number, 0-based (printed 1-based). */
        readonly index: number,
        /** Position in width, height and depth. */
        readonly place: Place,
        /** Extent along the width axis. */
        readonly width: number,
        /** Extent along the height axis. */
        readonly height: number,
        /** Area of the patch, multiplied by 8 * pi. */
        readonly area: number) {
    }

    /** Position along an axis: 0 = width, 1 = height, 2 = depth. */
    placeAt(axis: number): number {
        return this.place[axis];
    }

    /** Extent along axis 0 (width) or 1 (height). */
    extent(axis: number): number {
        return axis === 0 ? this.width : this.height;
    }
}

/** One face of the box and the patches it was subdivided into. */
export class Face implements Iterable<Patch> {
    readonly patches: Patch[] = [];

    constructor(
        /** 0-based face number (printed 1-based). */
        readonly index: number,
        /** Inclusive global index of this face's first patch. */
        readonly firstPatch: number,
        /** Inclusive global index of this face's last patch. */
        readonly lastPatch: number,
        /** Reflectivity per colour channel. */
        readonly reflectivity: Record<Channel, number>,
        /** Emissivity per colour channel. */
        readonly emissivity: Record<Channel, number>) {
    }

    /** Adds a patch, in global patch order. */
    addPatch(patch: Patch): void {
        this.patches.push(patch);
    }

    /** Number of patches on this face. */
    get patchCount(): number {
        return this.lastPatch - this.firstPatch + 1;
    }

    [Symbol.iterator](): Iterator<Patch> {
        return this.patches[Symbol.iterator]();
    }
}

/** The problem description: the box dimensions and the per-face optical
 *  properties, read and validated from the 'geom' file (Reader in the C
 *  reference). */
export class Problem {
    /** Box dimensions: x, y, z, the mirrored three, then x again. */
    readonly box: number[] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];

    /** Reflectivity of each of the six faces, per colour channel. */
    readonly reflectivity: Record<Channel, number>[] = [];

    /** Emissivity of each of the six faces, per colour channel. */
    readonly emissivity: Record<Channel, number>[] = [];

    private constructor() {
        for (let iface = 0; iface < 6; iface++) {
            this.reflectivity.push({ R: 0.0, G: 0.0, B: 0.0 });
            this.emissivity.push({ R: 0.0, G: 0.0, B: 0.0 });
        }
    }

    private static geomPath(): string {
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

    /**
     * Reads and validates the 'geom' file.
     *
     * @return the problem, or null after printing the C reference's error
     *         message
     */
    static read(): Problem | null {
        let text: string;
        try {
            text = fs.readFileSync(Problem.geomPath(), "utf8");
        } catch (e) {
            console.log("slalom:  'geom' geometry file not found.");
            return null;
        }

        /* The C code reads with fscanf() and discards the remainder of the
         * box line and of the third reflectivity/emissivity line.  Here the
         * numeric tokens are extracted from the whole file, which also
         * tolerates the trailing comments used by the shipped geom file. */
        const number = /[-+]?(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)/g;
        const values: number[] = [];
        let match: RegExpExecArray | null;
        while (values.length < 39 && (match = number.exec(text)) !== null) {
            values.push(parseFloat(match[0]));
        }
        if (values.length < 39) {
            console.log("slalom:  'geom' does not contain the required 39 numbers.");
            return null;
        }

        const problem = new Problem();
        problem.box[0] = values[0];
        problem.box[1] = values[1];
        problem.box[2] = values[2];

        /* Colour-major, exactly as the C reference reads them: one colour
         * channel after another, six faces per channel. */
        let idx = 3;
        for (const channel of CHANNELS) {
            for (let iface = 0; iface < 6; iface++) {
                problem.reflectivity[iface][channel] = values[idx++];
            }
        }
        for (const channel of CHANNELS) {
            for (let iface = 0; iface < 6; iface++) {
                problem.emissivity[iface][channel] = values[idx++];
            }
        }

        /* Sanity-check the values that were just read. */
        for (const channel of CHANNELS) {
            const j = CHANNELS.indexOf(channel);
            if (problem.box[j] < 1.0 || problem.box[j] >= 100.0) {
                console.log("Box dimensions must be between 1 and 100.");
                return null;
            }
            problem.box[j + 3] = problem.box[j];

            let maxEmiss = 0.0;
            for (let iface = 0; iface < 6; iface++) {
                const rho = problem.reflectivity[iface][channel];
                const emiss = problem.emissivity[iface][channel];
                if (rho < 0.000 || rho > 0.999) {
                    console.log("Reflectivities must be between .000 and .999.");
                    return null;
                }
                if (emiss < 0.0) {
                    console.log("Emissivity cannot be negative.");
                    return null;
                }
                if (maxEmiss < emiss) {
                    maxEmiss = emiss;
                }
            }
            if (maxEmiss === 0.0) {
                console.log("Emissivities are zero.  Problem is trivial.");
                return null;
            }
        }
        problem.box[6] = problem.box[3];
        return problem;
    }
}

/** The box decomposed into nearly-square patches (Region in the C
 *  reference).  Patches are allocated to the six faces proportionate to
 *  their areas, then each face is subdivided into a grid of nearly square
 *  patches. */
export class Geometry implements Iterable<Patch> {
    readonly faces: Face[] = [];

    private constructor(readonly box: number[]) {
    }

    /** Face {@code iface}, 0-based. */
    face(iface: number): Face {
        return this.faces[iface];
    }

    /** Total number of patches on the box. */
    get patchCount(): number {
        return this.faces[this.faces.length - 1].lastPatch + 1;
    }

    /** Index of the first patch not on face 0; the solver's non0, because
     *  the coplanar interactions on face 0 carry no unknowns. */
    get firstNonCoplanar(): number {
        return this.faces[1].firstPatch;
    }

    /** Iterates over every patch, in global patch order. */
    *[Symbol.iterator](): Iterator<Patch> {
        for (const face of this.faces) {
            yield* face;
        }
    }

    /**
     * Decomposes the surface of the box into {@code npatch} patches that
     * are as nearly equal in size and square as possible.
     *
     * @return the geometry, or null after printing the C reference's error
     *         message when the decomposition fails
     */
    static decompose(npatch: number, problem: Problem): Geometry | null {
        const geometry = new Geometry(problem.box);
        const box = problem.box;

        /* Allocate patches to each face, proportionate to area of each face.
         * Each face starts where the previous one ended; only the last patch
         * is forced, so the six ranges always partition [0, npatch). */
        const totalArea = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
        let covered = 0.0;
        let firstPatch = 0;
        for (let iface = 0; iface < 6; iface++) {
            let lastPatch: number;
            if (iface === 5) {
                lastPatch = npatch - 1;
            } else {
                covered = covered + box[iface] * box[iface + 1];
                lastPatch = Math.trunc(npatch * covered / totalArea + 0.5) - 1;
            }
            geometry.faces.push(new Face(iface, firstPatch, lastPatch,
                problem.reflectivity[iface], problem.emissivity[iface]));
            firstPatch = lastPatch + 1;
        }

        /* Subdivide each face into its nearly square patches. */
        for (let iface = 0; iface < 6; iface++) {
            const face = geometry.faces[iface];
            const numpat = face.patchCount;
            let depth = 0.0;
            if (iface >= 3) {
                depth = box[iface - 1];
            }
            const numcol = Math.min(numpat,
                    Math.max(1, Math.trunc(
                        Math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5)));
            const width = box[iface] / numcol;
            let rowCursor = numcol - 1;
            let offsetW = 0.0;
            for (let column = 0; column < numcol; column++) {
                const rowOffset = Math.trunc(rowCursor / numcol);
                const numrow = Math.trunc((rowCursor + numpat) / numcol) - rowOffset;
                if (numrow === 0) {
                    console.log("Eccentric box requires more patches.");
                    return null;
                }
                const height = box[iface + 1] / numrow;
                let offsetH = 0.0;
                const area = width * height * (8.0 * Math.PI);
                const lead = face.firstPatch + rowOffset;
                for (let ipatch = lead; ipatch < lead + numrow; ipatch++) {
                    face.addPatch(new Patch(ipatch, [offsetW, offsetH, depth],
                            width, height, area));
                    offsetH = offsetH + height;
                }
                offsetW = offsetW + width;
                rowCursor = rowCursor + numpat;
            }
        }
        return geometry;
    }
}
