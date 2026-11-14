import java.util.Arrays;
import java.util.EnumMap;
import java.util.Map;

/**
 * The radiosity linear system for one problem size.
 *
 * The matrix is stored as rows ({@code coeff[i]} is patch i's equation); the
 * per-channel diagonal terms, right-hand sides and solutions are vectors
 * keyed by {@link Channel}.  The methods correspond to the C reference's
 * SetUp1 (parallel form factors), SetUp2 (orthogonal form factors), SetUp3
 * (normalisation), Solver (LDL^T) and Verify (residual check), in that
 * order.  The arithmetic order of every loop is kept identical to the C
 * reference so the results agree bit for bit.
 */
final class RadiositySystem {

    private static final double EPS = 0.5e-8;

    final Geometry geometry;

    /** coeff[i] holds patch i's equation. */
    final double[][] coeff;

    /** Per-channel diagonal terms, right-hand sides and solutions. */
    final Map<Channel, double[]> diag = new EnumMap<Channel, double[]>(Channel.class);
    final Map<Channel, double[]> rhs = new EnumMap<Channel, double[]>(Channel.class);
    final Map<Channel, double[]> result = new EnumMap<Channel, double[]>(Channel.class);

    RadiositySystem(Geometry geometry) {
        int n = geometry.patchCount();
        this.geometry = geometry;
        this.coeff = new double[n][n];
        for (Channel channel : Channel.values()) {
            diag.put(channel, new double[n]);
            rhs.put(channel, new double[n]);
            result.put(channel, new double[n]);
        }
    }

    /** Dot product of the first n elements of two vectors (unit stride). */
    private static double ddot(double[] a, double[] b, int n) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += a[i] * b[i];
        }
        return sum;
    }

    /** Element-wise squares of a 2 x 2 corner grid. */
    private static double[][] squares(double[][] grid) {
        double[][] squared = new double[2][2];
        for (int row = 0; row < 2; row++) {
            for (int column = 0; column < 2; column++) {
                squared[row][column] = grid[row][column] * grid[row][column];
            }
        }
        return squared;
    }

    /** Corner offsets between two parallel patches: [axis][row][column]. */
    private static double[][][] parallelGrids(Patch ipatch, Patch jpatch) {
        double[][][] grids = new double[2][2][2];
        for (int axis = 0; axis < 2; axis++) {
            double delta = jpatch.place(axis) - ipatch.place(axis);
            double own = ipatch.extent(axis);
            double other = jpatch.extent(axis);
            grids[axis][0][0] = delta;
            grids[axis][0][1] = delta - own;
            grids[axis][1][0] = delta + other;
            grids[axis][1][1] = delta + other - own;
        }
        return grids;
    }

    /** Corner offsets, squares and bumped squares of two orthogonal patches:
     *  {r, r squared, s squared}. */
    private static double[][][] orthogonalGrids(Patch ipatch, Patch jpatch,
            int axis) {
        double[][] r = new double[2][2];
        double r00 = jpatch.place(axis) - ipatch.place(1 - axis);
        double ownOther = ipatch.extent(1 - axis);
        r[0][0] = r00;
        r[0][1] = r00 - ownOther;
        r[1][0] = r00 + jpatch.extent(axis);
        r[1][1] = r[1][0] - ownOther;
        double[][] r2 = squares(r);

        double[][] s = new double[2][2];
        s[0][0] = ipatch.place(axis) - jpatch.place(2);
        s[1][0] = s[0][0] + ipatch.extent(axis);
        s[0][1] = jpatch.place(1 - axis) - ipatch.place(2);
        s[1][1] = s[0][1] + jpatch.extent(1 - axis);
        for (int row = 0; row < 2; row++) {
            for (int column = 0; column < 2; column++) {
                s[row][column] = s[row][column] * s[row][column];
            }
            /* Bump the term by a small real to avoid singularities in the
             * coupling function: */
            s[row][1] = s[row][1] + 1e-35;
        }
        return new double[][][] {r, r2, s};
    }

    /** One 2 x 2 evaluation of the analytic form-factor integral.
     *
     *  {@code distance2} is the squared separation along the coupling axis,
     *  {@code squares} the squared corner offsets and {@code lengths} the
     *  corresponding signed offsets.  {@code accumulated} is the running
     *  term carried through the caller's nested loops; the arithmetic order
     *  matches the C reference. */
    private static double coupling(double distance2, double[][] squares,
            double[][] lengths, double accumulated) {
        double root = Math.sqrt(distance2);
        double inverseRoot = 1.0 / root;
        double logTerm = accumulated;
        double angleTerm = 0.0;
        for (int k = 0; k < 2; k++) {
            for (int n = 0; n < 2; n++) {
                double square = squares[k][n];
                double length = lengths[k][n];
                logTerm = (distance2 - square) * Math.log(square + distance2) - logTerm;
                angleTerm = -length * Math.atan(length * inverseRoot) - angleTerm;
            }
            logTerm = -logTerm;
            angleTerm = -angleTerm;
        }
        return -4.0 * root * angleTerm - logTerm;
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp1: form factors for parallel (opposite) patches.              */
    /* ------------------------------------------------------------------ */

    void addParallelFormFactors() {
        for (int iface = 0; iface < 3; iface++) {
            Face face = geometry.face(iface);
            Face opposite = geometry.face(iface + 3);
            double depth = opposite.patches().get(0).place(2);
            double depth2 = depth * depth;
            for (Patch ipatch : face) {
                for (Patch jpatch : opposite) {
                    double[][][] d = parallelGrids(ipatch, jpatch);
                    double[][][] d2 = {squares(d[0]), squares(d[1])};
                    double total = 0.0;
                    for (int j = 0; j < 2; j++) {
                        for (int m = 0; m < 2; m++) {
                            for (int i = 0; i < 2; i++) {
                                total = coupling(d2[1 - j][m][i] + depth2,
                                        d2[j], d[j], total);
                            }
                            total = -total;
                        }
                    }
                    coeff[ipatch.index][jpatch.index] = total;
                    coeff[jpatch.index][ipatch.index] = total;
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp2: form factors for orthogonal (adjacent) patches.            */
    /* ------------------------------------------------------------------ */

    void addOrthogonalFormFactors() {
        for (int iface = 0; iface < 6; iface++) {
            Face face = geometry.face(iface);
            for (int m = 0; m < 2; m++) {
                Face adjacent = geometry.face((iface + m + 1) % 6);
                for (Patch ipatch : face) {
                    for (Patch jpatch : adjacent) {
                        double[][][] grids = orthogonalGrids(ipatch, jpatch, m);
                        double[][] r = grids[0];
                        double[][] r2 = grids[1];
                        double[][] s = grids[2];
                        double total = 0.0;
                        for (int i = 0; i < 2; i++) {
                            for (int j = 0; j < 2; j++) {
                                total = coupling(s[i][0] + s[j][1], r2, r, total);
                            }
                            total = -total;
                        }
                        total = Math.abs(total);
                        coeff[ipatch.index][jpatch.index] = total;
                        coeff[jpatch.index][ipatch.index] = total;
                    }
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp3: normalise row sums to 1 and fold in rho and emissivities.  */
    /* ------------------------------------------------------------------ */

    /** @return false on a form-factor check failure. */
    boolean normalise() {
        int n = geometry.patchCount();
        for (Face face : geometry.faces()) {
            double[] inverseRho = new double[Channel.values().length];
            double[] scaledEmiss = new double[Channel.values().length];
            for (Channel channel : Channel.values()) {
                int index = channel.ordinal();
                inverseRho[index] = 1.0 / face.reflectivity.get(channel);
                scaledEmiss[index] = face.emissivity.get(channel) * inverseRho[index];
            }
            for (Patch patch : face) {
                double[] row = coeff[patch.index];
                double rowSum = 0.0;
                for (int jpatch = 0; jpatch < face.firstPatch; jpatch++) {
                    rowSum += row[jpatch];
                }
                for (int jpatch = face.lastPatch + 1; jpatch < n; jpatch++) {
                    rowSum += row[jpatch];
                }
                /* Make sure the row sum (total form factor) is close to 1: */
                if (Math.abs(rowSum - patch.area) > (0.5e-9 * rowSum)) {
                    System.out.println("Total form factor is too far from unity.");
                    return false;
                }
                rowSum = -rowSum;
                /* Set coplanar patch interactions to zero. */
                Arrays.fill(row, face.firstPatch, face.lastPatch + 1, 0.0);
                /* Assign diagonal entries and right-hand sides. */
                for (Channel channel : Channel.values()) {
                    int index = channel.ordinal();
                    diag.get(channel)[patch.index] = inverseRho[index] * rowSum;
                    rhs.get(channel)[patch.index] = scaledEmiss[index] * rowSum;
                }
            }
        }
        return true;
    }

    /* ------------------------------------------------------------------ */
    /*  Solver: LDL^T factorisation and backsolve, per colour channel.     */
    /*  No pivoting; the matrix is diagonally dominant.                    */
    /* ------------------------------------------------------------------ */

    void solve() {
        int n = geometry.patchCount();
        int non0 = geometry.firstNonCoplanar();

        for (Channel channel : Channel.values()) {
            double[] diagChannel = diag.get(channel);
            double[] rhsChannel = rhs.get(channel);
            double[] solution = result.get(channel);

            /* Load the lower triangle of coefficients, diagonal, and
             * solution vector. */
            for (int i = non0; i < n; i++) {
                coeff[i][i] = diagChannel[i];
                solution[i] = rhsChannel[i];
                for (int j = 0; j < i; j++) {
                    coeff[i][j] = coeff[j][i];
                }
            }

            for (int j = 0; j < non0; j++) {
                coeff[j][j] = 1.0 / diagChannel[j];
                solution[j] = rhsChannel[j];
            }

            /* Factor the matrix, writing factors over the original matrix. */
            for (int j = non0; j < n; j++) {
                for (int k = non0; k < j; k++) {
                    coeff[j][k] -= ddot(coeff[k], coeff[j], k);
                }
                for (int k = 0; k < j; k++) {
                    double factor = coeff[j][k];
                    coeff[j][k] = factor * coeff[k][k];
                    coeff[j][j] -= factor * coeff[j][k];
                }
                coeff[j][j] = 1.0 / coeff[j][j];
            }

            /* Backsolve, in three stages (for L, D, and L transpose). */
            for (int k = non0; k < n; k++) {
                solution[k] -= ddot(solution, coeff[k], k);
            }

            for (int k = 0; k < n; k++) {
                solution[k] *= coeff[k][k];
            }

            /* Column access across rows (the C code uses stride NMAX): */
            for (int k = n - 2; k >= non0; k--) {
                double sum = 0.0;
                for (int j = k + 1; j < n; j++) {
                    sum += solution[j] * coeff[j][k];
                }
                solution[k] -= sum;
            }

            for (int k = non0 - 1; k >= 0; k--) {
                double sum = 0.0;
                for (int j = non0; j < n; j++) {
                    sum += solution[j] * coeff[j][k];
                }
                solution[k] -= sum;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Verify: checks that the computed radiosities satisfy the          */
    /*  equations.  Runs after the clock has stopped.                     */
    /* ------------------------------------------------------------------ */

    boolean verify() {
        int n = geometry.patchCount();
        double residualTotal = 0.0;
        double anorm = 0.0;
        double xnorm = 0.0;

        for (Channel channel : Channel.values()) {
            double[] diagChannel = diag.get(channel);
            double[] solution = result.get(channel);

            /* Copy the lower triangle of the coefficients to the upper
             * triangle, and load the diagonal. */
            for (int i = 0; i < n; i++) {
                coeff[i][i] = diagChannel[i];
                for (int j = 0; j < i; j++) {
                    coeff[i][j] = coeff[j][i];
                }
            }
            /* Multiply the matrix by the solution vector, and accumulate
             * the norm of the residual.  The norms are per channel (only
             * the last channel's survive into the denominator); the
             * residual total accumulates across all three channels. */
            anorm = 0.0;
            xnorm = 0.0;
            for (int j = 0; j < n; j++) {
                double residual = rhs.get(channel)[j];
                for (int i = 0; i < n; i++) {
                    residual -= coeff[j][i] * solution[i];
                    anorm = Math.max(anorm, Math.abs(coeff[j][i]));
                }
                xnorm = Math.max(xnorm, Math.abs(solution[j]));
                residualTotal += Math.abs(residual);
            }
        }
        residualTotal /= (anorm * xnorm);
        if (residualTotal > 3.0 * EPS) {
            System.out.println("Residual is too large: " + CFormat.g(residualTotal));
            return false;
        }
        return true;
    }
}
