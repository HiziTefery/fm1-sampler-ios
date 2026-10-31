#include "SamplerUI.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace theme;

namespace {
float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
std::string num(const char* f, double v) { char b[48]; snprintf(b, sizeof b, f, v); return b; }
const SamplerApp::Page kTabs[] = {SamplerApp::P_HOME, SamplerApp::P_FX, SamplerApp::P_ENV, SamplerApp::P_LFO, SamplerApp::P_ARP, SamplerApp::P_SEQ, SamplerApp::P_MIX, SamplerApp::P_KEY, SamplerApp::P_SAVE, SamplerApp::P_LEN};
const char* kTabName[] = {"SAMPLE", "FX", "ENV", "LFO", "ARP", "TAPE", "MIX", "KEY", "LOOP", "LEN"};
}  // namespace

SamplerUI::SamplerUI(SamplerApp& app) : app_(app) {}

// ---- primitives ----
void SamplerUI::rrect(Rect r, float rad, uint32_t c) { UiOp o; o.type = UiOp::RRECT; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.color = c; ops_.push_back(o); }
void SamplerUI::stroke(Rect r, float rad, uint32_t c, float w) { UiOp o; o.type = UiOp::STROKE; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.f[5] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::text(float x, float cy, float size, uint32_t c, const std::string& s, int align, bool bold) {
    UiOp o; o.type = UiOp::TEXT; o.f[0] = x; o.f[1] = cy + size * 0.36f; o.f[2] = size; o.f[3] = (float)align; o.f[4] = bold; o.color = c; o.text = s; ops_.push_back(o);
}
void SamplerUI::line(float x1, float y1, float x2, float y2, uint32_t c, float w) { UiOp o; o.type = UiOp::LINE; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::circle(float cx, float cy, float r, uint32_t c) { UiOp o; o.type = UiOp::CIRCLE; o.f[0] = cx; o.f[1] = cy; o.f[2] = r; o.color = c; ops_.push_back(o); }
void SamplerUI::tri(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c) { UiOp o; o.type = UiOp::TRI; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = x3; o.f[5] = y3; o.color = c; ops_.push_back(o); }
void SamplerUI::clip(Rect r) { UiOp o; o.type = UiOp::CLIP; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; ops_.push_back(o); }
void SamplerUI::endClip() { UiOp o; o.type = UiOp::ENDCLIP; ops_.push_back(o); }
void SamplerUI::hit(Rect r, std::function<void(float, float)> d, std::function<void(float, float)> m, std::function<void()> u) { hits_.push_back({r, std::move(d), std::move(m), std::move(u)}); }
void SamplerUI::tap(Rect r, std::function<void()> f) { hit(r, [f](float, float) { f(); }); }
std::string SamplerUI::fit(const std::string& s, float width, float size) const {
    const size_t maxc = (size_t)std::max(1.f, width / (size * 0.60f));
    return s.size() <= maxc ? s : s.substr(0, maxc > 1 ? maxc - 1 : 1) + "\xE2\x80\xA6";
}
void SamplerUI::touch(int action, float x, float y, uint32_t now) {
    now_ = now;
    if (action == 0) {
        haveActive_ = false;
        for (size_t i = hits_.size(); i-- > 0;) if (hits_[i].r.has(x, y)) { active_ = hits_[i]; haveActive_ = true; if (active_.down) active_.down(x, y); break; }
    } else if (action == 1) {
        if (haveActive_ && active_.move) active_.move(x, y);
    } else if (haveActive_) {
        if (active_.up) active_.up();
        haveActive_ = false;
    }
}
void SamplerUI::button(Rect r, const std::string& label, bool on, std::function<void()> f, float size, bool accent) {
    rrect(r, R_CONTROL, on ? ACCENT : SURFACE);
    if (!on) stroke(r, R_CONTROL, accent ? ACCENT : OUTLINE, 1);
    text(r.x + r.w / 2, r.y + r.h / 2, size, on ? ON_ACCENT : accent ? ACCENT : TEXT, fit(label, r.w - 6, size), 1, true);
    tap(r, std::move(f));
}

// ---- frame ----
void SamplerUI::frame(uint32_t now) {
    now_ = now;
    st_ = app_.status();
    ops_.clear(); hits_.clear();
    rrect({0, 0, W_, H_}, 0, BG);
    const float tb = 44, left = 176, right = 252;
    if (!editor_) {   // the cassette deck fills the screen; the old sample / page editor stays one tap away (EDIT)
        deck({0, 0, W_, H_});
        if (sheetOpen_) librarySheet();
        if (projectOpen_) projectSheet();
        return;
    }
    topBar({0, 0, W_, tb});
    trackList({MARGIN, tb + MARGIN, left - MARGIN, H_ - tb - 2 * MARGIN});
    sampleArea({left + MARGIN, tb + MARGIN, W_ - left - right - 2 * MARGIN, H_ - tb - 2 * MARGIN});
    pagePanel({W_ - right, tb + MARGIN, right - MARGIN, H_ - tb - 2 * MARGIN});
    if (sheetOpen_) librarySheet();
}

void SamplerUI::topBar(Rect r) {
    rrect(r, 0, SURFACE);
    line(0, r.h, W_, r.h, OUTLINE, 1);
    const float cy = r.h / 2;
    float x = MARGIN;
    // PLAY / STOP
    Rect pl{x, 6, 44, r.h - 12};
    rrect(pl, R_CONTROL, RAISED);
    if (st_.playing) { rrect({pl.x + 15, cy - 8, 5, 16}, 0, ACCENT); rrect({pl.x + 24, cy - 8, 5, 16}, 0, ACCENT); }
    else tri(pl.x + 16, cy - 9, pl.x + 16, cy + 9, pl.x + 30, cy, TEXT);
    tap(pl, [this] { app_.transportPlay(); });
    x += 52;
    // REC: arms the first take on a clean looper, overdub afterwards
    Rect rc{x, 6, 86, r.h - 12};
    const bool recOn = st_.freeRecording || st_.track_clip_rec[app_.track()], armed = st_.freeArmed;
    rrect(rc, R_CONTROL, recOn ? ACCENT : RAISED);
    if (armed) stroke(rc, R_CONTROL, ACCENT, 2);
    circle(rc.x + 18, cy, 6, recOn ? ON_ACCENT : ACCENT);
    text(rc.x + 28, cy, 9.5f, recOn ? ON_ACCENT : TEXT, app_.loopExists() ? "OVERDUB" : "REC", 0, true);
    tap(rc, [this] { app_.loopRecord(); });
    x += 94;
    button({x, 6, 56, r.h - 12}, "SAVE", false, [this] { app_.loopSave(); }, 11);
    x += 62;
    button({x, 6, 56, r.h - 12}, "CLEAR", false, [this] { app_.loopClear(); }, 11);
    x += 62;
    button({x, 6, 56, r.h - 12}, "DECK", false, [this] { editor_ = false; }, 11);
    x += 70;
    text(x, cy - 6, 12, st_.freeRecording || recOn || armed ? ACCENT : TEXT, app_.loopText(), 0, true);
    text(x, cy + 11, 9.5f, DIM, std::string("LOOP SOURCE: ") + (app_.knobs(SamplerApp::P_SAVE)[1].value), 0, false);
    tap({x - 4, 0, 200, r.h}, [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
    // link state and load
    const uint32_t lc = linkMidi_ && linkAudio_ ? TEXT : linkMidi_ ? ACCENT : DIM;
    circle(W_ - 22, cy, 5, lc);
    text(W_ - 34, cy - 6, 10, DIM, linkMidi_ ? "FM-1" : "NO FM-1", 2, true);
    text(W_ - 34, cy + 9, 9, DIM, "CPU " + num("%.0f", st_.cpu_permille / 10.0) + "%", 2, false);
}

// ---- cassette deck ----
// A tape deck front panel: smoked cassette in the middle with turning reels, analogue VU needles and knobs at its sides, the eight loops as
// tape strips that scroll under a fixed head (OP-1 style) and the transport keys. Its colours are its own (they do not follow the light/dark theme).
namespace {
constexpr float kPi = 3.14159265f;
constexpr uint32_t kPanel = theme::argb(26, 26, 31), kPanelHi = theme::argb(52, 52, 60), kMetal = theme::argb(150, 148, 140), kShell = theme::argb(46, 44, 52),
    kShellHi = theme::argb(74, 72, 82), kGlass = theme::argb(14, 13, 17), kReel = theme::argb(232, 230, 222), kTape = theme::argb(86, 58, 40), kTapeHi = theme::argb(120, 84, 58),
    kPaper = theme::argb(237, 232, 218), kInk = theme::argb(28, 26, 30), kRed = theme::argb(206, 64, 44), kAmber = theme::argb(232, 163, 61);
}

// Tape pack of radius r with its winding rings, a white hub with six teeth turned by `ang`.
void SamplerUI::reel(float cx, float cy, float r, float ang) {
    circle(cx, cy, r, kTape);
    for (float rr = 22; rr < r - 1; rr += 4) stroke({cx - rr, cy - rr, 2 * rr, 2 * rr}, rr, rr < r - 5 ? theme::argb(70, 46, 32) : kTapeHi, 1);
    stroke({cx - r, cy - r, 2 * r, 2 * r}, r, kTapeHi, 1.5f);
    circle(cx, cy, 21, kReel);
    circle(cx, cy, 15, kGlass);
    for (int k = 0; k < 6; k++) {
        const float a = ang + k * kPi / 3;
        line(cx + 15 * std::cos(a), cy + 15 * std::sin(a), cx + 9 * std::cos(a), cy + 9 * std::sin(a), kReel, 3.5f);
    }
    circle(cx, cy, 3, kMetal);
}

// Analogue VU meter: cream face, scale with a red end, needle at v (0..1).
void SamplerUI::vu(Rect r, float v, const std::string& name) {
    rrect({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 7, kMetal);
    rrect(r, 5, kPaper);
    const float px = r.x + r.w / 2, py = r.y + r.h * 1.02f, R = r.h * 0.9f;
    clip(r);
    for (int j = 0; j <= 20; j++) {
        const float a = (-48.f + j * 4.8f) * kPi / 180, major = j % 4 == 0 ? 1.f : 0.f;
        const float l = major ? 10.f : 5.f;
        line(px + std::sin(a) * (R - l), py - std::cos(a) * (R - l), px + std::sin(a) * R, py - std::cos(a) * R, j >= 14 ? kRed : kInk, major ? 1.6f : 1.f);
        if (major) text(px + std::sin(a) * (R - 19), py - std::cos(a) * (R - 19), 8, j >= 14 ? kRed : kInk, std::to_string(j / 4 * 2 - 10 + (j >= 16 ? 0 : 0)).substr(0, 3), 1, true);
    }
    text(px + 34, r.y + r.h * 0.74f, 11, kInk, "VU", 1, true);
    text(r.x + 8, r.y + r.h - 9, 8, kInk, name, 0, true);
    const float a = (-48.f + clamp01(v) * 96.f) * kPi / 180;
    line(px, py, px + std::sin(a) * (R - 2), py - std::cos(a) * (R - 2), kInk, 1.6f);
    circle(px, py, 9, kInk);
    endClip();
}

// A rotary knob on a knob of the app (drag up / down to turn).
void SamplerUI::knob(float cx, float cy, SamplerApp::Page page, int idx, bool selectedTrackAccent) {
    auto k = app_.knobs(page);
    const auto& p = k[(size_t)idx];
    if (!p.valid) return;
    const float R = 17, n = clamp01(p.norm);
    for (int j = 0; j <= 10; j++) { const float a = (-135.f + j * 27.f) * kPi / 180; circle(cx + std::sin(a) * (R + 6), cy - std::cos(a) * (R + 6), 1.2f, j / 10.f <= n ? kAmber : kPanelHi); }
    circle(cx, cy, R + 1, kMetal);
    circle(cx, cy, R, kShell);
    circle(cx - 2, cy - 3, R - 5, kShellHi);
    const float a = (-135.f + n * 270.f) * kPi / 180;
    line(cx + std::sin(a) * 4, cy - std::cos(a) * 4, cx + std::sin(a) * (R - 2), cy - std::cos(a) * (R - 2), kAmber, 2.5f);
    text(cx, cy + R + 16, 8.5f, kMetal, fit(p.name, 100, 8.5f), 1, true);
    text(cx, cy + R + 27, 9.5f, kPaper, p.value, 1, true);
    Rect hr{cx - 30, cy - R - 10, 60, 2 * R + 40};
    hit(hr, [this, n](float, float y) { dragY_ = y; dragV_ = n; },
        [this, page, idx](float, float y) { app_.knobs(page)[(size_t)idx].set(clamp01(dragV_ + (dragY_ - y) / 110.f)); });
}

void SamplerUI::cassette(Rect c) {
    const int t = app_.track();
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const bool recNow = st_.freeRecording || st_.track_clip_rec[t];
    const int mic = app_.micState();
    // shell: smoked plastic, a lighter edge, four screws
    rrect({c.x - 3, c.y - 3, c.w + 6, c.h + 6}, 15, recNow ? kAmber : kMetal);
    rrect(c, 13, kShell);
    stroke({c.x + 3, c.y + 3, c.w - 6, c.h - 6}, 11, kShellHi, 1);
    for (int i = 0; i < 4; i++) { const float sx = c.x + (i & 1 ? c.w - 12 : 12), sy = c.y + (i & 2 ? c.h - 12 : 12); circle(sx, sy, 4, kMetal); line(sx - 2.5f, sy - 2.5f, sx + 2.5f, sy + 2.5f, kShell, 1.2f); }
    // paper label
    Rect lb{c.x + 24, c.y + 14, c.w - 48, c.h * 0.31f};
    rrect(lb, 3, kPaper);
    rrect({lb.x, lb.y, lb.w, 8}, 0, kAmber); rrect({lb.x, lb.y + 8, lb.w, 3}, 0, kRed);
    std::string title = app_.sampleLabel(t), sub = "TRACK " + std::to_string(t + 1) + "  " + SamplerApp::algoName(app_.algo());
    if (title.empty()) title = "EMPTY TRACK";
    if (mic == 1) { title = "WAITING FOR SOUND"; sub = "MICROPHONE  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())); }
    else if (mic == 2) { title = "RECORDING " + std::to_string(st_.sampleSeconds).substr(0, 4) + " S"; sub = "MICROPHONE"; }
    else if (mic == 3) title = "SAVING";
    const std::string ts = app_.toast();
    if (!ts.empty()) sub = ts;
    text(lb.x + 26, lb.y + 24, 8.5f, kInk, fit(sub, lb.w - 52, 8.5f), 0, true);
    text(lb.x + 26, lb.y + 41, 16, kInk, fit(title, lb.w - 52, 16), 0, true);
    for (int i = 0; i < 2; i++) line(lb.x + 26, lb.y + lb.h - 11 + i * 0.f - i * 7, lb.x + lb.w - 26, lb.y + lb.h - 11 - i * 7, theme::argb(170, 165, 150), 1);
    // library arrows on the label, tap the label for the list
    tri(lb.x + 14, lb.y + 33, lb.x + 6, lb.y + 41, lb.x + 14, lb.y + 49, kInk);
    tri(lb.x + lb.w - 14, lb.y + 33, lb.x + lb.w - 6, lb.y + 41, lb.x + lb.w - 14, lb.y + 49, kInk);
    tap({lb.x + 26, lb.y, lb.w - 52, lb.h}, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    tap({lb.x, lb.y + 10, 26, lb.h - 10}, [this] { app_.browse(-1); });
    tap({lb.x + lb.w - 26, lb.y + 10, 26, lb.h - 10}, [this] { app_.browse(1); });
    // counter
    float lsec = 0, psec = 0, lp = 0;
    if (hasLoop) { lsec = (float)(st_.loopSamples / app_.session().sampleRate()); lp = loopPos(); psec = lp * lsec; }
    else if (st_.freeRecording) psec = lsec = st_.freeSeconds;
    char cnt[48]; snprintf(cnt, sizeof cnt, "%d:%04.1f / %d:%04.1f", (int)psec / 60, std::fmod(psec, 60.f), (int)lsec / 60, std::fmod(lsec, 60.f));
    text(lb.x + lb.w - 28, lb.y + 24, 9, kInk, hasLoop || st_.freeRecording ? cnt : "NO LOOP", 2, true);
    if (recNow || st_.freeArmed) { if (recNow || (now_ / 400) % 2 == 0) circle(lb.x + 40, lb.y + lb.h - 22, 4.5f, kRed); }
    // window with the reels
    Rect wd{c.x + c.w * 0.1f, lb.y + lb.h + 10, c.w * 0.8f, c.h - (lb.h + 14) - 10 - 44};
    rrect({wd.x - 2, wd.y - 2, wd.w + 4, wd.h + 4}, 13, kShellHi);
    rrect(wd, 12, kGlass);
    const float cy = wd.y + wd.h / 2, rmax = wd.h / 2 - 2, rmin = 26;
    float p = hasLoop ? lp : std::min(1.f, st_.freeSeconds / 60.f), ang = hasLoop ? -lp * 2 * kPi * 6 : st_.freeRecording ? -(float)now_ * 0.004f : 0.f;
    if (!hasLoop && !st_.freeRecording) p = 0.5f;
    const float rl = rmin + (rmax - rmin) * (1 - p), rr = rmin + (rmax - rmin) * p;
    const float lx = wd.x + wd.w * 0.27f, rx = wd.x + wd.w * 0.73f;
    clip(wd);
    reel(lx, cy, rl, ang); reel(rx, cy, rr, ang);
    // tape: from the packs to the bottom guides, a sheen on the glass
    line(lx - rl * 0.6f, cy + rl * 0.8f, wd.x + wd.w * 0.36f, wd.y + wd.h, kTape, 3);
    line(rx + rr * 0.6f, cy + rr * 0.8f, wd.x + wd.w * 0.64f, wd.y + wd.h, kTape, 3);
    tri(wd.x + wd.w * 0.5f, wd.y, wd.x + wd.w * 0.74f, wd.y, wd.x + wd.w * 0.30f, wd.y + wd.h, theme::withAlpha(0xffffff, 14));
    endClip();
    // head plate with rollers and the tape across the head
    Rect pl{c.x + c.w * 0.2f, c.y + c.h - 38, c.w * 0.6f, 30};
    rrect(pl, 7, kShellHi); rrect({pl.x + 2, pl.y + 2, pl.w - 4, pl.h - 4}, 6, kGlass);
    const float ry = pl.y + 9;
    line(pl.x + 14, ry, pl.x + pl.w - 14, ry, hasLoop && st_.playing ? kTapeHi : kTape, 3);
    circle(pl.x + 14, ry, 5, kMetal); circle(pl.x + pl.w - 14, ry, 5, kMetal);
    rrect({pl.x + pl.w / 2 - 12, pl.y + 3, 24, 9}, 2, kMetal);
    circle(pl.x + pl.w / 2 - 20, pl.y + 21, 3, kShellHi); circle(pl.x + pl.w / 2 + 20, pl.y + 21, 3, kShellHi);
}

float SamplerUI::loopPos() const {
    const int t = app_.track();
    if (!(app_.loopExists() && st_.loopSamples > 0)) return 0.f;
    return st_.track_has_clip[t] ? st_.clipPos[t] : (float)(st_.pos / st_.loopSamples);
}

// The eight loops as tape strips. The tape runs under a fixed head in the middle: the loop is drawn repeated, shifted by the position.
void SamplerUI::tapeStrip(Rect r) {
    rrect({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 5, kMetal);
    rrect(r, 3, kGlass);
    const float lane = (r.h - 6 - 7 * 2) / 8, lbl = 22, tx = r.x + lbl, tw = r.w - lbl - 4, cxh = tx + tw / 2;
    const float lp = loopPos(), Lpx = tw * 0.7f;
    float mx = 0.08f; float pk[8][Session::kLoopBins]; int n[8];
    for (int i = 0; i < 8; i++) { n[i] = app_.session().loopPeaks(i, pk[i], Session::kLoopBins); for (int j = 0; j < n[i]; j++) mx = std::max(mx, pk[i][j]); }
    for (int i = 0; i < 8; i++) {
        const float y = r.y + 3 + i * (lane + 2), cy = y + lane / 2;
        const bool sel = i == app_.track(), rec = st_.track_clip_rec[i];
        rrect({r.x + 3, y, r.w - 6, lane}, 2, sel ? kPanelHi : kPanel);
        text(r.x + 12, cy, 8.5f, sel ? kAmber : kMetal, std::to_string(i + 1), 1, true);
        tap({r.x, y - 1, r.w, lane + 2}, [this, i] { app_.selectTrack(i); });
        clip({tx, y, tw, lane});
        rrect({tx, cy - 1, tw, 2}, 0, kTape);   // the tape itself
        if (n[i] > 0) {
            const int cols = std::max(1, (int)(Lpx / 1.5f)), per = std::max(1, n[i] / cols), cnt = (n[i] + per - 1) / per;
            const float bw = Lpx / cnt;
            for (int k = -2; k <= 2; k++)
                for (int a = 0; a < cnt; a++) {
                    const float x = cxh + (a * bw - lp * Lpx) + k * Lpx;
                    if (x + bw < tx || x > tx + tw) continue;
                    float v = 0; for (int j = a * per; j < std::min(n[i], (a + 1) * per); j++) v = std::max(v, pk[i][j]);
                    v = std::min(1.f, v / mx);
                    if (v > 0.02f) rrect({x, cy - v * lane / 2, std::max(1.f, bw - 0.3f), std::max(1.f, v * lane)}, 0, rec ? kAmber : sel ? kReel : theme::argb(170, 168, 160));
                }
            if (st_.freeRecording || rec) {}   // the take is drawn once it exists
        }
        endClip();
    }
    // the head: fixed line in the middle with a marker
    line(cxh, r.y + 1, cxh, r.y + r.h - 1, st_.freeRecording || st_.track_clip_rec[app_.track()] ? kRed : kAmber, 1.5f);
    tri(cxh - 5, r.y - 6, cxh + 5, r.y - 6, cxh, r.y + 1, kAmber);
}

void SamplerUI::key(Rect r, const std::string& label, int icon, bool on, std::function<void()> f, bool hot) {
    rrect({r.x, r.y + 2, r.w, r.h}, 5, theme::argb(10, 10, 12));   // the key's shadow
    rrect(r, 5, on ? kAmber : kPanelHi);
    rrect({r.x + 2, r.y + 2, r.w - 4, 3}, 1, on ? theme::argb(255, 205, 120) : theme::argb(84, 84, 94));
    const uint32_t ink = on ? kInk : kPaper;
    const float cx = r.x + r.w / 2, cy = r.y + r.h / 2 - 5;
    if (icon == 1) tri(cx - 5, cy - 7, cx - 5, cy + 7, cx + 7, cy, ink);                               // play
    else if (icon == 2) { rrect({cx - 6, cy - 7, 4.5f, 14}, 0, ink); rrect({cx + 2, cy - 7, 4.5f, 14}, 0, ink); }   // pause
    else if (icon == 3) circle(cx, cy, 7, hot ? kRed : kRed);                                           // record
    else if (icon == 4) rrect({cx - 6, cy - 6, 12, 12}, 1, ink);                                        // stop / clear
    text(cx, r.y + r.h - 9, 8.5f, ink, label, 1, true);
    tap(r, std::move(f));
}

void SamplerUI::deck(Rect r) {
    // panel: brushed dark metal, a bevel, four screws
    rrect({0, 0, W_, H_}, 0, kPanel);
    stroke({3, 3, W_ - 6, H_ - 6}, 6, kPanelHi, 1.5f);
    for (int i = 0; i < 4; i++) { const float sx = i & 1 ? W_ - 11 : 11, sy = i & 2 ? H_ - 11 : 11; circle(sx, sy, 3.5f, kMetal); line(sx - 2, sy - 2, sx + 2, sy + 2, kPanel, 1); }
    // header
    const bool recOn = st_.freeRecording || st_.track_clip_rec[app_.track()], armed = st_.freeArmed;
    text(24, 18, 11, kMetal, "FM-1 SAMPLER  8 TRACK", 0, true);
    text(W_ / 2, 18, 11, recOn || armed ? kAmber : kPaper, app_.loopText() + "   SRC " + std::string(app_.knobs(SamplerApp::P_SAVE)[1].value), 1, true);
    tap({W_ / 2 - 150, 4, 300, 28}, [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
    circle(W_ - 24, 18, 4, linkMidi_ && linkAudio_ ? kReel : linkMidi_ ? kAmber : kShellHi);
    text(W_ - 34, 18, 9.5f, kMetal, std::string(linkMidi_ ? "FM-1  " : "NO FM-1  ") + "CPU " + num("%.0f", st_.cpu_permille / 10.0) + "%", 2, true);
    // centre: the cassette, VU needles and knobs on both sides
    const float top = 34, ch = 214, cw = std::min(ch * 1.6f, W_ * 0.42f);
    const float cxl = 28, zr = W_ - 24, x0 = cxl + cw + 26, w0 = zr - x0;
    cassette({cxl, top, cw, ch});
    // right side: the VU needles (master, selected track), the selected page and its four knobs in a line
    auto smooth = [](float& vu, float peak) { const float db = peak > 1e-4f ? 20 * std::log10(peak) : -80.f; const float t = clamp01((db + 38.f) / 41.f); vu += (t - vu) * (t > vu ? 0.55f : 0.14f); return vu; };
    const float vl = smooth(vu_[0], st_.master_peak), vr = smooth(vu_[1], st_.track_peak[app_.track()]);
    const float vw = (w0 - 14) / 2, vh = 72;
    vu({x0, top + 4, vw, vh}, vl, "MASTER");
    vu({x0 + vw + 14, top + 4, vw, vh}, vr, "TRACK " + std::to_string(app_.track() + 1));
    const float py = top + vh + 28;
    text(x0, py, 9, kMetal, "PAGE", 0, true);
    text(x0 + 36, py, 15, kAmber, SamplerApp::pageName(app_.page()), 0, true);
    for (int i = 0; i < 10; i++) {   // the pages as notches: tap one to go there (the FM-1 buttons do the same)
        Rect nr{zr - (10 - i) * 22.f, py - 11, 22, 22};
        rrect({nr.x + 3, py - 3, 16, 6}, 1, kTabs[i] == app_.page() ? kAmber : kPanelHi);
        tap(nr, [this, i] { app_.gotoPage(kTabs[i]); });
    }
    const float ky = py + 52;
    for (int i = 0; i < 4; i++) knob(x0 + w0 * (i + 0.5f) / 4, ky, app_.page(), i);
    // the loops
    tapeStrip({24, top + ch + 16, W_ - 48, 84});
    // transport keys
    const float ty = H_ - 46, kw = std::min(84.f, (W_ - 60) / 9.f), gap = 6;
    struct K { const char* l; int icon; bool on; std::function<void()> f; };
    const int ms = app_.micState();
    static const char* micLab[] = {"MIC", "WAITING", "STOP MIC", "SAVING"};
    std::vector<K> ks = {
        {st_.playing ? "PAUSE" : "PLAY", st_.playing ? 2 : 1, st_.playing, [this] { app_.transportPlay(); }},
        {app_.loopExists() ? "OVERDUB" : "REC", 3, recOn, [this] { app_.loopRecord(); }},
        {micLab[ms], 0, ms == 2, [this] { app_.micSample(); }},
        {"FILE", 0, false, [this] { app_.requestFile(); }},
        {"LIBRARY", 0, false, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); }},
        {"PROJECT", 0, projectOpen_, [this] { projectOpen_ = true; clearArmed_ = 0; }},
        {"EDIT", 0, false, [this] { editor_ = true; }},
    };
    const float total = ks.size() * kw + (ks.size() - 1) * gap;
    for (size_t i = 0; i < ks.size(); i++) {
        Rect kr{(W_ - total) / 2 + i * (kw + gap), ty, kw, 36};
        key(kr, ks[i].l, ks[i].icon, ks[i].on, ks[i].f, false);
        if (i == 1 && armed) stroke(kr, 5, kAmber, 2);
    }
}

void SamplerUI::trackList(Rect r) {
    const float rh = (r.h - 7 * 4) / 8;
    for (int i = 0; i < SamplerApp::kTracks; i++) {
        Rect row{r.x, r.y + i * (rh + 4), r.w, rh};
        const bool sel = i == app_.track();
        rrect(row, R_CARD, sel ? RAISED : SURFACE);
        stroke(row, R_CARD, sel ? ACCENT : OUTLINE, sel ? 2 : 1);
        text(row.x + 14, row.y + rh / 2, T_TITLE, sel ? ACCENT : TEXT, std::to_string(i + 1), 1, true);
        const std::string lab = app_.sampleLabel(i);
        text(row.x + 30, row.y + rh / 2 - 6, 11, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY" : lab, r.w - 40, 11), 0, true);
        // level and loop mark
        const float lv = std::min(1.f, st_.track_peak[i] * 1.5f);
        rrect({row.x + 30, row.y + rh - 12, r.w - 44, 3}, 0, OUTLINE);
        rrect({row.x + 30, row.y + rh - 12, (r.w - 44) * lv, 3}, 0, TEXT);
        if (st_.track_clip_rec[i]) circle(row.x + r.w - 10, row.y + 10, 4, ACCENT);
        else if (st_.track_has_clip[i]) circle(row.x + r.w - 10, row.y + 10, 3.5f, TEXT);
        tap(row, [this, i] { app_.selectTrack(i); });
    }
}

void SamplerUI::sampleArea(Rect r) {
    const int t = app_.track();
    const auto ts = app_.state(t);
    const std::string lab = app_.sampleLabel(t);
    text(r.x, r.y + 12, T_TITLE, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY TRACK: PICK A SAMPLE" : lab, r.w - 120, T_TITLE), 0, true);
    text(r.x + r.w, r.y + 12, T_CAPTION, DIM, "TRACK " + std::to_string(t + 1), 2, true);
    // algorithm
    const float cw = (r.w - 5 * 4) / 6;
    for (int a = 0; a < 6; a++) {
        Rect c{r.x + a * (cw + 4), r.y + 30, cw, 28};
        button(c, a == 5 ? "PAUL" : SamplerApp::algoName(a), ts.algo == a, [this, a] { app_.setAlgo(a); }, 9.5f);
    }
    // waveform with the playback window
    Rect w{r.x, r.y + 66, r.w, 118};
    rrect(w, R_CARD, SURFACE); stroke(w, R_CARD, OUTLINE, 1);
    const int bins = std::max(20, (int)(w.w / 3));
    if (app_.micState() == 1 || app_.micState() == 2) {   // microphone: input level + trigger threshold while waiting, the growing take while recording
        const float cy0 = w.y + w.h / 2, half = w.h / 2 - 8;
        if (app_.micState() == 1) {
            const float thr = std::min(1.f, app_.micThreshold() * 2.f), lv = std::min(1.f, st_.inputPeak * 2.f);
            rrect({w.x + 8, cy0 - lv * half, w.w - 16, std::max(2.f, lv * half * 2)}, 0, lv > thr ? ACCENT : OUTLINE);
            line(w.x + 8, cy0 - thr * half, w.x + w.w - 8, cy0 - thr * half, ACCENT, 1);
            line(w.x + 8, cy0 + thr * half, w.x + w.w - 8, cy0 + thr * half, ACCENT, 1);
            text(w.x + w.w / 2, w.y + w.h - 12, T_CAPTION, DIM, "WAITING FOR SOUND ABOVE THE LINES  DRAG TO SET  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), 1, false);
            auto setThr = [this, half, cy0](float, float y) { app_.setMicThreshold01(std::log10(std::max(0.0025f, std::fabs(y - cy0) / half / 2.f) / 0.005f) / 2.f); };
            hit(w, setThr, setThr);
        } else {
            float live[2048]; const int n = app_.session().livePeaks(live, 2048);
            const int cols = std::max(20, (int)(w.w / 3)), per = std::max(1, (n + cols - 1) / cols);   // longer takes are squeezed to fit
            const float bw0 = (w.w - 16) / std::max(cols, (n + per - 1) / per);
            for (int i = 0; i * per < n; i++) {
                float v = 0; for (int j = i * per; j < std::min(n, (i + 1) * per); j++) v = std::max(v, live[j]);
                v = std::min(1.f, v * 1.1f);
                rrect({w.x + 8 + i * bw0, cy0 - std::max(1.f, v * half), std::max(1.f, bw0 - 1), std::max(2.f, v * half * 2)}, 0, TEXT);
            }
            text(w.x + w.w - 24, w.y + 16, T_CAPTION, ACCENT, "REC " + std::to_string(st_.sampleSeconds).substr(0, 4) + "S", 2, true);
        }
        goto after_wave;
    }
    {
    auto pk = app_.peaks(t, bins);
    const float cy = w.y + w.h / 2, bw = w.w / bins;
    for (int i = 0; i < (int)pk.size(); i++) {
        const float pos = (i + 0.5f) / bins, v = std::min(1.f, pk[(size_t)i] * 1.1f);
        const bool in = pos >= ts.start && pos <= ts.end;
        rrect({w.x + i * bw + 0.5f, cy - std::max(1.f, v * (w.h / 2 - 8)), std::max(1.f, bw - 1), std::max(2.f, v * (w.h - 16))}, 0, in ? TEXT : OUTLINE);
    }
    if (pk.empty()) text(w.x + w.w / 2, cy, T_BODY, DIM, "NO SAMPLE", 1, false);
    for (int j = 0; j < st_.voiceN[t]; j++) {   // where the sounding voices are reading
        const float vx = w.x + 2 + st_.voicePos[t][j] * (w.w - 4);
        line(vx, w.y + 22, vx, w.y + w.h - 10, ACCENT, 1.5f);
        circle(vx, w.y + w.h - 10, 3.5f, ACCENT);
    }
    }
    after_wave:
    // start / end handles (not while the microphone take is shown)
    for (int h = 0; h < (app_.micState() == 1 || app_.micState() == 2 ? 0 : 2); h++) {
        const float v = h ? ts.end : ts.start, hx = w.x + 2 + v * (w.w - 4);
        line(hx, w.y + 4, hx, w.y + w.h - 4, ACCENT, 2);
        rrect({hx - (h ? 10 : 0), w.y + 4, 10, 14}, 1, ACCENT);
        hit({hx - 18, w.y, 36, w.h},
            [](float, float) {},
            [this, h, w](float x, float) { auto k = app_.knobs(SamplerApp::P_HOME); k[(size_t)h].set(clamp01((x - w.x - 2) / (w.w - 4))); });
    }
    // loop strip under the waveform
    Rect ls{r.x, w.y + w.h + 6, r.w, 34};
    rrect(ls, 0, SURFACE); stroke(ls, 0, OUTLINE, 1);
    {   // the whole loop: every track mixed (dim) and the selected track's own loop on top
        float mixp[400], trp[400]; const int ln = app_.session().loopPeaks(-1, mixp, 400); app_.session().loopPeaks(t, trp, 400);
        if (ln > 0) {
            float mx = 0.08f; for (int i = 0; i < ln; i++) mx = std::max(mx, mixp[i]);
            const float lcy = ls.y + ls.h / 2, lh = ls.h / 2 - 3, lbw = (ls.w - 4) / ln;
            for (int i = 0; i < ln; i++) {
                const float v = std::min(1.f, mixp[i] / mx), u = std::min(1.f, trp[i] / mx);
                if (v > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - v * lh, std::max(1.f, lbw), std::max(1.f, 2 * v * lh)}, 0, OUTLINE);
                if (u > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - u * lh, std::max(1.f, lbw), std::max(1.f, 2 * u * lh)}, 0, TEXT);
            }
        } else text(ls.x + ls.w / 2, ls.y + ls.h / 2, T_CAPTION, DIM, "NO LOOP: PRESS REC", 1, false);
    }
    const float lp = st_.track_has_clip[t] ? st_.clipPos[t] : (app_.loopExists() && st_.loopSamples > 0 ? (float)(st_.pos / st_.loopSamples) : 0.f);
    if (app_.loopExists() || st_.freeRecording) rrect({ls.x + lp * (ls.w - 4), ls.y - 2, 3, ls.h + 4}, 0, st_.freeRecording || st_.track_clip_rec[t] ? ACCENT : TEXT);
    // library row
    Rect lr{r.x, ls.y + ls.h + 12, r.w, 34};
    const auto& lib = app_.library();
    const int cur = app_.libraryCursor();
    button({lr.x, lr.y, 34, lr.h}, "<", false, [this] { app_.browse(-1); }, 15);
    button({lr.x + lr.w - 34 - 0, lr.y, 34, lr.h}, ">", false, [this] { app_.browse(1); }, 15);
    Rect nm{lr.x + 40, lr.y, lr.w - 80, lr.h};
    rrect(nm, R_CARD, SURFACE); stroke(nm, R_CARD, OUTLINE, 1);
    if (!lib.empty()) {
        text(nm.x + 10, nm.y + nm.h / 2 - 6, 12, TEXT, fit(lib[(size_t)cur].label, nm.w - 80, 12), 0, true);
        text(nm.x + 10, nm.y + nm.h / 2 + 9, 9, DIM, lib[(size_t)cur].group + "  " + std::to_string(cur + 1) + "/" + std::to_string(lib.size()), 0, false);
    }
    text(nm.x + nm.w - 10, nm.y + nm.h / 2, 9.5f, ACCENT, "LIBRARY", 2, true);
    tap(nm, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    // microphone and disk
    Rect br{r.x, lr.y + lr.h + 8, r.w, 38};
    const int ms = app_.micState();
    static const char* micLab[] = {"RECORD FROM MIC", "WAITING FOR SOUND...", "STOP AND KEEP", "SAVING..."};
    button({br.x, br.y, br.w * 0.6f - 3, br.h}, micLab[ms], ms == 2, [this] { app_.micSample(); }, 12, ms != 0);
    button({br.x + br.w * 0.6f + 3, br.y, br.w * 0.4f - 3, br.h}, "LOAD FILE FROM DISK", false, [this] { app_.requestFile(); }, 11);
    const std::string ts_ = app_.toast();
    if (!ts_.empty()) text(r.x, br.y + br.h + 14, 11, ACCENT, ts_, 0, true);
}

void SamplerUI::pagePanel(Rect r) {
    const float tw = (r.w - 4 * 4) / 5;
    for (int i = 0; i < 10; i++) {
        Rect c{r.x + (i % 5) * (tw + 4), r.y + (i / 5) * 32, tw, 28};
        button(c, kTabName[i], app_.page() == kTabs[i], [this, i] { app_.gotoPage(kTabs[i]); }, 9.5f);
    }
    auto k = app_.knobs();
    const float top = r.y + 72, rh = (r.h - 72) / 4;
    for (int i = 0; i < 4; i++) {
        const auto& p = k[(size_t)i];
        const float y = top + i * rh;
        if (!p.valid) continue;
        text(r.x, y + 8, T_CAPTION, DIM, "K" + std::to_string(i + 1) + "  " + p.name, 0, true);
        text(r.x + r.w, y + 10, T_VALUE, p.isChoice ? ACCENT : TEXT, p.value, 2, true);
        Rect s{r.x, y + 26, r.w, 14};
        rrect(s, 0, SURFACE); stroke(s, 0, OUTLINE, 1);
        if (p.isChoice) {   // segments are not known here: a notch at the current position
            rrect({s.x + p.norm * (s.w - 8), s.y, 8, s.h}, 0, ACCENT);
        } else {
            rrect({s.x, s.y, std::max(2.f, s.w * clamp01(p.norm)), s.h}, 0, TEXT);
        }
        Rect hr{s.x, y + 14, s.w, 40};
        hit(hr, [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); },
            [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); });
    }
}

void SamplerUI::librarySheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(BG, 200));
    tap({0, 0, W_, H_}, [this] { sheetOpen_ = false; });
    const Rect box{W_ * 0.22f, 24, W_ * 0.56f, H_ - 48};
    rrect(box, R_CARD, SURFACE); stroke(box, R_CARD, ACCENT, 1);
    tap(box, [] {});
    text(box.x + 14, box.y + 20, T_TITLE, TEXT, "SAMPLE LIBRARY", 0, true);
    button({box.x + box.w - 70, box.y + 6, 60, 28}, "CLOSE", false, [this] { sheetOpen_ = false; }, 10);
    sheetList_ = {box.x + 6, box.y + 44, box.w - 12, box.h - 52};
    const auto& lib = app_.library();
    // rows: a header per group, then its entries
    struct Row { bool head; std::string text; int entry; };
    std::vector<Row> rows;
    std::string g;
    for (int i = 0; i < (int)lib.size(); i++) { if (lib[(size_t)i].group != g) { g = lib[(size_t)i].group; rows.push_back({true, g, -1}); } rows.push_back({false, lib[(size_t)i].label, i}); }
    const float rh = 34;
    sheetScroll_ = std::min(std::max(0.f, sheetScroll_), std::max(0.f, rows.size() * rh - sheetList_.h));
    clip(sheetList_);
    for (size_t i = 0; i < rows.size(); i++) {
        const float y = sheetList_.y + i * rh - sheetScroll_;
        if (y + rh < sheetList_.y || y > sheetList_.y + sheetList_.h) continue;
        if (rows[i].head) { text(sheetList_.x + 8, y + rh / 2, T_CAPTION, DIM, rows[i].text, 0, true); line(sheetList_.x, y + rh - 3, sheetList_.x + sheetList_.w, y + rh - 3, OUTLINE, 1); continue; }
        const bool cur = rows[i].entry == app_.libraryCursor();
        if (cur) { rrect({sheetList_.x, y + 1, sheetList_.w, rh - 2}, R_CONTROL, RAISED); stroke({sheetList_.x, y + 1, sheetList_.w, rh - 2}, R_CONTROL, ACCENT, 1); }
        text(sheetList_.x + 14, y + rh / 2, T_BODY + 1, cur ? ACCENT : TEXT, fit(rows[i].text, sheetList_.w - 30, T_BODY + 1), 0, cur);
    }
    endClip();
    hit(sheetList_, [this](float, float y) { sheetLastY_ = y; sheetMoved_ = 0; },
        [this](float, float y) { sheetScroll_ -= y - sheetLastY_; sheetMoved_ += std::fabs(y - sheetLastY_); sheetLastY_ = y; },
        [this, rows, rh]() {
            if (sheetMoved_ > 8) return;
            const int idx = (int)((sheetLastY_ - sheetList_.y + sheetScroll_) / rh);
            if (idx >= 0 && idx < (int)rows.size() && !rows[(size_t)idx].head) { app_.loadEntry(rows[(size_t)idx].entry); sheetOpen_ = false; }
        });
}

// PROJECT menu: what used to be the SAVE / CLEAR buttons of the FM-1 (and more) on the screen.
void SamplerUI::projectSheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(0xff000000, 200));
    tap({0, 0, W_, H_}, [this] { projectOpen_ = false; });
    const float bw = std::min(460.f, W_ - 40), bh = 38, gap = 8;
    const int t = app_.track();
    const bool hasClip = st_.track_has_clip[t], any = app_.loopExists();
    const Rect box{(W_ - bw) / 2, 22, bw, H_ - 44};
    rrect(box, 8, kShell); stroke(box, 8, kMetal, 1.5f);
    tap(box, [] {});
    text(box.x + 18, box.y + 22, 15, kAmber, "PROJECT", 0, true);
    key({box.x + box.w - 84, box.y + 8, 72, 30}, "CLOSE", 0, false, [this] { projectOpen_ = false; }, false);
    const std::string ts = app_.toast();
    text(box.x + 18, box.y + 48, 10, kPaper, !ts.empty() ? ts : app_.loopText(), 0, true);
    float y = box.y + 66;
    auto row = [&](const std::string& label, const std::string& hint, bool enabled, bool hot, std::function<void()> f) {
        Rect r{box.x + 18, y, box.w - 36, bh};
        rrect(r, 5, hot ? kRed : kPanelHi);
        text(r.x + 14, r.y + bh / 2 - 6, 11.5f, enabled ? kPaper : kMetal, label, 0, true);
        text(r.x + 14, r.y + bh / 2 + 9, 8.5f, enabled ? theme::argb(190, 188, 180) : theme::argb(100, 98, 94), hint, 0, false);
        tap(r, std::move(f));
        y += bh + gap;
    };
    const std::string tn = "TRACK " + std::to_string(t + 1);
    row("SAVE LOOP OF " + tn, "into the sample library (USER)", hasClip, false, [this] { app_.loopSave(); });
    row("CLEAR LOOP OF " + tn, "the other tracks keep theirs", hasClip, false, [this] { app_.loopClearTrack(); });
    const bool armed = clearArmed_ && now_ < clearArmed_;
    row(armed ? "TAP AGAIN: CLEAR ALL LOOPS" : "CLEAR ALL LOOPS", "the looper is clean again, loop length is free", any, armed, [this, armed] {
        if (armed) { app_.loopClear(); clearArmed_ = 0; } else clearArmed_ = now_ + 3000;
    });
    row("LOOP LENGTH x2", "the loop doubled, its content repeated", any, false, [this] { app_.lenAction(true); });
    row("LOOP LENGTH /2", "half as long, the second half is dropped", any, false, [this] { app_.lenAction(false); });
    row(std::string("LOOP SOURCE: ") + app_.knobs(SamplerApp::P_SAVE)[1].value, "what a take records: the track's sound or the microphone", true, false,
        [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
}
