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
 * The port is organised around the problem domain instead of the C arrays:
 *
 *   format.ts    printf-compatible number formatting (%g, %f, %d)
 *   geometry.ts  the problem description and surface decomposition:
 *                Patch and Face objects, iterable over patches
 *   solver.ts    the linear system: form factors, LDL^T solve, residual
 *   benchmark.ts Meter (one timed run), Storer, analytic operation counts
 *   machineInfo.ts the machine report and final statistics block
 *   driver.ts    command line parsing and the bisection driver
 *   Slalom.ts    this entry module
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

export { g, f, d } from "./format";
export { meter, MeterResult, meterRow, resultLines } from "./benchmark";
import { main } from "./driver";

/* Run the driver only when this file is the entry point, so the module can
 * also be imported (e.g. by tests calling meter() at a fixed size). */
const isDirectRun =
    typeof require !== "undefined" && require.main === module;
if (isDirectRun) {
    main(process.argv.slice(2));
}
