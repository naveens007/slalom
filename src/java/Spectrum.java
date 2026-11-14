/**
 * An RGB triple stored in an array indexed by {@link Channel#ordinal()}.
 *
 * The optical properties of a face are naturally a small fixed-size vector.
 * Using an array keeps them compact and avoids boxing them in an enum map.
 */
final class Spectrum {

    private final double[] values = new double[Channel.values().length];

    /** Value for a colour channel. */
    double get(Channel channel) {
        return values[channel.ordinal()];
    }

    /** Stores the value for a colour channel. */
    void set(Channel channel, double value) {
        values[channel.ordinal()] = value;
    }
}
