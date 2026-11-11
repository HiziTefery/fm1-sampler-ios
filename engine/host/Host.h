// Host: the platform-neutral part of an app around the engine (Session + SamplerApp + SamplerUI behind one lock).
// MIDI bytes of the FM-1 in, SysEx frames for it out, one UI frame as draw ops, touches, the audio block.
// The iOS app is built on it (ios/); tools/test_host.cpp checks it on the host. No platform code here.
#pragma once
#include "../app/SamplerApp.h"
#include "../dsp/Common.h"
#include "../session/Session.h"
#include "../ui/SamplerUI.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Host {
public:
    Host(const std::string& dataDir, const std::string& sampleDir) : session_(SAMPLERATE) {
        session_.setDataDir(dataDir); session_.setSampleDir(sampleDir);   // before the app: it reads the library and loads the first samples
        app_ = std::make_unique<SamplerApp>(session_, [this](const uint8_t* m, size_t n) { out_.insert(out_.end(), m, m + n); });
        ui_ = std::make_unique<SamplerUI>(*app_);
    }

    // ---- audio thread ----
    // out: interleaved stereo, in: interleaved stereo or null (no microphone)
    void render(float* out, const float* in, int frames) { Denormals::enableOnThisThread(); session_.process(out, in, frames); }

    // ---- any other thread ----
    // Raw MIDI bytes as they come from the port: running status, real-time bytes and SysEx split over several calls are handled.
    void midiIn(const uint8_t* d, size_t n) {
        std::lock_guard<std::mutex> l(mtx_);
        for (size_t i = 0; i < n; i++) {
            const uint8_t s = d[i];
            if (s >= 0xf8) continue;                        // real time (clock, active sensing): may come anywhere, even inside a SysEx
            if (inSx_) {                                    // SysEx from the FM-1 (scale state, redraw request), may span calls
                if (s >= 0x80 && s != 0xf7) { inSx_ = false; i--; continue; }   // anything else that starts a message ends a broken SysEx
                if (sx_.size() < 256) sx_.push_back(s);
                if (s == 0xf7) { inSx_ = false; app_->onSysex(sx_.data(), sx_.size()); }
                continue;
            }
            if (s == 0xf0) { inSx_ = true; sx_.assign(1, s); msgLen_ = 0; continue; }
            if (s >= 0xf0) { msgLen_ = 0; continue; }
            if (s >= 0x80) { msg_[0] = s; msgLen_ = 1; msgNeed_ = ((s & 0xf0) == 0xc0 || (s & 0xf0) == 0xd0) ? 2 : 3; continue; }
            if (msgLen_ == 0) continue;                     // a data byte without a status
            msg_[msgLen_++] = s;
            if (msgLen_ == msgNeed_) { if (msgNeed_ == 3) app_->onMidi(msg_, 3); msgLen_ = 1; }   // running status: the status stays
        }
    }
    // ~60 Hz. `out` receives the bytes to send to the FM-1: whole SysEx frames one after another (see forEachFrame).
    void tick(uint32_t nowMs, std::vector<uint8_t>& out) {
        std::lock_guard<std::mutex> l(mtx_);
        app_->tick(nowMs);
        out.clear(); out.swap(out_);
    }
    // Calls f(ptr, length) for every complete SysEx frame (.. F7) of a tick's output.
    template <class F> static void forEachFrame(const std::vector<uint8_t>& bytes, F&& f) {
        size_t s = 0;
        for (size_t i = 0; i < bytes.size(); i++) if (bytes[i] == 0xf7) { f(bytes.data() + s, i - s + 1); s = i + 1; }
    }
    bool wantFile() { std::lock_guard<std::mutex> l(mtx_); return app_->wantFile(); }                // the UI asked for the disk picker (once)
    void importFile(const std::string& wavPath) { std::lock_guard<std::mutex> l(mtx_); app_->importFile(wavPath); }   // a 16-bit WAV named sample_*.wav in the data dir
    void say(const std::string& msg) { std::lock_guard<std::mutex> l(mtx_); app_->say(msg); }
    void setLink(bool midi, bool usbAudio) { std::lock_guard<std::mutex> l(mtx_); ui_->setLink(midi, usbAudio); }
    // One UI frame for a screen of w x h layout units (the UI is laid out for h = 400): the draw ops are copied into `ops`.
    void uiFrame(float w, float h, uint32_t nowMs, std::vector<UiOp>& ops) {
        std::lock_guard<std::mutex> l(mtx_);
        ui_->setSize(w, h, 0, 0);
        ui_->frame(nowMs);
        ops = ui_->ops();
    }
    void uiTouch(int action /*0 down 1 move 2 up*/, float x, float y, uint32_t nowMs) { std::lock_guard<std::mutex> l(mtx_); ui_->touch(action, x, y, nowMs); }

    Session& session() { return session_; }

private:
    Session session_;
    std::mutex mtx_;                    // SamplerApp and SamplerUI are single-threaded: every call above takes it
    std::vector<uint8_t> out_;          // SysEx bytes waiting for the next tick
    std::unique_ptr<SamplerApp> app_;
    std::unique_ptr<SamplerUI> ui_;
    std::vector<uint8_t> sx_; bool inSx_ = false;
    uint8_t msg_[3] = {0, 0, 0}; int msgLen_ = 0, msgNeed_ = 3;   // channel message being assembled
};
