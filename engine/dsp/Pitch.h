// Pitch of a sample: YIN on a window taken where the sample is loudest. Returns the fundamental in Hz, or 0 when no clear pitch is found.
#pragma once
#include "Sampler.h"
#include <algorithm>
#include <cmath>
#include <vector>

inline float detectPitchHz(const SampleBuf& b) {
    const int sr = b.sr, n = b.frames;
    if (n < 2048) return 0.f;
    std::vector<float> m((size_t)n);
    for (int i = 0; i < n; i++) m[(size_t)i] = 0.5f * (b.d[2 * (size_t)i] + b.d[2 * (size_t)i + 1]);
    const int W = 2048, tauMax = std::min(W, (int)(sr / 50.f)), tauMin = std::max(2, (int)(sr / 2000.f));
    if (n < W + tauMax + 1) return 0.f;
    // the loudest window inside the first 1.5 s, a little after its start: the note itself, not the attack click
    const int lim = std::min(n - W - tauMax - 1, (int)(1.5f * sr)), hop = 512;
    int best = 0; double be = -1;
    for (int s = 0; s <= lim; s += hop) { double e = 0; for (int i = 0; i < W; i += 4) e += (double)m[(size_t)(s + i)] * m[(size_t)(s + i)]; if (e > be) { be = e; best = s; } }
    best = std::min(lim, best + hop);
    if (be < 1e-6) return 0.f;
    std::vector<float> d((size_t)tauMax + 1, 0.f);
    for (int tau = 1; tau <= tauMax; tau++) { double sum = 0; for (int i = 0; i < W; i++) { const float x = m[(size_t)(best + i)] - m[(size_t)(best + i + tau)]; sum += x * x; } d[(size_t)tau] = (float)sum; }
    float run = 0; d[0] = 1.f;
    for (int tau = 1; tau <= tauMax; tau++) { run += d[(size_t)tau]; d[(size_t)tau] = run > 0 ? d[(size_t)tau] * tau / run : 1.f; }   // cumulative mean normalised difference
    int tau = -1;
    for (int t = tauMin; t < tauMax; t++) if (d[(size_t)t] < 0.15f) { while (t + 1 < tauMax && d[(size_t)t + 1] < d[(size_t)t]) t++; tau = t; break; }
    if (tau < 0) return 0.f;
    float better = (float)tau;
    if (tau > 1 && tau < tauMax) { const float a = d[(size_t)tau - 1], bb = d[(size_t)tau], c = d[(size_t)tau + 1], den = a - 2 * bb + c; if (std::fabs(den) > 1e-9f) better = tau + 0.5f * (a - c) / den; }
    return better > 0 ? (float)sr / better : 0.f;
}
