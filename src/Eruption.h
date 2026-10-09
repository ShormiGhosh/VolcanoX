// Eruption.h -- the volcano's burst schedule.
//
// The volcano does not smoulder evenly: it BURSTS. A burst is a fast rise (kRise) followed by
// an exponential decay (kDecay). Scheduled bursts start every kPeriod seconds, and every third
// is a big one (30% larger). The viewer can also fire an extra big burst with the E key.
// Everything that reacts to an eruption (the smoke plume, the vent flash, the crater light, the
// lava fountain, the shockwave, the pyroclastic flows, the bombs, the camera shake) is a
// closed-form function of time and of the list of recent burst start times, so nothing is
// stored beyond that list and nothing is pre-recorded.
//
//   scheduled burst b starts at   t_b = kFirst + b * kPeriod
//   envelope(tau)  = tau / kRise                          for 0 <= tau < kRise
//                  = exp(-(tau - kRise) / kDecay)         for tau >= kRise
//   intensity(t)   = sum over the recent bursts of amp * envelope(t - start)
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "MathUtils.h"

namespace Eruption {

constexpr float kPeriod = 9.0f;   // seconds between scheduled bursts
constexpr float kFirst = 1.5f;    // the first scheduled burst
constexpr float kRise = 0.35f;    // seconds from the start of a burst to its peak
constexpr float kDecay = 2.2f;    // e-folding time of the decay after the peak
constexpr int kMaxRecent = 24;

// One burst: when it started, how large it is, a stable id (for hashing), and whether it is
// a big one (big bursts also send pyroclastic flows down the slopes and bombs toward the lake).
struct Burst {
    float start = 0.0f;
    float amp = 1.0f;
    int id = 0;
    bool big = false;
};

// Index of the most recent scheduled burst that has started (-1 before the first).
inline int latestBurst(float t) { return t < kFirst ? -1 : (int)std::floor((t - kFirst) / kPeriod); }
inline float burstStart(int b) { return kFirst + b * kPeriod; }

// Size of scheduled burst b: 0.75..1.0, and every third burst is 30% bigger.
inline float burstAmp(int b) {
    float a = 0.75f + 0.25f * (0.5f + 0.5f * hashNoise(b * 17 + 3));
    return (b % 3 == 2) ? a * 1.3f : a;
}

// Bursts the viewer has fired with the E key (always big).
inline std::vector<Burst>& manualBursts() { static std::vector<Burst> v; return v; }

inline void trigger(float t) {
    static int counter = 0;
    auto& v = manualBursts();
    Burst b;
    b.start = t; b.amp = 1.3f; b.id = 1000 + counter++; b.big = true;
    v.push_back(b);
    if (v.size() > 8) v.erase(v.begin());
}

// Every burst (scheduled and fired) that started at most `window` seconds ago, oldest first.
inline int recent(float t, float window, Burst* out) {
    int n = 0;
    for (int b = latestBurst(t); b >= 0 && n < kMaxRecent; --b) {
        float st = burstStart(b);
        if (t - st > window) break;
        out[n++] = { st, burstAmp(b), b, b % 3 == 2 };
    }
    for (const Burst& m : manualBursts())
        if (m.start <= t && t - m.start <= window && n < kMaxRecent) out[n++] = m;
    for (int i = 1; i < n; ++i) {                       // insertion sort by start time
        Burst key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].start > key.start) { out[j + 1] = out[j]; --j; }
        out[j + 1] = key;
    }
    return n;
}

inline float envelope(float tau) {
    if (tau < 0.0f) return 0.0f;
    return tau < kRise ? tau / kRise : std::exp(-(tau - kRise) / kDecay);
}

// How hard the volcano is erupting right now (0 = quiet, about 1 at a burst's peak).
inline float intensity(float t) {
    Burst b[kMaxRecent];
    int n = recent(t, 14.0f, b);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += b[i].amp * envelope(t - b[i].start);
    return std::min(sum, 1.5f);
}

// Slowly changing amount of smoke hanging over the vent: builds up fast after a burst,
// then thins over several seconds. Drives the size of the ash umbrella.
inline float plume(float t) {
    Burst b[kMaxRecent];
    int n = recent(t, 36.0f, b);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        float tau = t - b[i].start;
        sum += b[i].amp * (1.0f - std::exp(-tau / 0.8f)) * std::exp(-tau / 6.0f);
    }
    return sum;
}

// Camera shake strength: a jolt at the start of every burst that dies away in about a second.
inline float shake(float t) {
    Burst b[kMaxRecent];
    int n = recent(t, 4.0f, b);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += b[i].amp * std::exp(-(t - b[i].start) / 0.55f);
    return std::min(sum, 1.6f);
}

// Seconds until the next scheduled burst starts.
inline float timeToNext(float t) { return burstStart(latestBurst(t) + 1) - t; }

} // namespace Eruption
