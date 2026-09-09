#include "signalquality.h"

extern "C" void *memcpy(void *destination, const void *source, unsigned long size)
{
    unsigned char *out = static_cast<unsigned char *>(destination);
    const unsigned char *in = static_cast<const unsigned char *>(source);
    for (unsigned long i = 0; i < size; ++i)
        out[i] = in[i];
    return destination;
}

extern "C" int runTests()
{
    const double periodicWithSpike[] = {
        1, 2, 18, 21, 20, 19, 2, 1,
        2, 19, 22, 95, 20, 18, 2, 1,
        1, 18, 21, 20, 19, 2, 1, 2
    };
    const RobustSignalResult stable = robustSignalLevel(periodicWithSpike, 24);
    if (!stable.valid || stable.burstCount < 2 || stable.level < 18.0 || stable.level > 24.0)
        return 1;

    const double oneOffNoise[] = {
        1, 2, 1, 2, 90, 86, 2, 1, 2, 1, 2, 1,
        2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1
    };
    if (robustSignalLevel(oneOffNoise, 24).valid)
        return 2;

    const double silence[24] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    if (robustSignalLevel(silence, 24).valid)
        return 3;

    const double shortWindow[] = {
        1, 2, 18, 21, 20, 1, 2, 19,
        95, 20, 1, 2, 1, 2, 1, 1
    };
    const RobustSignalResult shortStable = robustSignalLevel(shortWindow, 16);
    if (!shortStable.valid || shortStable.level < 18.0 || shortStable.level > 24.0)
        return 4;

    return 0;
}
