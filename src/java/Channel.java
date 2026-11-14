/**
 * The three colour channels the benchmark solves for, in report order.
 *
 * Used as the key of the per-channel diagonal, right-hand-side and solution
 * vectors (see {@link RadiositySystem}); the declaration order matches the
 * array indices 0, 1, 2 of the C reference.
 */
public enum Channel {
    R,
    G,
    B
}
