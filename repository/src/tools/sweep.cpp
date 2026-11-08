// sif_sweep: judgement-timing probe (Windows). Finds when a song's music starts in the system audio (WASAPI loopback,
// matched against our own decode of the same cue), then presses the lane keys at chart time + a fixed offset.
// Run it once per offset against the original game (shadPS4) and against sif.exe; compare which offsets give Perfect.
// Usage: sif_sweep --game DIR --live ID:VARIANT --offset MS --keys shadps4|port [--window TITLE_PART]
//        [--post] [--mirror] [--extra] [--tap-ms MS]
// Times: every press happens at song_start + note audio time (chart time + lead-in) + offset, where song_start is
// the QPC time at which audio sample 0 of the cue entered the system mix.
#include <windows.h>  // WIN32_LEAN_AND_MEAN comes from the sif_engine target
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <timeapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/live/song.h"

namespace {

constexpr int kRate = 4000;  // analysis rate (Hz): 0.25 ms lag resolution

double QpcFreq() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return double(f.QuadPart);
}
int64_t QpcNow() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
}

// Downmixes interleaved PCM to mono and averages blocks of `factor` samples (a crude low-pass + decimation).
struct Decimator {
    int factor = 12;
    double acc = 0;
    int n = 0;
    void Push(double mono, std::vector<float>& out) {
        acc += mono;
        if (++n == factor) {
            out.push_back(float(acc / factor));
            acc = 0;
            n = 0;
        }
    }
};

// Normalized cross-correlation of `ref` against `sig` at every lag in [from, to); returns the best lag and score.
void BestLag(const std::vector<float>& sig, size_t from, size_t to, const std::vector<float>& ref, size_t& lag, double& score) {
    double rr = 0;
    for (float r : ref) rr += double(r) * r;
    score = -1;
    for (size_t l = from; l + ref.size() <= sig.size() && l < to; ++l) {
        double sr = 0, ss = 0;
        for (size_t i = 0; i < ref.size(); ++i) {
            sr += double(sig[l + i]) * ref[i];
            ss += double(sig[l + i]) * sig[l + i];
        }
        const double s = ss > 0 && rr > 0 ? sr / std::sqrt(ss * rr) : 0;
        if (s > score) {
            score = s;
            lag = l;
        }
    }
}

struct KeyEvent {
    int64_t qpc;
    int lane;
    bool down;
};

// --post: the key goes to `target` only (WM_KEYDOWN/WM_KEYUP), never into the global input stream
void PostKey(HWND target, WORD vk, bool down) {
    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LPARAM l = 1 | LPARAM(scan) << 16;
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) l |= LPARAM(1) << 24;
    if (!down) l |= LPARAM(0xC0000000);
    PostMessageW(target, down ? WM_KEYDOWN : WM_KEYUP, vk, l);
}

void SendKey(WORD vk, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = WORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &in, sizeof(in));
}

HWND FindWindowContaining(const std::wstring& part) {
    struct Ctx { std::wstring part; HWND found; } ctx{part, nullptr};
    EnumWindows([](HWND h, LPARAM p) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(p);
        wchar_t title[512];
        if (IsWindowVisible(h) && GetWindowTextW(h, title, 512) && std::wstring(title).find(c->part) != std::wstring::npos) {
            c->found = h;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // progress must reach a redirected log immediately
    std::string game, window = "shadPS4", keys = "shadps4";
    int song_id = 340, variant = 0;
    double offset_ms = 0;
    // diagnostics: a different offset per lane (so each lane lands in its own grade), skipped lanes, taps only
    double lane_offset[9];
    bool lane_set[9] = {}, lane_skip[9] = {}, tap_only = false;
    // --post: PostMessage to the window instead of SendInput; --mirror: lane k pressed as 8 - k (the game sometimes
    // plays a chart mirrored); --extra: also press the notes of chart channels 2-4 (the game plays them as notes)
    bool post = false, mirror = false, extra = false;
    double tap_ms = 50;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--tap-only")) tap_only = true;
        if (!std::strcmp(argv[i], "--post")) post = true;
        if (!std::strcmp(argv[i], "--mirror")) mirror = true;
        if (!std::strcmp(argv[i], "--extra")) extra = true;
        if (!std::strcmp(argv[i], "--tap-ms") && i + 1 < argc) tap_ms = std::atof(argv[i + 1]);
    }
    for (int i = 1; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--game")) game = argv[++i];
        else if (!std::strcmp(argv[i], "--offset")) offset_ms = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--lane-offset")) {  // LANE:MS
            char* rest = nullptr;
            const long lane = std::strtol(argv[++i], &rest, 10);
            if (lane >= 0 && lane < 9 && rest && *rest == ':') {
                lane_offset[lane] = std::atof(rest + 1);
                lane_set[lane] = true;
            }
        } else if (!std::strcmp(argv[i], "--skip-lane")) {
            const long lane = std::strtol(argv[++i], nullptr, 10);
            if (lane >= 0 && lane < 9) lane_skip[lane] = true;
        }
        else if (!std::strcmp(argv[i], "--keys")) keys = argv[++i];
        else if (!std::strcmp(argv[i], "--window")) window = argv[++i];
        else if (!std::strcmp(argv[i], "--live")) {
            char* rest = nullptr;
            song_id = int(std::strtol(argv[++i], &rest, 10));
            if (rest && *rest == ':') variant = int(std::strtol(rest + 1, nullptr, 10));
        }
    }
    // lanes left to right: L2 L1 Left Down Right Cross Circle R1 R2 (shadPS4 default keyboard map) / sif.exe keys
    const WORD shad_keys[9] = {'E', 'Q', VK_LEFT, VK_DOWN, VK_RIGHT, 'N', 'B', 'U', 'O'};
    const WORD port_keys[9] = {'A', 'S', 'D', 'F', VK_SPACE, 'J', 'K', 'L', VK_OEM_1};
    const WORD* lane_keys = keys == "port" ? port_keys : shad_keys;

    sif::Vfs vfs;
    sif::live::Song song;
    sif::cri::Pcm music;
    std::string error;
    if (game.empty() || !vfs.Mount(sif::os::PathFromUtf8(game)) || !sif::live::LoadSong(vfs, song_id, variant, song, &error) ||
        !sif::live::LoadSongMusic(vfs, song, music, &error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    std::vector<sif::live::Note> notes = song.chart.notes;
    if (extra) {
        for (int ch = 2; ch <= 4; ++ch) {
            std::string name = song.chart_name;
            name.replace(name.size() - 8, 8, "_ch" + std::to_string(ch) + ".scd");
            char folder[64];
            std::snprintf(folder, sizeof(folder), "live/%04d/%04d.pac/score.pac/", song_id, song_id);
            std::vector<uint8_t> scor;
            sif::live::Chart c;
            if (!vfs.Read(folder + name, scor, &error) || !sif::live::ParseChart(scor, c, &error)) {
                std::fprintf(stderr, "channel %d: %s\n", ch, error.c_str());
                return 1;
            }
            notes.insert(notes.end(), c.notes.begin(), c.notes.end());
        }
    }
    // reference: mono 4 kHz; a 1.5 s segment starting at the first clearly audible second of the cue
    std::vector<float> ref_all;
    Decimator dec_ref{music.rate / kRate};
    for (size_t f = 0; f < music.Frames(); ++f) {
        double m = 0;
        for (int c = 0; c < music.channels; ++c) m += music.samples[f * size_t(music.channels) + size_t(c)];
        dec_ref.Push(m / music.channels / 32768.0, ref_all);
    }
    size_t ref_start = 0;
    for (size_t s = 0; s + kRate < ref_all.size(); s += kRate / 10) {
        double e = 0;
        for (size_t i = s; i < s + kRate / 10; ++i) e += double(ref_all[i]) * ref_all[i];
        if (std::sqrt(e / (kRate / 10)) > 0.02) {
            ref_start = s;
            break;
        }
    }
    const std::vector<float> ref(ref_all.begin() + ptrdiff_t(ref_start), ref_all.begin() + ptrdiff_t(ref_start + kRate * 3 / 2));
    std::printf("song %04d/%02d '%s': %zu notes, lead-in %.0f ms; reference segment at %.3f s of the cue; offset %+.1f ms, keys %s\n",
                song_id, variant, song.title.c_str(), song.chart.notes.size(), song.chart.lead_in_ms,
                double(ref_start) / kRate, offset_ms, keys.c_str());

    // WASAPI loopback of the default render device
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* mix = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumerator))) ||
        FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) ||
        FAILED(client->GetMixFormat(&mix)) ||
        FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10'000'000, 0, mix, nullptr)) ||
        FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture)))) {
        std::fprintf(stderr, "WASAPI loopback setup failed\n");
        return 1;
    }
    const bool is_float = mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                          (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->wBitsPerSample == 32);
    if (mix->nSamplesPerSec % kRate != 0 || !is_float) {
        std::fprintf(stderr, "unsupported mix format: %lu Hz, %u bits\n", mix->nSamplesPerSec, mix->wBitsPerSample);
        return 1;
    }
    std::printf("loopback: %lu Hz, %u ch float; waiting for the song (start it now)\n", mix->nSamplesPerSec, mix->nChannels);
    client->Start();

    const double freq = QpcFreq();
    std::vector<float> sig;       // captured mono at kRate
    std::vector<int64_t> sig_qpc; // QPC time (in QPC ticks) of each captured analysis sample
    Decimator dec_sig{int(mix->nSamplesPerSec) / kRate};
    int64_t song_start = 0;
    double match = 0;
    size_t searched = 0;  // lags below this were already tested
    size_t best_lag = 0;
    int64_t next_report = QpcNow() + int64_t(freq * 5);
    const int64_t give_up = QpcNow() + int64_t(freq * 300);  // 5 minutes
    for (int64_t next_check = QpcNow(); !song_start && QpcNow() < give_up;) {
        Sleep(5);
        UINT32 packet = 0;
        while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            UINT64 dev_pos = 0, qpc_pos = 0;  // qpc_pos: 100 ns units
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, &dev_pos, &qpc_pos))) break;
            const auto* f = reinterpret_cast<const float*>(data);
            const int64_t t0 = int64_t(double(qpc_pos) * 1e-7 * freq);
            for (UINT32 i = 0; i < frames; ++i) {
                double m = 0;
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    for (int c = 0; c < mix->nChannels; ++c) m += f[i * mix->nChannels + c];
                }
                const size_t before = sig.size();
                dec_sig.Push(m / mix->nChannels, sig);
                if (sig.size() != before) sig_qpc.push_back(t0 + int64_t(double(i) / mix->nSamplesPerSec * freq));
            }
            capture->ReleaseBuffer(frames);
        }
        if (QpcNow() >= next_check && sig.size() > ref.size() + kRate) {
            next_check = QpcNow() + int64_t(freq * 0.1);
            size_t lag = 0;
            double score = 0;
            BestLag(sig, searched, sig.size(), ref, lag, score);
            searched = sig.size() - ref.size() + 1;
            if (score > match) {
                match = score;
                best_lag = lag;
            }
            if (QpcNow() >= next_report) {
                next_report = QpcNow() + int64_t(freq * 5);
                std::printf("  %.0f s captured, best correlation so far %.3f\n", double(sig.size()) / kRate, match);
            }
            // first hit above 0.7 (0.78 seen with other sound playing): keep testing 100 ms of later lags, then take the best one (the true peak)
            if (match > 0.7 && searched > best_lag + kRate / 10) {
                song_start = sig_qpc[best_lag] - int64_t(double(ref_start) / kRate * freq);
            }
        }
    }
    if (!song_start) {
        std::fprintf(stderr, "song not found in the system audio\n");
        return 1;
    }
    std::printf("song found: correlation %.3f; audio sample 0 at QPC %lld (%.1f ms ago)\n", match, (long long)song_start,
                double(QpcNow() - song_start) / freq * 1000);

    HWND target = FindWindowContaining(std::wstring(window.begin(), window.end()));
    if (post && !target) {
        std::fprintf(stderr, "--post: no window titled '%s'\n", window.c_str());
        return 1;
    }
    if (target && !post) SetForegroundWindow(target);
    std::vector<KeyEvent> events;
    const auto at = [&](double ms, int lane) {
        const double off = lane_set[lane] ? lane_offset[lane] : offset_ms;
        return song_start + int64_t((ms + off) / 1000.0 * freq);
    };
    for (const auto& n : notes) {
        const int lane = mirror ? 8 - n.lane : n.lane;
        if (lane_skip[lane]) continue;
        events.push_back({at(n.time_ms, lane), lane, true});
        events.push_back({at(n.IsHold() && !tap_only ? n.end_ms : n.time_ms + tap_ms, lane), lane, false});
    }
    std::stable_sort(events.begin(), events.end(), [](const KeyEvent& a, const KeyEvent& b) { return a.qpc < b.qpc; });
    double late_sum = 0, late_max = 0;
    int skipped = 0;
    timeBeginPeriod(1);  // Sleep(1) sleeps ~1 ms instead of up to one 15.6 ms scheduler tick
    for (const KeyEvent& e : events) {
        if (double(QpcNow() - e.qpc) / freq > 0.030) {
            ++skipped;  // more than 30 ms past (song detected too late): skip rather than press out of time
            continue;
        }
        // events at the same instant (double notes, a release with a press) go out back to back
        while (double(e.qpc - QpcNow()) / freq > 0.020) Sleep(1);
        while (QpcNow() < e.qpc) {}
        post ? PostKey(target, lane_keys[e.lane], e.down) : SendKey(lane_keys[e.lane], e.down);
        const double late = double(QpcNow() - e.qpc) / freq * 1000;
        late_sum += late;
        late_max = std::max(late_max, late);
    }
    timeEndPeriod(1);
    std::printf("sent %zu key events (%d skipped), mean lateness %.3f ms, max %.3f ms\n", events.size() - size_t(skipped),
                skipped, late_sum / std::max<size_t>(1, events.size() - size_t(skipped)), late_max);
    client->Stop();
    return skipped ? 2 : 0;
}
