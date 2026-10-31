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
// Black, white, orange. Flat shapes only: no frames, no cards. White is content, orange is what is active or recording, the dim white is what is idle.
namespace {
constexpr float kPi = 3.14159265f;
constexpr uint32_t kBg = theme::argb(0, 0, 0), kPaper = theme::argb(255, 255, 255), kOrange = theme::argb(255, 104, 40), kInk = kBg,
    kLine = theme::argb(255, 255, 255, 46), kDim = theme::argb(255, 255, 255, 120);
}

// A roller: a white ring with three spokes, turned by `ang`.
void SamplerUI::reel(float cx, float cy, float r, float ang) {
    circle(cx, cy, r, kPaper);
    circle(cx, cy, r - 2, kBg);
    for (int k = 0; k < 3; k++) { const float a = ang + k * 2 * kPi / 3; line(cx, cy, cx + (r - 3) * std::cos(a), cy + (r - 3) * std::sin(a), kPaper, 2); }
    circle(cx, cy, 3, kPaper);
}

// The sample of the selected track: its waveform (the playback window bright), the playback mode, and a line for every sounding voice.
void SamplerUI::sampleView(Rect r) {
    const int t = app_.track();
    const auto ts = app_.state(t);
    text(r.x, r.y + 8, 11, kPaper, SamplerApp::algoName(ts.algo), 0, true);
    tap({r.x, r.y, 120, 18}, [this, ts] { app_.setAlgo((ts.algo + 1) % 6); });   // tap the mode: the next one
    const int nv = st_.voiceN[t];
    if (nv > 0) text(r.x + r.w, r.y + 8, 9, kOrange, std::to_string(nv) + (nv == 1 ? " VOICE" : " VOICES"), 2, true);
    const Rect w{r.x, r.y + 20, r.w, r.h - 20};
    const int bins = std::max(20, (int)(w.w / 2.5f));
    auto pk = app_.peaks(t, bins);
    if (pk.empty()) { text(w.x + w.w / 2, w.y + w.h / 2, 10, kDim, "NO SAMPLE", 1, true); return; }
    const float cy = w.y + w.h / 2, bw = w.w / bins;
    for (int i = 0; i < (int)pk.size(); i++) {
        const float pos = (i + 0.5f) / bins, v = std::min(1.f, pk[(size_t)i] * 1.1f);
        rrect({w.x + i * bw, cy - std::max(1.f, v * (w.h / 2 - 1)), std::max(1.f, bw - 0.6f), std::max(2.f, v * (w.h - 2))}, 0, pos >= ts.start && pos <= ts.end ? kPaper : kLine);
    }
    for (int j = 0; j < nv; j++) {
        const float vx = w.x + std::min(1.f, std::max(0.f, st_.voicePos[t][j])) * w.w;
        line(vx, w.y, vx, w.y + w.h, kOrange, 2);
    }
}

// A rotary knob on a knob of the app (drag up / down to turn): a white disc, a black pointer, orange dots for the value.
void SamplerUI::knob(float cx, float cy, SamplerApp::Page page, int idx, bool) {
    auto k = app_.knobs(page);
    const auto& p = k[(size_t)idx];
    if (!p.valid) return;
    const float R = 14, n = clamp01(p.norm);
    for (int j = 0; j <= 10; j++) { const float a = (-135.f + j * 27.f) * kPi / 180; circle(cx + std::sin(a) * (R + 6), cy - std::cos(a) * (R + 6), 1.4f, j / 10.f <= n + 0.001f ? kOrange : kLine); }
    circle(cx, cy, R, kPaper);
    const float a = (-135.f + n * 270.f) * kPi / 180;
    line(cx + std::sin(a) * 4, cy - std::cos(a) * 4, cx + std::sin(a) * (R - 3), cy - std::cos(a) * (R - 3), kInk, 3.5f);
    text(cx, cy + R + 15, 9, kDim, fit(p.name, 100, 9), 1, true);
    text(cx, cy + R + 28, 11, kPaper, p.value, 1, true);
    Rect hr{cx - 36, cy - R - 12, 72, 2 * R + 46};
    hit(hr, [this, n](float, float y) { dragY_ = y; dragV_ = n; },
        [this, page, idx](float, float y) { app_.knobs(page)[(size_t)idx].set(clamp01(dragV_ + (dragY_ - y) / 110.f)); });
}

void SamplerUI::cassette(Rect c) {
    const int t = app_.track();
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const bool recNow = st_.freeRecording || st_.track_clip_rec[t];
    const int mic = app_.micState();
    // label: paper strip with a coloured edge, the sample name, the counter
    Rect lb{c.x + 24, c.y + 12, c.w - 48, 50};
    std::string title = app_.sampleLabel(t), sub;
    if (title.empty()) title = "EMPTY TRACK";
    if (mic == 1) { title = "WAITING FOR SOUND"; sub = "MICROPHONE  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())); }
    else if (mic == 2) { title = "RECORDING " + std::to_string(st_.sampleSeconds).substr(0, 4) + " S"; sub = "MICROPHONE"; }
    else if (mic == 3) title = "SAVING";
    const std::string ts = app_.toast();
    if (!ts.empty()) sub = ts;
    text(lb.x + 28, lb.y + 14, 8.5f, kDim, fit(sub, lb.w - 130, 8.5f), 0, true);
    text(lb.x + 28, lb.y + 31, 16, kOrange, std::to_string(t + 1), 0, true);   // the track: tap for the next one
    text(lb.x + 46, lb.y + 31, 16, kPaper, fit(title, lb.w - 74, 15), 0, true);
    tap({lb.x + 24, lb.y + 12, 22, 30}, [this, t] { app_.selectTrack((t + 1) % SamplerApp::kTracks); });
    tri(lb.x + 18, lb.y + 15, lb.x + 12, lb.y + 23, lb.x + 18, lb.y + 31, kPaper);
    tri(lb.x + lb.w - 12, lb.y + 15, lb.x + lb.w - 6, lb.y + 23, lb.x + lb.w - 12, lb.y + 31, kPaper);
    tap({lb.x + 46, lb.y, lb.w - 70, lb.h}, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    tap({lb.x, lb.y, 24, lb.h}, [this] { app_.browse(-1); });
    tap({lb.x + lb.w - 24, lb.y, 24, lb.h}, [this] { app_.browse(1); });
    float lsec = 0, psec = 0, lp = 0;
    if (hasLoop) { lsec = (float)(st_.loopSamples / app_.session().sampleRate()); lp = loopPos(); psec = lp * lsec; }
    else if (st_.freeRecording) psec = lsec = st_.freeSeconds;
    char cnt[48]; snprintf(cnt, sizeof cnt, "%d:%04.1f / %d:%04.1f", (int)psec / 60, std::fmod(psec, 60.f), (int)lsec / 60, std::fmod(lsec, 60.f));
    text(lb.x + lb.w - 22, lb.y + 14, 9, kPaper, hasLoop || st_.freeRecording ? cnt : "NO LOOP", 2, true);
    if (recNow || st_.freeArmed) { if (recNow || (now_ / 400) % 2 == 0) circle(lb.x + lb.w - 14, lb.y + 33, 4.5f, kOrange); }
    // the tape loop: one closed ribbon round two rollers; it runs once round per loop, the orange dot is the splice = the start of the loop
    Rect wd{c.x + c.w * 0.07f, lb.y + lb.h + 8, c.w * 0.86f, c.h - (lb.h + 12) - 8 - 32 + 14};
    // The ribbon runs over two wheels (a big and a small one) and one roller inside the loop that pushes it out into a bend.
    // Between two posts it is a common tangent, round a post an arc: the path is built from those.
    struct Post { float x, y, r; int s; };   // s = -1: the post is inside the ribbon (it wraps round it), +1: outside
    const float W0 = wd.w, H0 = wd.h;
    const Post posts[3] = {
        {wd.x + 0.22f * W0, wd.y + 0.52f * H0, 0.31f * H0, -1},   // big wheel
        {wd.x + 0.80f * W0, wd.y + 0.50f * H0, 0.20f * H0, -1},   // small wheel
        {wd.x + 0.52f * W0, wd.y + 0.23f * H0, 0.10f * H0, -1},   // the roller: inside, under the top run, it pushes the ribbon out
    };
    struct Seg { bool arc; float x0, y0, x1, y1, cx, cy, r, a0, da, len; };
    std::vector<Seg> segs;
    {
        const int N = 3; float phi[N], px[N][2], qx[N][2];   // exit point of post i, entry point of post i
        for (int i = 0; i < N; i++) {
            const Post &a = posts[i], &b = posts[(i + 1) % N];
            const float dx = b.x - a.x, dy = b.y - a.y, D = std::sqrt(dx * dx + dy * dy), k = b.s * b.r - a.s * a.r;
            phi[i] = std::atan2(dy, dx) - std::asin(std::max(-1.f, std::min(1.f, k / D)));
            const float nx = -std::sin(phi[i]), ny = std::cos(phi[i]);
            px[i][0] = a.x - a.s * a.r * nx; px[i][1] = a.y - a.s * a.r * ny;
            qx[(i + 1) % N][0] = b.x - b.s * b.r * nx; qx[(i + 1) % N][1] = b.y - b.s * b.r * ny;
        }
        for (int i = 0; i < N; i++) {
            const int j = (i + 1) % N; const Post& b = posts[j];
            Seg l{false, px[i][0], px[i][1], qx[j][0], qx[j][1], 0, 0, 0, 0, 0, 0}; l.len = std::hypot(l.x1 - l.x0, l.y1 - l.y0); segs.push_back(l);
            const float a0 = std::atan2(qx[j][1] - b.y, qx[j][0] - b.x), a1 = std::atan2(px[j][1] - b.y, px[j][0] - b.x);
            float da = a1 - a0;
            if (b.s > 0) { while (da < 0) da += 2 * kPi; while (da > 2 * kPi) da -= 2 * kPi; } else { while (da > 0) da -= 2 * kPi; while (da < -2 * kPi) da += 2 * kPi; }
            Seg ar{true, 0, 0, 0, 0, b.x, b.y, b.r, a0, da, b.r * std::fabs(da)}; segs.push_back(ar);
        }
    }
    float per = 0; for (const Seg& g : segs) per += g.len;
    auto at = [&](float sp, float& x, float& y) {
        sp = std::fmod(sp + per, per);
        for (const Seg& g : segs) {
            if (sp > g.len) { sp -= g.len; continue; }
            if (!g.arc) { const float f = g.len > 0 ? sp / g.len : 0; x = g.x0 + (g.x1 - g.x0) * f; y = g.y0 + (g.y1 - g.y0) * f; }
            else { const float an = g.a0 + g.da * (g.len > 0 ? sp / g.len : 0); x = g.cx + g.r * std::cos(an); y = g.cy + g.r * std::sin(an); }
            return;
        }
        x = segs[0].x0; y = segs[0].y0;
    };
    const float turn = hasLoop ? lp : st_.freeRecording ? std::fmod((float)now_ / 2500.f, 1.f) : 0.f;
    const uint32_t tapeCol = recNow ? kOrange : kPaper;
    for (const Seg& g : segs) {
        if (!g.arc) { line(g.x0, g.y0, g.x1, g.y1, tapeCol, 2.5f); continue; }
        const int n = std::max(2, (int)(std::fabs(g.da) / 0.2f));
        for (int i = 0; i < n; i++) {
            const float u0 = g.a0 + g.da * i / n, u1 = g.a0 + g.da * (i + 1) / n;
            line(g.cx + g.r * std::cos(u0), g.cy + g.r * std::sin(u0), g.cx + g.r * std::cos(u1), g.cy + g.r * std::sin(u1), tapeCol, 2.5f);
        }
    }
    const int t0 = std::max(1, (int)std::lround(per / (2 * kPi * posts[0].r))), t1 = std::max(1, (int)std::lround(per / (2 * kPi * posts[1].r))), t2 = std::max(1, (int)std::lround(per / (2 * kPi * posts[2].r)));
    reel(posts[0].x, posts[0].y, posts[0].r - 3, -turn * 2 * kPi * t0); reel(posts[1].x, posts[1].y, posts[1].r - 3, -turn * 2 * kPi * t1);
    reel(posts[2].x, posts[2].y, posts[2].r - 1.5f, -turn * 2 * kPi * t2);
    float x, y; at(turn * per, x, y); circle(x, y, 5, recNow ? kPaper : kOrange);
}

float SamplerUI::loopPos() const {
    const int t = app_.track();
    if (!(app_.loopExists() && st_.loopSamples > 0)) return 0.f;
    return st_.track_has_clip[t] ? st_.clipPos[t] : (float)(st_.pos / st_.loopSamples);
}

// Timeline of the selected track: a ruler with bars and seconds, the loop's waveform, the playhead. Drag anywhere on it to scrub.
void SamplerUI::timeline(Rect r) {
    const int t = app_.track();
    const float tx = r.x + 6, tw = r.w - 12, rh = 28, wy = r.y + rh + 8, wh = r.h - rh - 16;
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const float sr = (float)app_.session().sampleRate(), loopSec = hasLoop ? (float)(st_.loopSamples / sr) : 0.f;
    if (!hasLoop) {
        const bool rec = st_.freeRecording;
        text(tx + tw / 2, r.y + r.h / 2, 11, rec ? kOrange : kDim, rec ? "RECORDING " + num("%.1f", st_.freeSeconds) + " S" : "NO LOOP: PRESS REC AND PLAY", 1, true);
        return;
    }
    const float barSec = loopSec / std::max(1, st_.loopBars), pps = tw / loopSec;
    const float barPx = barSec * pps; const int every = barPx >= 26 ? 1 : barPx >= 13 ? 2 : barPx >= 7 ? 4 : 8;
    for (int b = 0; b <= st_.loopBars; b++) {   // bars: numbers and beats on top, a thin line through the waveform
        const float x = tx + b * barPx;
        if (b < st_.loopBars) {
            if (b % every == 0) { line(x, r.y + 8, x, r.y + 19, kPaper, 1.2f); text(x + 4, r.y + 14, 9, kPaper, std::to_string(b + 1), 0, true); }
        }
        if (b % every == 0 || b == st_.loopBars) line(x, wy, x, wy + wh, kLine, 1);
    }
    static const float steps[] = {0.5f, 1, 2, 5, 10, 15, 30, 60, 120};
    float step = 120; for (float sp : steps) if (sp * pps >= 44) { step = sp; break; }
    for (float sec = 0; sec <= loopSec + 0.001f; sec += step) {   // seconds under the bars
        const float x = tx + sec * pps;
        line(x, r.y + 22, x, r.y + 8 + rh - 1, kDim, 1);
        char lb[16]; if (loopSec >= 60) snprintf(lb, sizeof lb, "%d:%02d", (int)sec / 60, (int)sec % 60); else snprintf(lb, sizeof lb, step < 1 ? "%.1fs" : "%.0fs", sec);
        if (x + 26 < tx + tw) text(x + 4, r.y + 28, 8, kDim, lb, 0, false);
    }
    float pk[Session::kLoopBins]; const int n = app_.session().loopPeaks(t, pk, Session::kLoopBins);
    const bool rec = st_.track_clip_rec[t];
    if (n > 0 && st_.track_has_clip[t]) {
        float mx = 0.08f; for (int i = 0; i < n; i++) mx = std::max(mx, pk[i]);
        const float cy = wy + wh / 2, bw = tw / n;
        for (int i = 0; i < n; i++) { const float v = std::min(1.f, pk[i] / mx); if (v > 0.01f) rrect({tx + i * bw, cy - v * (wh / 2 - 3), std::max(1.f, bw - 0.4f), std::max(1.f, v * (wh - 6))}, 0, rec ? kOrange : kPaper); }
    } else text(tx + tw / 2, wy + wh / 2, 10, kDim, "TRACK " + std::to_string(t + 1) + " HAS NO LOOP", 1, true);
    const float hx = tx + std::min(1.f, loopPos()) * tw;
    line(hx, r.y + 8, hx, wy + wh, kOrange, 2);
    tri(hx - 5, r.y + 6, hx + 5, r.y + 6, hx, r.y + 14, kOrange);
    auto seek = [this, tx, tw](float x, float) { app_.seek(clamp01((x - tx) / tw)); };
    hit({tx, r.y, tw, r.h}, seek, seek);
}

void SamplerUI::key(Rect r, const std::string& label, int icon, bool on, std::function<void()> f, bool) {
    const uint32_t ink = on ? kOrange : kPaper;
    const float cx = r.x + r.w / 2, cy = r.y + r.h / 2 - 5;
    if (icon == 1) tri(cx - 5, cy - 7, cx - 5, cy + 7, cx + 7, cy, ink);
    else if (icon == 2) { rrect({cx - 6, cy - 7, 4.5f, 14}, 0, ink); rrect({cx + 2, cy - 7, 4.5f, 14}, 0, ink); }
    else if (icon == 3) circle(cx, cy, 6.5f, kOrange);
    text(cx, icon ? r.y + r.h - 9 : r.y + r.h / 2, 9, ink, label, 1, true);
    tap(r, std::move(f));
}

void SamplerUI::deck(Rect r) {
    rrect({0, 0, W_, H_}, 0, kBg);
    const bool recOn = st_.freeRecording || st_.track_clip_rec[app_.track()], armed = st_.freeArmed;
    const float top = 12, ch = 192, cw = std::min(ch * 1.75f, W_ * 0.42f);
    const float cxl = 28, zr = W_ - 24, x0 = cxl + cw + 26, w0 = zr - x0;
    cassette({cxl, top, cw, ch});
    const float vh = 78;
    sampleView({x0, top + 2, w0, vh});
    const float py = top + vh + 24;
    text(x0, py, 15, kOrange, SamplerApp::pageName(app_.page()), 0, true);
    for (int i = 0; i < 10; i++) {   // the pages as notches: tap one to go there (the FM-1 buttons do the same)
        Rect nr{zr - (10 - i) * 22.f, py - 11, 22, 22};
        rrect({nr.x + 3, py - 3, 16, 6}, 0, kTabs[i] == app_.page() ? kOrange : kLine);
        tap(nr, [this, i] { app_.gotoPage(kTabs[i]); });
    }
    for (int i = 0; i < 4; i++) knob(x0 + w0 * (i + 0.5f) / 4, py + 40, app_.page(), i);
    const float ty = H_ - 46;
    timeline({24, top + ch + 16, W_ - 48, ty - 8 - (top + ch + 16)});
    struct K { const char* l; int icon; bool on; std::function<void()> f; };
    const int ms = app_.micState();
    static const char* micLab[] = {"MIC", "WAITING", "STOP MIC", "SAVING"};
    const float kw = std::min(84.f, (W_ - 60) / 9.f), gap = 6;
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
        if (i == 1 && armed && (now_ / 400) % 2 == 0) rrect({kr.x, kr.y + kr.h - 3, kr.w, 3}, 0, kOrange);
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
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(0xff000000, 235));
    tap({0, 0, W_, H_}, [this] { projectOpen_ = false; });
    const float bw = std::min(460.f, W_ - 40), bh = 38, gap = 8;
    const int t = app_.track();
    const bool hasClip = st_.track_has_clip[t], any = app_.loopExists();
    const Rect box{(W_ - bw) / 2, 22, bw, H_ - 44};
    tap(box, [] {});
    text(box.x + 18, box.y + 22, 15, kOrange, "PROJECT", 0, true);
    key({box.x + box.w - 84, box.y + 8, 72, 30}, "CLOSE", 0, false, [this] { projectOpen_ = false; }, false);
    const std::string ts = app_.toast();
    text(box.x + 18, box.y + 48, 10, kPaper, !ts.empty() ? ts : app_.loopText(), 0, true);
    float y = box.y + 66;
    auto row = [&](const std::string& label, const std::string& hint, bool enabled, bool hot, std::function<void()> f) {
        Rect r{box.x + 18, y, box.w - 36, bh};
        text(r.x + 10, r.y + bh / 2 - 6, 11.5f, hot ? kOrange : enabled ? kPaper : kLine, label, 0, true);
        text(r.x + 10, r.y + bh / 2 + 9, 8.5f, kDim, hint, 0, false);
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
