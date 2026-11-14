/**
 * The seven timed phases of the benchmark, in report order.
 */
public enum Task {
    READER("Reader"),
    REGION("Region"),
    SET_UP_1("SetUp1"),
    SET_UP_2("SetUp2"),
    SET_UP_3("SetUp3"),
    SOLVER("Solver"),
    STORER("Storer");

    /** Name as printed in the report table. */
    final String label;

    Task(String label) {
        this.label = label;
    }
}
