/*
 * Machine information (the TypeScript counterpart of system_info.c) and the
 * final statistics block (What in the C reference).
 */

import * as fs from "fs";
import * as os from "os";
import { f, d } from "./format";
import { NMAX, resultLines } from "./benchmark";

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

/** Prints the machine report and the final statistics. */
export function what(n: number, timing: number, work: number): void {
    console.log();
    systemInfo();

    console.log();
    process.stdout.write(resultLines(n, timing, work));
}
