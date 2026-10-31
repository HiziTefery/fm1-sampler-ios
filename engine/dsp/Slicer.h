// Finds drum hits in a sample: the start frames of up to `count` slices, sorted. Equal slices when the sample has no clear onsets.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

inline std::vector<int> findSlices(const std::vector<float>& stereo, int frames, int sr, int count = 16) {
    std::vector<int> starts;
    const int hop = std::max(64, sr / 200), bins = frames / hop;
    if (bins >= 8) {
        std::vector<float> e((size_t)bins, 0.f);
        float peak = 1e-6f;
        for (int b = 0; b < bins; b++) {
            float s = 0.f; for (int i = b * hop; i < (b + 1) * hop; i++) s += std::fabs(stereo[(size_t)i * 2]) + std::fabs(stereo[(size_t)i * 2 + 1]);
            e[(size_t)b] = s / (2.f * hop); peak = std::max(peak, e[(size_t)b]);
        }
        std::vector<std::pair<float, int>> cand;   // onset strength, bin
        for (int b = 2; b < bins - 1; b++) {
            float avg = 0.f; int k = 0; for (int j = std::max(0, b - 12); j < b - 1; j++) { avg += e[(size_t)j]; k++; }
            avg = avg / std::max(1, k) + 0.01f * peak;
            const float ratio = e[(size_t)b] / avg;
            if (ratio > 2.5f && e[(size_t)b] > 0.05f * peak && e[(size_t)b] >= e[(size_t)b - 1] && e[(size_t)b] >= e[(size_t)b + 1]) cand.push_back({ratio * e[(size_t)b], b});
        }
        std::sort(cand.begin(), cand.end(), [](auto& a, auto& b) { return a.first > b.first; });
        const int minGap = std::max(1, sr / 15 / hop);   // 65 ms
        for (auto& c : cand) {
            if ((int)starts.size() >= count) break;
            bool near = false; for (int s : starts) if (std::abs(s / hop - c.second) < minGap) near = true;
            if (!near) starts.push_back(std::max(0, (c.second - 1) * hop));
        }
        std::sort(starts.begin(), starts.end());
        if (starts.size() >= 2 && starts[0] > hop * 4) starts.insert(starts.begin(), 0);
    }
    if (starts.size() < 2) { starts.clear(); for (int i = 0; i < count; i++) starts.push_back((int)((int64_t)frames * i / count)); }
    return starts;
}

// The same analysis split in two so that the threshold can move while playing: findOnsets runs once when the sample is loaded (every local energy
// peak that rises above its recent past), pickSlices is cheap and chooses from them: thr 0..1 raises the level a hit must reach (relative to the loudest
// part of the sample) from 2 % to 60 %, in a log scale, so a high threshold keeps only the loud hits.
struct Onset { float ratio, level; int frame; };
inline std::vector<Onset> findOnsets(const std::vector<float>& stereo, int frames, int sr) {
    std::vector<Onset> out;
    const int hop = std::max(64, sr / 200), bins = frames / hop;
    if (bins < 8) return out;
    std::vector<float> e((size_t)bins, 0.f);
    float peak = 1e-6f;
    for (int b = 0; b < bins; b++) {
        float s = 0.f; for (int i = b * hop; i < (b + 1) * hop; i++) s += std::fabs(stereo[(size_t)i * 2]) + std::fabs(stereo[(size_t)i * 2 + 1]);
        e[(size_t)b] = s / (2.f * hop); peak = std::max(peak, e[(size_t)b]);
    }
    for (int b = 2; b < bins - 1; b++) {
        float avg = 0.f; int k = 0; for (int j = std::max(0, b - 12); j < b - 1; j++) { avg += e[(size_t)j]; k++; }
        avg = avg / std::max(1, k) + 0.01f * peak;
        const float ratio = e[(size_t)b] / avg, level = e[(size_t)b] / peak;
        if (ratio > 1.4f && level > 0.03f && e[(size_t)b] >= e[(size_t)b - 1] && e[(size_t)b] >= e[(size_t)b + 1]) out.push_back({ratio, level, std::max(0, (b - 1) * hop)});
    }
    return out;
}
// the start frames of at most `count` slices inside [from, to): equal slices when there are fewer than two hits
inline std::vector<int> pickSlices(const std::vector<Onset>& on, int from, int to, int sr, float thr, int count = 16) {
    std::vector<int> starts;
    thr = std::min(1.f, std::max(0.f, thr));
    const float minRatio = 1.4f + 1.1f * thr, minLevel = 0.02f * std::pow(30.f, thr);
    std::vector<const Onset*> cand;
    for (const Onset& o : on) if (o.frame >= from && o.frame < to && o.ratio >= minRatio && o.level >= minLevel) cand.push_back(&o);
    std::sort(cand.begin(), cand.end(), [](const Onset* a, const Onset* b) { return a->ratio * a->level > b->ratio * b->level; });
    const int minGap = sr / 15;   // 65 ms
    for (const Onset* c : cand) {
        if ((int)starts.size() >= count) break;
        bool near = false; for (int s : starts) if (std::abs(s - c->frame) < minGap) near = true;
        if (!near) starts.push_back(c->frame);
    }
    std::sort(starts.begin(), starts.end());
    if (starts.size() >= 2 && (int)starts.size() < count && starts[0] > from + sr / 50) starts.insert(starts.begin(), from);
    if (starts.size() < 2) { starts.clear(); for (int i = 0; i < count; i++) starts.push_back(from + (int)((int64_t)(to - from) * i / count)); }
    return starts;
}
