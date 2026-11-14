/*
 * Meter: times one complete run of the benchmark at a given problem size,
 * from reading the geometry through storing the answer, and reports the
 * timing, analytic operation counts and speed of each phase.
 */

import * as fs from "fs";
import { f, d, g } from "./format";
import { Channel, Geometry, Problem } from "./geometry";
import { RadiositySystem, Vector } from "./solver";

/** NMAX = Largest npatch for this computer; overridable for experiments. */
export const NMAX: number = parseEnvInt("SLALOM_NMAX", 8192);

/** The seven timed phases, in report order. */
export const TASKS = [
    "Reader", "Region", "SetUp1", "SetUp2", "SetUp3", "Solver", "Storer"] as const;
export type Task = typeof TASKS[number];

function parseEnvInt(name: string, fallback: number): number {
    const raw = process.env[name];
    if (raw === undefined || raw.length === 0) {
        return fallback;
    }
    const parsed = parseInt(raw, 10);
    return Number.isNaN(parsed) ? fallback : parsed;
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
/*  Report rows and the final statistics block.                          */
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

/* --------------------------------------------------------------------- */
/*  Storer: writes the answer to the 'answer' file.                       */
/* --------------------------------------------------------------------- */

function storeAnswer(geometry: Geometry,
        result: Record<Channel, Vector>): boolean {
    try {
        const lines: string[] = [];
        lines.push(d(geometry.patchCount, 0) + " patches:");
        lines.push(" Patch  Face       Position in w, h, d              Width     Height");
        for (const face of geometry.faces) {
            for (const patch of face) {
                lines.push(
                    d(patch.index + 1, 5) + "   " + d(face.index + 1, 4)
                    + f(patch.placeAt(0), 11, 5) + f(patch.placeAt(1), 11, 5)
                    + f(patch.placeAt(2), 11, 5) + "  "
                    + f(patch.width, 11, 5) + f(patch.height, 11, 5));
            }
        }
        lines.push("");
        lines.push(" Patch  Face  Radiosities");
        for (const face of geometry.faces) {
            for (const patch of face) {
                lines.push(
                    d(patch.index + 1, 5) + "   " + d(face.index + 1, 4)
                    + f(result.R[patch.index], 12, 8)
                    + f(result.G[patch.index], 12, 8)
                    + f(result.B[patch.index], 12, 8));
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
/*  The analytic operation counts.                                        */
/* --------------------------------------------------------------------- */

/**
 * The analytic LLNL-weighted operation count of each phase; the counts
 * follow ../slalom.h's weights and must not be changed.
 */
function operationCounts(npatch: number, geometry: Geometry): number[] {
    const box = geometry.box;
    const p: number[] = [0, 0, 0, 0, 0, 0];
    let columns = 0;
    let sumSquares = 0.0;
    for (let i = 0; i < 6; i++) {
        p[i] = geometry.face(i).patchCount;
        sumSquares += p[i] * p[i];
        /* int addition truncates, exactly like the C reference. */
        columns += Math.trunc(Math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5);
    }
    const opposite = p[0] * p[3] + p[1] * p[4] + p[2] * p[5];
    const non0 = geometry.firstNonCoplanar + 1.0;
    const ops: number[] = [0, 0, 0, 0, 0, 0, 0];
    ops[0] = 258.0;
    ops[1] = 154.0 + 8.0 * columns + npatch;
    ops[2] = 3.0 + 840.0 * opposite;
    ops[3] = 430.0 * (npatch * npatch - sumSquares - 2.0 * opposite) / 2.0;
    ops[4] = 72.0 + 9.0 * npatch + npatch * npatch - sumSquares;
    ops[5] = npatch * (npatch * (npatch + 7.5) - 2.5) - 21.0
        + non0 * (non0 * (2.0 * non0 - 16.5) + 35.5)
        + non0 * npatch * (9.0 - 3.0 * non0);
    ops[6] = 48.0 * npatch;
    return ops;
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

    /* Time the tasks, individually and collectively. */
    const stamps: number[] = [when()];
    const problem = Problem.read();
    if (problem === null) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    stamps.push(when());

    const geometry = Geometry.decompose(npatch, problem);
    if (geometry === null) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    stamps.push(when());

    const system = new RadiositySystem(geometry);
    system.addParallelFormFactors();
    stamps.push(when());
    system.addOrthogonalFormFactors();
    stamps.push(when());
    if (!system.normalise()) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    stamps.push(when());
    system.solve();
    stamps.push(when());
    if (!storeAnswer(geometry, system.result)) {
        return { ok: false, timing: 0.0, work: 0.0 };
    }
    stamps.push(when());
    const timing = stamps[stamps.length - 1] - stamps[0];
    const secs = stamps.slice(1).map((stamp, index) => stamp - stamps[index]);

    /* Assess the floating-point work done by each routine, and total.
     * The counts are tallied into doubles to prevent integer overflow. */
    const ops = operationCounts(npatch, geometry);
    const work = ops.reduce((sum, value) => sum + value, 0.0);

    /* Display the timing-work-speed breakdown by routine. */
    console.log(npatch + " patches:");
    console.log(" Task  Seconds       Operations        MFLOPS    % of Time");
    let i = 0;
    for (const task of TASKS) {
        if (secs[i] === 0.0) {
            secs[i] = 0.001;
        }
        console.log(meterRow(task, secs[i], ops[i],
                (ops[i] / secs[i]) * 1e-6, 100.0 * secs[i] / timing));
        i++;
    }
    console.log(meterRow("TOTALS", timing, work, (work / timing) * 1e-6, 100.0));
    system.verify();

    return { ok: true, timing, work };
}
