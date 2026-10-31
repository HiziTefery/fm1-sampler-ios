// FM1-DAW design system. One palette, one set of radii and spacings; the FM-1 screen (surface/Surface.cpp) uses the same hues.
// Colours are ARGB. Sizes are dp.
#pragma once
#include <cmath>
#include <cstdint>

namespace theme {
constexpr uint32_t argb(int r, int g, int b, int a = 255) { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
constexpr uint32_t withAlpha(uint32_t c, int a) { return (c & 0x00ffffffu) | (uint32_t)a << 24; }

// Lo-fi look: two base colours (paper and ink, in a dark and a light variant) and one accent. No gradients, 1 px lines, square corners.
// The colours are variables so the phone can switch variant at run time (theme::apply); everything else names them through this header.
inline uint32_t BG, SURFACE, RAISED, OUTLINE, TEXT, DIM, TRACK, ACCENT, ON_ACCENT;   // TRACK: the dim base of bars, rings and progress lines
inline uint32_t VIOLET, GREEN, RED, AMBER;   // old names kept: they now all resolve to paper / accent, so state is told by fill and position, not hue
inline bool light = true;
inline void apply(bool lightVariant) {
    light = lightVariant;
    if (!light) { BG = argb(0, 0, 0); SURFACE = argb(16, 16, 16); RAISED = argb(34, 34, 34); OUTLINE = argb(64, 64, 64); TEXT = argb(255, 255, 255); DIM = argb(106, 106, 106); TRACK = argb(42, 42, 42); ACCENT = argb(255, 106, 31); ON_ACCENT = argb(0, 0, 0); }
    else { BG = argb(241, 239, 233); SURFACE = argb(228, 225, 216); RAISED = argb(214, 211, 201); OUTLINE = argb(168, 165, 154); TEXT = argb(17, 17, 17); DIM = argb(154, 150, 140); TRACK = argb(214, 211, 201); ACCENT = argb(255, 90, 10); ON_ACCENT = argb(255, 255, 255); }
    VIOLET = TEXT; GREEN = TEXT; RED = ACCENT; AMBER = ACCENT;
}
inline const bool kInit = (apply(true), true);   // light by default

constexpr float R_CARD = 2, R_CONTROL = 2, GAP = 6, MARGIN = 8;
constexpr float T_CAPTION = 10, T_BODY = 12, T_VALUE = 15, T_TITLE = 18, T_NUMBER = 22;

// Tracks are told apart by number and position, not hue.
inline uint32_t trackColor(int) { return TEXT; }
}  // namespace theme
