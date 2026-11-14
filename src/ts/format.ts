/*
 * C-compatible printf conversions.
 *
 * JavaScript has no printf, and Number.prototype.toFixed keeps trailing
 * zeros while C's %g strips them, so the conversions used by the benchmark
 * output are re-implemented here to print identical text to the C
 * reference.
 */

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
