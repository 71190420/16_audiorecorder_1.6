#ifndef SIGNALQUALITY_H
#define SIGNALQUALITY_H

struct RobustSignalResult {
    bool valid;
    double level;
    double noiseFloor;
    int burstCount;
};

inline RobustSignalResult robustSignalLevel(const double *samples, int count)
{
    RobustSignalResult result = {false, 0.0, 0.0, 0};
    if (!samples || count < 16)
        return result;

    if (count > 64) {
        samples += count - 64;
        count = 64;
    }

    double sorted[64];
    for (int i = 0; i < count; ++i) {
        int j = i;
        while (j > 0 && sorted[j - 1] > samples[i]) {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = samples[i];
    }

    result.noiseFloor = sorted[(count - 1) / 5];
    double gate = result.noiseFloor * 1.5;
    if (gate < result.noiseFloor + 2.0)
        gate = result.noiseFloor + 2.0;
    if (gate < 3.0)
        gate = 3.0;

    double active[64];
    int activeCount = 0;
    int runLength = 0;
    for (int i = 0; i <= count; ++i) {
        if (i < count && samples[i] >= gate) {
            ++runLength;
            continue;
        }
        if (runLength >= 2) {
            ++result.burstCount;
            for (int j = i - runLength; j < i; ++j)
                active[activeCount++] = samples[j];
        }
        runLength = 0;
    }

    if (result.burstCount < 2 || activeCount < 6)
        return result;

    for (int i = 1; i < activeCount; ++i) {
        const double value = active[i];
        int j = i;
        while (j > 0 && active[j - 1] > value) {
            active[j] = active[j - 1];
            --j;
        }
        active[j] = value;
    }

    result.level = active[(activeCount - 1) * 7 / 10];
    result.valid = true;
    return result;
}

#endif // SIGNALQUALITY_H
