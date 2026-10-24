#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

void run(const std::string& message, std::function<void ()> func);

extern int g_total;
extern int g_passes;

template<class T> bool equal(const T& a, const T& b)
{
    return a == b;
}

template<> inline bool equal<double>(const double& a, const double& b)
{
    return std::abs(a - b) < 1e-5;
}

template<class T> void test(const std::string& message, const T& expected, const T& actual)
{
    g_total++;
    if (equal(expected, actual)) {
        g_passes++;
    } else {
        std::cerr << "\x1b[31;1mFAIL:\x1b[0m " << message;
        std::cerr << ", expected: " << "\x1b[32;1m" << expected << "\x1b[0m";
        std::cerr << ", actual: " << "\x1b[31;1m" << actual << "\x1b[0m" << std::endl;
    }
}

// Like test(), but accepts a difference of up to `tolerance`: some sizes
// depend on the version of FFmpeg, which started stripping the encoder
// delay of MP3 and AAC files in 7.0.
template<class T> void test_near(
    const std::string& message, const T& expected, const T& actual, const T& tolerance
)
{
    g_total++;
    if (equal(expected, actual) || std::abs(actual - expected) <= tolerance) {
        g_passes++;
    } else {
        std::cerr << "\x1b[31;1mFAIL:\x1b[0m " << message;
        std::cerr << ", expected: " << "\x1b[32;1m" << expected << "\x1b[0m";
        std::cerr << " +/- " << tolerance;
        std::cerr << ", actual: " << "\x1b[31;1m" << actual << "\x1b[0m" << std::endl;
    }
}

void test_audio();
void test_fft();
void test_full_image();
void test_pipeline();
void test_utils();
