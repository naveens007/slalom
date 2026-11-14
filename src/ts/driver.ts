/*
 * Driver (the TypeScript counterpart of sl-driver.c): command line parsing
 * and the fixed-time bisection search over the problem size.
 */

import { g } from "./format";
import { meter, MeterResult } from "./benchmark";
import { what } from "./machineInfo";

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
    const run: MeterResult = meter(lowerBound);
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
    const run: MeterResult = meter(upperBound);
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

export function main(argv: string[]): void {
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
