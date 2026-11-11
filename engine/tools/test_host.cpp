// Host tests of engine/host/Host.h, the platform-neutral glue the iOS app is built on: MIDI byte stream in (running status, real-time bytes,
// split SysEx), SysEx frames out, UI frames and touches, the disk import. Exit code 1 on failure.
#include "../host/Host.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s: ", #c); printf(__VA_ARGS__); printf("\n"); } else { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    const int sr = SAMPLERATE;
    const std::string dir = "/tmp/fm1sampler_host", samples = std::string(getenv("HOME")) + "/fm1-sampler/samples";
    system(("rm -rf " + dir + "; mkdir -p " + dir).c_str());
    Host h(dir, samples);
    std::vector<float> buf(512), in(512, 0.f), all; std::vector<uint8_t> out; std::vector<UiOp> ops; uint32_t now = 0;
    size_t frames = 0, badFrames = 0, loose = 0;
    auto run = [&](double sec) {
        for (long i = 0; i < (long)(sec * sr) / 256; i++) {
            h.render(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); all.insert(all.end(), buf.begin(), buf.end());
            if (i % 5 == 0) {
                now += 33; h.tick(now, out);
                size_t covered = 0;
                Host::forEachFrame(out, [&](const uint8_t* m, size_t n) { frames++; covered += n; if (n < 8 || m[0] != 0xf0 || m[n - 1] != 0xf7 || m[1] != 0x7d) badFrames++; for (size_t k = 1; k + 1 < n; k++) if (m[k] & 0x80) badFrames++; });
                loose += out.size() - covered;
            }
        }
    };
    auto rmsFrom = [&](size_t from) { double a = 0; size_t n = 0; for (size_t i = from; i < all.size(); i++) { a += all[i] * all[i]; n++; } return n ? std::sqrt(a / n) : 0.0; };
    auto midi = [&](std::initializer_list<uint8_t> b) { std::vector<uint8_t> v(b); h.midiIn(v.data(), v.size()); };
    auto text = [&](float x, float size) { h.uiFrame(860, 400, now, ops); for (auto& o : ops) if (o.type == UiOp::TEXT && std::fabs(o.f[0] - x) < 0.5f && std::fabs(o.f[2] - size) < 0.5f) return o.text; return std::string("?"); };
    auto tapAt = [&](float x, float y) { h.uiFrame(860, 400, now, ops); h.uiTouch(0, x, y, now); h.uiTouch(2, x, y, now); run(0.1); };

    run(2.5);
    CHECK(frames > 10 && badFrames == 0 && loose == 0, "the FM-1 gets whole SysEx frames (%zu frames, %zu bad, %zu loose bytes)", frames, badFrames, loose);
    h.uiFrame(860, 400, now, ops);
    CHECK(ops.size() > 50 && ops[0].type == UiOp::RRECT && ops[0].f[2] == 860 && ops[0].f[3] == 400, "a UI frame: %zu ops, the first one is the background", ops.size());
    CHECK(text(24, 29) == "1", "the deck shows track 1 (%s)", text(24, 29).c_str());

    {   // a key of the FM-1, one byte per call with clock bytes in between
        size_t m = all.size();
        midi({0x90}); midi({0xf8}); midi({60}); midi({0xfe, 100});
        run(1.0); midi({0x80, 60, 0}); run(0.3);
        CHECK(rmsFrom(m) > 0.003, "a note split over calls with real-time bytes sounds (rms %.4f)", rmsFrom(m));
    }
    {   // SELECT encoder, two detents in one message with running status
        midi({0xb0, 16, 65, 16, 65}); run(0.2);
        CHECK(text(24, 29) == "3", "two encoder steps with running status select track 3 (%s)", text(24, 29).c_str());
        midi({0xb0, 16, 63, 16, 63}); run(0.2);
        CHECK(text(24, 29) == "1", "and back to track 1 (%s)", text(24, 29).c_str());
    }
    {   // SysEx from the FM-1 split over two calls (REDRAW), then a note right behind it in the same call
        size_t f0 = frames, m = all.size();
        midi({0xf0, 0x7d, 'D', 'A'}); midi({'W', 0x01, 0x42, 0xf7, 0x90, 64, 100});
        run(1.0); midi({0x80, 64, 0}); run(0.3);
        CHECK(frames > f0 && badFrames == 0, "a split SysEx is taken, the screen is repainted (%zu frames)", frames - f0);
        CHECK(rmsFrom(m) > 0.003, "the note after the SysEx sounds (rms %.4f)", rmsFrom(m));
    }
    {   // a broken SysEx (no F7) must not swallow what follows
        size_t m = all.size();
        midi({0xf0, 0x7d, 'D', 0x90, 67, 100}); run(0.8); midi({0x80, 67, 0}); run(0.3);
        CHECK(rmsFrom(m) > 0.003, "a note after a broken SysEx sounds (rms %.4f)", rmsFrom(m));
    }
    {   // touches: PLAY and STOP of the deck, the disk pictogram
        tapAt(24 + 20, 248); run(0.8);
        const bool playing = h.session().status().playing;
        tapAt(24 + 20 + 64, 248); run(1.2);
        CHECK(playing && !h.session().status().playing, "PLAY and STOP by touch");
        CHECK(!h.wantFile(), "no file wanted yet");
        tapAt(351, 184);
        const bool w1 = h.wantFile(), w2 = h.wantFile();
        CHECK(w1 && !w2, "the disk pictogram asks for the file picker once");
    }
    {   // a file from the disk lands on the selected track
        { std::ifstream src(samples + "/Bass/Fuzz.wav", std::ios::binary); std::ofstream dst(dir + "/sample_FromDisk.wav", std::ios::binary); dst << src.rdbuf(); }
        midi({0xb0, 16, 65, 16, 65}); midi({16, 65, 16, 65}); run(0.2);   // the second call goes on with the running status
        h.importFile(dir + "/sample_FromDisk.wav"); run(2.0);
        CHECK(text(24, 29) == "5" && h.session().trackLabel(4) == "sample_FromDisk", "import loads into track 5 (%s)", h.session().trackLabel(4).c_str());
        h.say("HELLO"); run(0.1);
        CHECK(text(24, 12) == "HELLO", "say() shows a toast (%s)", text(24, 12).c_str());
    }
    printf(fails ? "FAILED (%d)\n" : "ALL OK (0 failures)\n", fails);
    return fails ? 1 : 0;
}
