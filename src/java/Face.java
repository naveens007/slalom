import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * One face of the box and the patches it was subdivided into.
 */
public final class Face implements Iterable<Patch> {

    /** 0-based face number (printed 1-based). */
    public final int index;

    /** Inclusive global index of this face's first patch. */
    public final int firstPatch;

    /** Inclusive global index of this face's last patch. */
    public final int lastPatch;

    /** Reflectivity per colour channel. */
    public final Spectrum reflectivity;

    /** Emissivity per colour channel. */
    public final Spectrum emissivity;

    private final List<Patch> patches = new ArrayList<Patch>();

    Face(int index, int firstPatch, int lastPatch,
            Spectrum reflectivity, Spectrum emissivity) {
        this.index = index;
        this.firstPatch = firstPatch;
        this.lastPatch = lastPatch;
        this.reflectivity = reflectivity;
        this.emissivity = emissivity;
    }

    /** Adds a patch, in global patch order. */
    void addPatch(Patch patch) {
        patches.add(patch);
    }

    /** Number of patches on this face. */
    public int patchCount() {
        return lastPatch - firstPatch + 1;
    }

    /** The patches of this face, in global patch order. */
    public List<Patch> patches() {
        return patches;
    }

    @Override
    public Iterator<Patch> iterator() {
        return patches.iterator();
    }
}
