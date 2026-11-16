// SPDX-License-Identifier: GPL-3.0-only
#include "ballistics/ballistics.h"
#include <iostream>
#include <cstdlib>
using namespace ballistics;
void Check(bool condition, const char* name) { if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); } }
int main(int argc, char**) {
    if (argc > 1) {
        // Machine-readable exhaustive parity input for the Python reference test.
        for (int n = 0; n < 4; ++n) for (int m = 0; m < 2; ++m)
            for (int d = 1; d <= 1700; ++d) for (int a : {-200, -1, 0, 1, 200}) {
                auto r = Calculate(static_cast<Nation>(n), static_cast<Mode>(m), d, a);
                std::cout << n << ',' << m << ',' << d << ',' << a << ',' << r.mil << '\n';
            }
        return 0;
    }
    Check(RoundEven(2.5) == 2 && RoundEven(3.5) == 4 && RoundEven(-2.5) == -2 && RoundEven(-3.5) == -4, "Python ties-to-even");
    Check(Calculate(Nation::Germany, Mode::Standard, 500, 0).mil == 883, "German standard");
    Check(Calculate(Nation::USA, Mode::Standard, 500, 999).mil == 883, "STD ignores inclination");
    Check(Calculate(Nation::USSR, Mode::Standard, 500, 0).mil == 1035, "Soviet standard");
    Check(Calculate(Nation::UK, Mode::Standard, 500, 0).mil == 462, "British standard");
    Check(Calculate(Nation::Germany, Mode::SPG, 500, 0).mil == 366, "German SPG");
    Check(Calculate(Nation::USA, Mode::SPG, 500, 0).mil == 300, "US SPG midpoint");
    Check(Calculate(Nation::USSR, Mode::SPG, 500, 20).mil == 280, "SPG inclination");
    Check(Calculate(Nation::UK, Mode::SPG, 500, 0).status == Status::ElevationExceeded, "UK max");
    Check(Calculate(Nation::UK, Mode::SPG, 100, -256).status == Status::Valid, "UK upper boundary");
    Check(Calculate(Nation::UK, Mode::SPG, 100, -257).status == Status::ElevationExceeded, "UK upper exceeded");
    Check(Calculate(Nation::UK, Mode::SPG, 100, 189).status == Status::Valid, "lower boundary");
    Check(Calculate(Nation::UK, Mode::SPG, 100, 190).status == Status::ElevationExceeded, "lower exceeded");
    Check(Calculate(Nation::Germany, Mode::Standard, 1600, 0).status == Status::Valid, "1600 valid");
    Check(Calculate(Nation::Germany, Mode::Standard, 1601, 0).status == Status::RangeExceeded, "1601 invalid");
    Check(Calculate(Nation::Germany, Mode::SPG, 0, 0).status == Status::InvalidDistance, "zero invalid");
    auto rows = ReferenceTable(Nation::Germany, Mode::SPG, 500, 0);
    Check(rows.front().distance == 485 && rows[5].distance == 500 && rows.back().distance == 515, "11 row range");
    for (int i = 1; i < 11; ++i) Check(rows[i].distance - rows[i-1].distance == 3, "3m step");
    rows = ReferenceTable(Nation::Germany, Mode::SPG, 1, 0);
    Check(rows.front().status == Status::InvalidDistance && rows[5].status == Status::Valid, "no negative distance solution");
    Check(Calculate(Nation::Germany, Mode::SPG, 1000001, 0).status == Status::InvalidDistance, "bounded conversion");
    bool threw = false; try { Calculate(static_cast<Nation>(99), Mode::SPG, 1, 0); } catch (...) { threw = true; }
    Check(threw, "invalid enum");
    std::cout << "All ballistics tests passed.\n";
}
