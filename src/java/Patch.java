/**
 * One surface element of the box.
 */
public final class Patch {

    /** Global patch number, 0-based (printed 1-based). */
    public final int index;

    /** Position in width, height and depth. */
    private final double[] place = new double[3];

    /** Extent along the width axis. */
    public final double width;

    /** Extent along the height axis. */
    public final double height;

    /** Area of the patch, multiplied by 8 * pi. */
    public final double area;

    Patch(int index, double placeW, double placeH, double placeD,
            double width, double height, double area) {
        this.index = index;
        this.place[0] = placeW;
        this.place[1] = placeH;
        this.place[2] = placeD;
        this.width = width;
        this.height = height;
        this.area = area;
    }

    /** Position along an axis: 0 = width, 1 = height, 2 = depth. */
    public double place(int axis) {
        return place[axis];
    }

    /** Extent along axis 0 (width) or 1 (height). */
    public double extent(int axis) {
        return axis == 0 ? width : height;
    }
}
