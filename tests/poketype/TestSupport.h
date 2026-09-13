/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef POKETYPE_TESTSUPPORT_H
#define POKETYPE_TESTSUPPORT_H

#include <cstdio>

inline int& TestFailures()
{
    static int failures = 0;
    return failures;
}

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond))                                                         \
        {                                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            TestFailures()++;                                                \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        long long va = (long long)(a), vb = (long long)(b);                  \
        if (va != vb)                                                        \
        {                                                                    \
            printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n",                  \
                   __FILE__, __LINE__, #a, #b, va, vb);                      \
            TestFailures()++;                                                \
        }                                                                    \
    } while (0)

int runKeyTableTests();
int runBTKeyboardTests();
int runCartSPITests();
int runSavestateTests();
int runCartSessionTests();

#endif // POKETYPE_TESTSUPPORT_H
