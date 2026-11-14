import java.math.MathContext;
import java.math.RoundingMode;
import java.util.Locale;

/**
 * C-compatible printf conversions.
 *
 * java.util.Formatter's %g keeps trailing zeros ("0.500000"), while C printf
 * %g strips them ("0.5") and switches to exponent form at different
 * thresholds.  The benchmark output is defined by the C reference, so %g is
 * re-implemented here to print identical text.
 */
public final class CFormat {

    /** Locale for every number the benchmark prints (C's "C" locale). */
    static final Locale LOC = Locale.US;

    private CFormat() {
    }

    /** Format like C's "%.{precision}g". */
    public static String g(double v, int precision) {
        if (v == 0.0) {
            return "0";
        }
        if (Double.isNaN(v)) {
            return "NaN";
        }
        if (Double.isInfinite(v)) {
            return v > 0.0 ? "Inf" : "-Inf";
        }
        int p = (precision <= 0) ? 1 : precision;

        /* Round to p significant digits (C uses round-to-nearest). */
        String sci = String.format(LOC, "%." + (p - 1) + "e", v);
        int expVal = Integer.parseInt(sci.substring(sci.indexOf('e') + 1));

        String s;
        if (expVal < -4 || expVal >= p) {
            /* Scientific notation: strip trailing zeros from the mantissa,
             * exponent with at least two digits. */
            String mant = sci.substring(0, sci.indexOf('e'));
            if (mant.indexOf('.') >= 0) {
                mant = mant.replaceAll("0+$", "");
                if (mant.endsWith(".")) {
                    mant = mant.substring(0, mant.length() - 1);
                }
            }
            String ePart = sci.substring(sci.indexOf('e') + 1);
            boolean negativeExponent = ePart.startsWith("-");
            if (ePart.startsWith("+") || ePart.startsWith("-")) {
                ePart = ePart.substring(1);
            }
            while (ePart.length() < 2) {
                ePart = "0" + ePart;
            }
            s = mant + "e" + (negativeExponent ? "-" : "+") + ePart;
        } else {
            /* Decimal notation: p significant digits, trailing zeros gone. */
            String fixed = new java.math.BigDecimal(v)
                    .round(new MathContext(p, RoundingMode.HALF_UP))
                    .toPlainString();
            if (fixed.indexOf('.') >= 0) {
                fixed = fixed.replaceAll("0+$", "");
                if (fixed.endsWith(".")) {
                    fixed = fixed.substring(0, fixed.length() - 1);
                }
            }
            s = fixed;
        }
        return s;
    }

    /** Format like C's "%g" (default precision 6). */
    public static String g(double v) {
        return g(v, 6);
    }
}
