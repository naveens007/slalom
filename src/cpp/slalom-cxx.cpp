/*
 * slalom-cxx -- C++ implementation of the SLALOM benchmark.
 *
 * STATUS: scaffold only. This establishes the build plumbing, the RAII file
 * handling and the linkage against the shared C machine-report code. None of
 * the numeric phases (Reader, Region, SetUp1/2/3, Solver, Storer, Verify,
 * Meter) are ported yet, so this binary deliberately does NOT print a
 * benchmark result. It is built only when -DSLALOM_ENABLE_CXX=ON, which is
 * off by default so the C reference build stays untouched.
 *
 * The C implementation in src/slalom.c remains the frozen reference: it
 * produces the published MFLOPS numbers and must not be refactored.
 */

#include <cstdio>

#include "file.hpp"
#include "system_info.h"

namespace {

const char *text(const char *s)
{
    return (s != nullptr) ? s : "unknown";
}

} // namespace

int main()
{
    std::printf("slalom-cxx: C++ implementation scaffold (solver not ported)\n");

    /* 'geom' lives next to the binary; the RAII handle closes it for us. */
    const slalom::File geom("geom", "r");
    if (!geom)
        {
            std::fprintf(stderr, "slalom-cxx: 'geom' not found (looked for %s)\n",
                         geom.path().c_str());
            return 1;
        }

    double box[3] = {0.0, 0.0, 0.0};
    int fields = 0;
    for (double &side : box)
        fields += std::fscanf(geom.get(), "%lg", &side);
    if (fields != 3)
        {
            std::fprintf(stderr, "slalom-cxx: must specify exactly 3 box "
                                 "coordinates.\n");
            return 1;
        }

    /* The machine report is still the shared C implementation. Compiling it
     * into this target is what proves the C/C++ mixed linkage works. */
    const MachineInfo *info = get_system_info();
    if (info == nullptr)
        {
            std::fprintf(stderr, "slalom-cxx: system information unavailable.\n");
            return 1;
        }

    std::printf("  geom      : %s\n", geom.path().c_str());
    std::printf("  box       : %g x %g x %g\n", box[0], box[1], box[2]);
    std::printf("  machine   : %s\n", text(info->arch));
    std::printf("  processor : %s\n", text(info->proc));
    std::printf("  compiler  : %s %s\n", text(info->compiler), text(info->version));
    std::printf("  build     : %s\n", SLALOM_BUILD_TYPE);
    std::printf("\nSolver phases (Reader/Region/SetUp1/SetUp2/SetUp3/Solver/"
                "Storer/Verify/Meter) are not ported yet.\n");

    return 0;
}