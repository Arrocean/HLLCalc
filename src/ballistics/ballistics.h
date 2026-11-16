// SPDX-License-Identifier: GPL-3.0-only
// Formula parameters ported from AimerHLL v6 (AimerSo), Aimer_HLL_v6.py.
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace ballistics {
enum class Nation { Germany, USA, USSR, UK };
enum class Mode { SPG, Standard };
struct Parameters { double standardBase, standardFactor, spgBase, spgFactor; };
inline constexpr std::array<Parameters, 4> parameters{{
    {1002, 0.2375, -77.33333333, -0.88666667},
    {1002, 0.2375, -33.0, -0.665},
    {1141.33, 0.2133, -33.0, -0.665},
    {550.7, 0.1773, -4.0, -1.04}
}};
// Python round(x): ties to even, unlike std::round. No global FP mode changes.
inline int RoundEven(double value) {
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) return static_cast<int>(lower);
    if (fraction > 0.5) return static_cast<int>(lower + 1);
    const int n = static_cast<int>(lower);
    return (n % 2 == 0) ? n : n + 1;
}
enum class Status { Valid, InvalidDistance, RangeExceeded, ElevationExceeded };
struct Result { int distance; int mil; Status status; };
inline Result Calculate(Nation nation, Mode mode, int distance, int inclination) {
    const auto index = static_cast<unsigned>(nation);
    if (index >= parameters.size() || (mode != Mode::SPG && mode != Mode::Standard))
        throw std::invalid_argument("invalid nation or mode");
    // Keep conversions bounded; UI accepts smaller inputs.
    if (distance <= 0 || distance > 1000000 || inclination < -1000000 || inclination > 1000000)
        return {distance, 0, Status::InvalidDistance};
    const auto& p = parameters[index];
    const double value = mode == Mode::SPG
        ? p.spgBase - p.spgFactor * distance - inclination
        : p.standardBase - p.standardFactor * distance;
    const int mil = RoundEven(value);
    const int maxMil = nation == Nation::UK ? 356 : 466;
    const Status status = mode == Mode::SPG
        ? ((mil < -89 || mil > maxMil) ? Status::ElevationExceeded : Status::Valid)
        : (distance > 1600 ? Status::RangeExceeded : Status::Valid);
    return {distance, mil, status};
}
inline std::array<Result, 11> ReferenceTable(Nation nation, Mode mode, int distance, int inclination) {
    std::array<Result, 11> result{};
    if (distance <= 0 || distance > 1000000) {
        for (auto& row : result) row = {0, 0, Status::InvalidDistance};
        return result;
    }
    for (int i = 0; i < 11; ++i) result[i] = Calculate(nation, mode, distance + (i - 5) * 3, inclination);
    return result;
}
}
