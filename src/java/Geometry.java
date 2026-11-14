import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * The box decomposed into nearly-square patches (Region in the C reference).
 *
 * Patches are allocated to the six faces proportionate to their areas, then
 * each face is subdivided into a grid of nearly square patches.
 */
public final class Geometry implements Iterable<Patch> {

    private final double[] box;
    private final List<Face> faces = new ArrayList<Face>();

    private Geometry(double[] box) {
        this.box = box;
    }

    /** The box dimensions (seven entries; see {@link Problem#box()}). */
    public double[] box() {
        return box;
    }

    /** The six faces, in order. */
    public List<Face> faces() {
        return faces;
    }

    /** Face {@code iface}, 0-based. */
    public Face face(int iface) {
        return faces.get(iface);
    }

    /** Total number of patches on the box. */
    public int patchCount() {
        return faces.get(faces.size() - 1).lastPatch + 1;
    }

    /**
     * Index of the first patch not on face 0; the solver's {@code non0},
     * because the coplanar interactions on face 0 carry no unknowns.
     */
    public int firstNonCoplanar() {
        return faces.get(1).firstPatch;
    }

    /** Iterates over every patch, in global patch order. */
    @Override
    public Iterator<Patch> iterator() {
        return new Iterator<Patch>() {
            private final Iterator<Face> faceIterator = faces.iterator();
            private Iterator<Patch> patchIterator = null;

            @Override
            public boolean hasNext() {
                while ((patchIterator == null || !patchIterator.hasNext())
                        && faceIterator.hasNext()) {
                    patchIterator = faceIterator.next().iterator();
                }
                return patchIterator != null && patchIterator.hasNext();
            }

            @Override
            public Patch next() {
                return patchIterator.next();
            }
        };
    }

    /**
     * Decomposes the surface of the box into {@code npatch} patches that
     * are as nearly equal in size and square as possible.
     *
     * @return the geometry, or null after printing the C reference's error
     *         message when the decomposition fails
     */
    public static Geometry decompose(int npatch, Problem problem) {
        Geometry geometry = new Geometry(problem.box());
        double[] box = problem.box();

        /* Allocate patches to each face, proportionate to area of each face.
         * Each face starts where the previous one ended; only the last patch
         * is forced, so the six ranges always partition [0, npatch). */
        double totalArea = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
        double covered = 0.0;
        int firstPatch = 0;
        for (int iface = 0; iface < 6; iface++) {
            int lastPatch;
            if (iface == 5) {
                lastPatch = npatch - 1;
            } else {
                covered = covered + box[iface] * box[iface + 1];
                lastPatch = (int) ((double) npatch * covered / totalArea + 0.5) - 1;
            }
            geometry.faces.add(new Face(iface, firstPatch, lastPatch,
                    problem.reflectivity(iface), problem.emissivity(iface)));
            firstPatch = lastPatch + 1;
        }

        /* Subdivide each face into its nearly square patches. */
        for (int iface = 0; iface < 6; iface++) {
            Face face = geometry.faces.get(iface);
            int numpat = face.patchCount();
            double depth = 0.0;
            if (iface >= 3) {
                depth = box[iface - 1];
            }
            int numcol = (int) (Math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5);
            if (numcol > numpat) {
                numcol = numpat;
            }
            if (numcol == 0) {
                numcol = 1;
            }
            double width = box[iface] / numcol;
            int rowCursor = numcol - 1;
            double offsetW = 0.0;
            for (int column = 0; column < numcol; column++) {
                int rowOffset = rowCursor / numcol;
                int numrow = (rowCursor + numpat) / numcol - rowOffset;
                if (numrow == 0) {
                    System.out.println("Eccentric box requires more patches.");
                    return null;
                }
                double height = box[iface + 1] / numrow;
                double offsetH = 0.0;
                double area = width * height * (8.0 * Math.PI);
                int lead = face.firstPatch + rowOffset;
                for (int ipatch = lead; ipatch < lead + numrow; ipatch++) {
                    face.addPatch(new Patch(ipatch, offsetW, offsetH, depth,
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
