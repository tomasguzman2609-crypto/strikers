// See include/port/custom_sfx.h.

#include "port/custom_sfx.h"
#include "port/host.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace
{

// Must match the mixer's own output format in src/platform/audio_out.cpp
// (kSampleRate / kChannels). Clips are converted to this once, at load time,
// so the per-buffer mix stays a plain add.
constexpr int kMixSampleRate = 32000;
constexpr int kMixChannels = 2;
constexpr int kMaxVoices = 4;

// The playback-rate floor every custom clip uses. Clips follow the sim's
// slow-motion time scale (perfect pass 0.5, Super Strike windup 0.4-0.6, its
// last instant 0.1, the matrix-cam close-up near 0) but never drop below
// this, so they always run a bit faster than the game's own slow-mo and stay
// intelligible: a clip plays at max(time scale, kSlowMoFloor).
constexpr float kSlowMoFloor = 0.75f;
// PortCustomSFXPlayFixed's floor: the Super Strike / HyperStrike voice lines
// (windup, kick, charge, shot) never slow down at all, however slow the
// special's own cinematic runs.
constexpr float kVoiceLineMinScale = 1.0f;

struct Clip
{
    std::vector<int16_t> samples; // interleaved, kMixChannels channels, kMixSampleRate
    bool exists = false; // sfx/<name>.wav was found on disk (whether or not it decoded)
    bool loaded = false; // ... and decoded into `samples`
};

struct Voice
{
    const Clip* clip = nullptr;
    double framePos = 0.0; // in frames (sample pairs), not samples; fractional so
                            // it can track the game's slow-motion timescale below
    float minScale = 1.0f; // playback-rate floor; both PortCustomSFXPlay and
                            // PortCustomSFXPlayFixed set it (kSlowMoFloor and
                            // kVoiceLineMinScale), so clips slow down with the
                            // sim but never below it
    float gain = 1.0f; // the clip's volume from sfx/sonidos.txt (1.0 = as recorded)
};

// One line of sfx/sonidos.txt: which file replaces a game sound, how loud, and
// whether the replacement is on at all (off = the game's own sound plays).
struct Replacement
{
    std::string file; // beside sonidos.txt in sfx/; "-" = silence the game's sound
    float gain = 1.0f;
    bool on = true;
};

// PORT: the game slows its own audio down (pitch + speed) during slow-motion
// moments (Super Strike close-ups, etc.) by driving MusyX's group pitch, not
// by resampling its output buffer. Our mixer sits after that buffer is
// already produced, so we approximate the same effect by advancing our own
// voices at the same fractional rate as the sim's time scale, instead of one
// output frame per source frame. FixedUpdateTask.cpp exports this pointer
// (already used by src/platform/overlay.cpp) — 1.0 = normal speed, lower
// during slow-mo.
extern "C" float* PortSimTimeScalePtr(void);

// MusyX's own start call (snd_synthapi.c), for previewing a game sound as it
// really is, past PortCustomSFXReplace.
extern "C" uint32_t sndFXStartEx(uint16_t fid, uint8_t vol, uint8_t pan, uint8_t studio);
extern "C" uint32_t sndFXCheck(uint32_t vid);
extern "C" unsigned long sndFXKeyOff(uint32_t vid);
extern "C" void sndMasterVolume(uint8_t volume, uint16_t time, uint8_t music, uint8_t fx);
// hw_pc.c: runs the whole engine for one 5 ms tick and returns that tick's stereo s16 buffer.
extern "C" void* salPortNextBuffer(void);
extern "C" unsigned int salPortBufferBytes(void);

std::string g_sfxDir;
bool g_haveDir = false;
// Keyed by file path. Clips are never freed, and live behind unique_ptr so a
// Voice's pointer stays valid however many more get loaded.
std::vector<std::pair<std::string, std::unique_ptr<Clip>>> g_clips; // small; linear lookup is fine
Voice g_voices[kMaxVoices];
// Guards g_voices between the game thread (Play) and the audio thread (Mix).
std::mutex g_voiceLock;

// sfx/sonidos.txt, by sound name; re-read whenever its contents change.
std::unordered_map<std::string, Replacement> g_replacements;
std::string g_configText;
std::chrono::steady_clock::time_point g_nextConfigCheck;
// The game's sound names by MusyX SFX id (from SebringSoundDefines), for the
// generic replacement in PortCustomSFXReplace.
std::vector<std::string> g_soundNames;

bool ReadU32LE(FILE* f, uint32_t* out)
{
    unsigned char b[4];
    if (std::fread(b, 1, 4, f) != 4)
        return false;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

bool ReadU16LE(FILE* f, uint16_t* out)
{
    unsigned char b[2];
    if (std::fread(b, 1, 2, f) != 2)
        return false;
    *out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return true;
}

// Minimal PCM WAV reader: 8/16/24/32-bit integer, any channel count or rate.
// Converts to kMixChannels/kMixSampleRate signed 16-bit as it goes. Floors
// anything it does not understand (compressed formats, missing chunks)
// rather than guessing.
bool LoadWav(const std::string& path, std::vector<int16_t>* out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
        return false;

    char riff[4], wave[4];
    uint32_t riffSize;
    bool ok = std::fread(riff, 1, 4, f) == 4 && ReadU32LE(f, &riffSize)
              && std::fread(wave, 1, 4, f) == 4 && std::memcmp(riff, "RIFF", 4) == 0
              && std::memcmp(wave, "WAVE", 4) == 0;
    if (!ok)
    {
        std::fclose(f);
        return false;
    }

    uint16_t formatTag = 0, channels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    std::vector<unsigned char> data;
    bool haveFmt = false;

    for (;;)
    {
        char id[4];
        uint32_t chunkSize;
        if (std::fread(id, 1, 4, f) != 4 || !ReadU32LE(f, &chunkSize))
            break;

        if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            uint16_t blockAlign, extra = 0;
            uint32_t byteRate;
            ok = ReadU16LE(f, &formatTag) && ReadU16LE(f, &channels) && ReadU32LE(f, &sampleRate)
                 && ReadU32LE(f, &byteRate) && ReadU16LE(f, &blockAlign)
                 && ReadU16LE(f, &bitsPerSample);
            long consumed = 16;
            if (ok && chunkSize > 16)
            {
                // cbSize + extension, for WAVE_FORMAT_EXTENSIBLE and the like; skipped.
                if (chunkSize >= 18 && ReadU16LE(f, &extra))
                    consumed = 18;
            }
            if (!ok)
                break;
            if (chunkSize > (uint32_t)consumed)
                std::fseek(f, (long)(chunkSize - (uint32_t)consumed), SEEK_CUR);
            haveFmt = true;
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            data.resize(chunkSize);
            if (chunkSize != 0 && std::fread(data.data(), 1, chunkSize, f) != chunkSize)
            {
                std::fclose(f);
                return false;
            }
            // Sound data normally comes after fmt; stop once we have both.
            if (haveFmt)
                break;
        }
        else
        {
            std::fseek(f, (long)chunkSize, SEEK_CUR);
        }
        if (chunkSize & 1)
            std::fseek(f, 1, SEEK_CUR); // chunks are word-aligned
    }
    std::fclose(f);

    if (!haveFmt || data.empty() || channels == 0 || sampleRate == 0)
        return false;
    if (formatTag != 1 /* PCM */ && formatTag != 0xFFFE /* EXTENSIBLE, assumed PCM */)
    {
        std::fprintf(stderr, "[port] custom_sfx: %s is not integer PCM (format tag %u), skipping\n",
                     path.c_str(), (unsigned)formatTag);
        return false;
    }
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
    {
        std::fprintf(stderr, "[port] custom_sfx: %s has unsupported bit depth %u, skipping\n",
                     path.c_str(), (unsigned)bitsPerSample);
        return false;
    }

    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t frameBytes = bytesPerSample * channels;
    const size_t srcFrames = frameBytes != 0 ? data.size() / frameBytes : 0;

    // Decode to interleaved float in [-1, 1], mixed down/up to kMixChannels.
    std::vector<float> mono; // per source frame, already channel-mixed
    mono.resize(srcFrames * kMixChannels);
    for (size_t i = 0; i < srcFrames; i++)
    {
        for (int ch = 0; ch < kMixChannels; ch++)
        {
            const int srcCh = (channels == 1) ? 0 : (ch % channels);
            const unsigned char* p = &data[i * frameBytes + (size_t)srcCh * bytesPerSample];
            float v = 0.0f;
            if (bitsPerSample == 8)
            {
                v = ((int)p[0] - 128) / 128.0f; // WAV 8-bit PCM is unsigned
            }
            else if (bitsPerSample == 16)
            {
                int16_t s = (int16_t)(p[0] | (p[1] << 8));
                v = s / 32768.0f;
            }
            else if (bitsPerSample == 24)
            {
                int32_t s = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16));
                if (s & 0x00800000)
                    s |= (int32_t)0xFF000000;
                v = s / 8388608.0f;
            }
            else // 32
            {
                int32_t s = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
                v = s / 2147483648.0f;
            }
            mono[i * kMixChannels + ch] = v;
        }
    }

    // Linear-interpolated resample to kMixSampleRate; a no-op copy when the
    // rates already match, which is the common case if clips are exported at
    // 32000 Hz to begin with.
    const size_t dstFrames = sampleRate == (uint32_t)kMixSampleRate
                                  ? srcFrames
                                  : (size_t)((double)srcFrames * kMixSampleRate / sampleRate);
    out->resize(dstFrames * kMixChannels);
    for (size_t i = 0; i < dstFrames; i++)
    {
        const double srcPos = sampleRate == (uint32_t)kMixSampleRate
                                   ? (double)i
                                   : (double)i * sampleRate / kMixSampleRate;
        size_t i0 = (size_t)srcPos;
        if (i0 >= srcFrames)
            i0 = srcFrames > 0 ? srcFrames - 1 : 0;
        size_t i1 = (i0 + 1 < srcFrames) ? i0 + 1 : i0;
        const float frac = (float)(srcPos - (double)i0);
        for (int ch = 0; ch < kMixChannels; ch++)
        {
            const float a = mono[i0 * kMixChannels + ch];
            const float b = mono[i1 * kMixChannels + ch];
            const float v = a + (b - a) * frac;
            int32_t s = (int32_t)(v * 32767.0f);
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            (*out)[i * kMixChannels + ch] = (int16_t)s;
        }
    }
    return true;
}

Clip* FindOrLoad(const std::string& path)
{
    for (auto& kv : g_clips)
    {
        if (kv.first == path)
            return kv.second.get();
    }
    g_clips.push_back({path, std::unique_ptr<Clip>(new Clip())});
    Clip* c = g_clips.back().second.get();
    if (!g_haveDir)
        return c;

    FILE* probe = std::fopen(path.c_str(), "rb");
    if (probe == nullptr)
        return c; // nothing dropped in for this sound: not an error
    std::fclose(probe);
    c->exists = true;

    if (LoadWav(path, &c->samples))
    {
        c->loaded = true;
        std::fprintf(stderr, "[port] custom_sfx: loaded %s (%zu frames)\n", path.c_str(),
                     c->samples.size() / kMixChannels);
    }
    else
    {
        std::fprintf(stderr, "[port] custom_sfx: %s exists but could not be decoded\n",
                     path.c_str());
    }
    return c;
}

std::string Trim(const std::string& text)
{
    size_t a = 0, b = text.size();
    while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r' || text[a] == '\n'))
        a++;
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\r' || text[b - 1] == '\n'))
        b--;
    return text.substr(a, b - a);
}

// sfx/sonidos.txt, one replacement per line ('#' starts a comment):
//   SOUND_NAME = file.wav, volume%, on|off
// The volume (0-200, default 100) and on/off (default on) are optional.
void ParseConfig(const std::string& text)
{
    g_replacements.clear();
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        const size_t hash = line.find('#');
        if (hash != std::string::npos)
            line.resize(hash);
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string name = Trim(line.substr(0, eq));
        std::vector<std::string> fields;
        std::string rest = line.substr(eq + 1);
        size_t pos = 0;
        for (;;)
        {
            const size_t comma = rest.find(',', pos);
            fields.push_back(Trim(rest.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos)));
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
        if (name.empty() || fields.empty() || fields[0].empty())
            continue;
        Replacement r;
        r.file = fields[0];
        if (fields.size() > 1 && !fields[1].empty())
        {
            float volume = (float)std::atof(fields[1].c_str());
            if (volume < 0.0f) volume = 0.0f;
            if (volume > 200.0f) volume = 200.0f;
            r.gain = volume / 100.0f;
        }
        if (fields.size() > 2)
            r.on = !(fields[2] == "off" || fields[2] == "OFF" || fields[2] == "0" || fields[2] == "no");
        g_replacements[name] = r;
    }
}

std::string ClipPath(const std::string& file)
{
    return g_sfxDir + "/" + file;
}

// Re-reads sfx/sonidos.txt at most once a second (it is tiny), and loads the
// clips it names right away rather than on their first play.
void RefreshConfig()
{
    if (!g_haveDir)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextConfigCheck)
        return;
    g_nextConfigCheck = now + std::chrono::seconds(1);

    std::string text;
    if (FILE* f = std::fopen((g_sfxDir + "/sonidos.txt").c_str(), "rb"))
    {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            text.append(buf, n);
        std::fclose(f);
    }
    if (text == g_configText)
        return;
    g_configText = text;
    ParseConfig(text);
    for (const auto& kv : g_replacements)
        if (kv.second.on && kv.second.file != "-")
            FindOrLoad(ClipPath(kv.second.file));
}

int StartClip(Clip* c, float minScale, float gain)
{
    if (c == nullptr || !c->exists)
        return 0;

    if (c->loaded && !c->samples.empty() && gain > 0.0f)
    {
        std::lock_guard<std::mutex> lock(g_voiceLock);
        Voice* slot = &g_voices[0]; // all voices busy: steal slot 0 rather than drop the cue
        for (auto& v : g_voices)
        {
            if (v.clip == nullptr)
            {
                slot = &v;
                break;
            }
        }
        slot->clip = c;
        slot->framePos = 0.0;
        slot->minScale = minScale;
        slot->gain = gain;
    }
    return 1;
}

int PlayInternal(const char* name, float minScale)
{
    if (name == nullptr || *name == '\0')
        return 0;

    RefreshConfig();
    auto it = g_replacements.find(name);
    if (it != g_replacements.end())
    {
        // Listed in sonidos.txt: off means "use the game's own sound".
        const Replacement& r = it->second;
        if (!r.on)
            return 0;
        if (r.file == "-")
            return 1;
        return StartClip(FindOrLoad(ClipPath(r.file)), minScale, r.gain);
    }
    return StartClip(FindOrLoad(ClipPath(std::string(name) + ".wav")), minScale, 1.0f);
}

} // namespace

static void PreloadModMomentClips();

extern "C" void PortCustomSFXInit(void)
{
    char dir[1024];
    if (port_executable_dir(dir, sizeof dir) != 0)
    {
        g_haveDir = false;
        return;
    }
    g_sfxDir = std::string(dir) + "/sfx";
    g_haveDir = true;
    for (auto& v : g_voices)
        v.clip = nullptr;
    g_nextConfigCheck = std::chrono::steady_clock::time_point();
    RefreshConfig();
    PreloadModMomentClips();
}

extern "C" void PortCustomSFXRegisterName(unsigned long sfxID, const char* name)
{
    if (name == nullptr || sfxID >= 0x10000)
        return;
    if (g_soundNames.size() <= sfxID)
        g_soundNames.resize(sfxID + 1);
    g_soundNames[sfxID] = name;
}

extern "C" void PortCustomSFXNamesReady(void)
{
    if (!g_haveDir)
        return;
    // The list of names sonidos.txt can use, for whoever is editing it (the
    // companion app reads it for its picker). Missing sfx/ folder: nothing to do.
    FILE* f = std::fopen((g_sfxDir + "/_lista_de_sonidos.txt").c_str(), "wb");
    if (f == nullptr)
        return;
    for (const std::string& name : g_soundNames)
        if (!name.empty())
            std::fprintf(f, "%s\n", name.c_str());
    std::fclose(f);
}

// Every captain's Super Strike / HyperStrike voice lines (SFXCHAR_<CAPTAIN>_EFFORTS_Windup_Super_01,
// ..._Kick_Super_01, SFXCHAR_HyperStrike_*): a replacement for one plays at normal speed like the
// lines PortCustomSFXPlayFixed plays, however slow the cinematic runs.
static bool IsSuperStrikeLine(const std::string& name)
{
    return name.find("_Windup_Super_") != std::string::npos || name.find("_Kick_Super") != std::string::npos
        || name.find("HyperStrike") != std::string::npos || name.find("_Hyper") != std::string::npos;
}

// The sound editor's preview requests: sfx/_preview.txt holds "<nonce> orig|custom <NAME>"; the
// game plays it, deletes the request and answers in sfx/_preview_result.txt with "<nonce> <result>":
// ok, noload (that sound's group isn't loaded right now - most are only during a match), nofile
// (no .wav assigned, or it doesn't decode) or unknown (no such sound name).
static std::chrono::steady_clock::time_point g_nextPreviewCheck;

static const char* Preview(const std::string& kind, const std::string& name)
{
    if (kind == "orig")
    {
        for (size_t id = 0; id < g_soundNames.size(); id++)
        {
            if (g_soundNames[id] != name)
                continue;
            return sndFXStartEx((uint16_t)id, 127, 64, 0) != 0xFFFFFFFFu ? "ok" : "noload";
        }
        return "unknown";
    }
    RefreshConfig();
    auto it = g_replacements.find(name);
    if (it == g_replacements.end() || it->second.file == "-")
        return "nofile";
    Clip* c = FindOrLoad(ClipPath(it->second.file));
    if (c == nullptr || !c->loaded)
        return "nofile";
    StartClip(c, kVoiceLineMinScale, it->second.gain);
    return "ok";
}

extern "C" void PortCustomSFXPoll(void)
{
    if (!g_haveDir)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextPreviewCheck)
        return;
    g_nextPreviewCheck = now + std::chrono::milliseconds(150);

    const std::string path = g_sfxDir + "/_preview.txt";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
        return;
    char buf[256];
    const size_t n = std::fread(buf, 1, sizeof buf - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    std::remove(path.c_str());

    char nonce[64], kind[16], name[160];
    if (std::sscanf(buf, "%63s %15s %159s", nonce, kind, name) != 3)
        return;
    const char* result = Preview(kind, name);
    if (FILE* out = std::fopen((g_sfxDir + "/_preview_result.txt").c_str(), "wb"))
    {
        std::fprintf(out, "%s %s\n", nonce, result);
        std::fclose(out);
    }
}

// The clips the mod plays itself, at one exact moment (a captain's Super Strike windup or kick,
// Bowser's landing, the ball off the post, ...), through PortCustomSFXPlay/PlayFixed. Some share
// their name with a sound the game also plays at other times (SFXCHAR_MARIO_EFFORTS_Kick_Super_01
// on any charged shot, SFXCHAR_BOWSER_Activate when Bowser shows up, ...): a sonidos.txt line for
// one only ever sets the file and volume of the mod's own moment, never those.
static const char* const kModClips[] = {
    "SFXBALL_Post_Metal",
    "SFXCHAR_BOWSER_Activate",
    "SFXCHAR_DAISY_EFFORTS_Kick_Super_01",
    "SFXCHAR_DAISY_EFFORTS_Windup_Super_01",
    "SFXCHAR_HyperStrike_Charge_01",
    "SFXCHAR_HyperStrike_Shot_01",
    "SFXCHAR_MARIO_EFFORTS_Kick_Super_01",
    "SFXCHAR_MARIO_EFFORTS_Windup_Super_01",
    "SFXCHAR_MYSTERY_EFFORTS_Electrocute_01",
    "SFXCHAR_Tackle_01",
    "SFXCHAR_WARIO_EFFORTS_Electrocute_01",
};

static bool IsModMomentClip(const std::string& name)
{
    for (const char* clip : kModClips)
        if (name == clip)
            return true;
    return false;
}

// Loads the mod's own clips at start-up instead of on their first play, which decoded the .wav
// mid-match (a hitch right at a Super Strike). sonidos.txt's are already loaded by RefreshConfig.
static void PreloadModMomentClips()
{
    for (const char* clip : kModClips)
        if (g_replacements.find(clip) == g_replacements.end())
            FindOrLoad(ClipPath(std::string(clip) + ".wav"));
}

extern "C" int PortCustomSFXReplace(unsigned long sfxID)
{
    if (sfxID >= g_soundNames.size() || g_soundNames[sfxID].empty() || IsModMomentClip(g_soundNames[sfxID]))
        return 0;
    RefreshConfig();
    auto it = g_replacements.find(g_soundNames[sfxID]);
    if (it == g_replacements.end() || !it->second.on)
        return 0;
    if (it->second.file == "-")
        return 1;
    Clip* c = FindOrLoad(ClipPath(it->second.file));
    if (c == nullptr || !c->loaded)
        return 0; // a file that isn't there or won't decode: keep the game's sound
    return StartClip(c, IsSuperStrikeLine(g_soundNames[sfxID]) ? kVoiceLineMinScale : kSlowMoFloor, it->second.gain);
}

extern "C" int PortCustomSFXPlay(const char* name)
{
    // Tracks the sim's slow motion like the game's own sounds do, but never
    // below kSlowMoFloor (see there). At normal speed the time scale is 1.0,
    // so outside slow-mo nothing changes.
    return PlayInternal(name, kSlowMoFloor);
}

extern "C" int PortCustomSFXPlayFixed(const char* name)
{
    return PlayInternal(name, kVoiceLineMinScale);
}

extern "C" void PortCustomSFXMix(short* pcm, unsigned int frames)
{
    if (pcm == nullptr || frames == 0)
        return;

    // Same clamp range the pitch/filter fades in Audio::FadeFilterTo* use in
    // practice; guards against a stray 0 or a runaway value doing something
    // silly to playback rate.
    float scale = 1.0f;
    if (float* pScale = PortSimTimeScalePtr())
        scale = *pScale;
    if (!(scale > 0.05f)) scale = 0.05f; // also catches NaN
    if (scale > 4.0f) scale = 4.0f;

    std::lock_guard<std::mutex> lock(g_voiceLock);
    for (auto& v : g_voices)
    {
        if (v.clip == nullptr)
            continue;
        const std::vector<int16_t>& s = v.clip->samples;
        const size_t total = s.size() / kMixChannels;
        if (total == 0)
        {
            v.clip = nullptr;
            continue;
        }
        const float step = (scale < v.minScale) ? v.minScale : scale;
        for (unsigned int i = 0; i < frames && v.framePos < (double)total; i++)
        {
            const size_t i0 = (size_t)v.framePos;
            const size_t i1 = (i0 + 1 < total) ? i0 + 1 : i0;
            const float frac = (float)(v.framePos - (double)i0);
            for (int ch = 0; ch < kMixChannels; ch++)
            {
                const float a = (float)s[i0 * kMixChannels + ch];
                const float b = (float)s[i1 * kMixChannels + ch];
                const int32_t sample = (int32_t)((a + (b - a) * frac) * v.gain);
                const int32_t mixed = (int32_t)pcm[i * kMixChannels + ch] + sample;
                int32_t clamped = mixed;
                if (clamped > 32767) clamped = 32767;
                if (clamped < -32768) clamped = -32768;
                pcm[i * kMixChannels + ch] = (int16_t)clamped;
            }
            v.framePos += (double)step;
        }
        if (v.framePos >= (double)total)
            v.clip = nullptr; // done
    }
}

// ---- Extracting the game's own sounds (STRIKERS_DUMP_SFX) ----
//
// The sound editor's "Extraer sonidos originales": the game is started once with STRIKERS_DUMP_SFX
// set, loads its sound groups one after another (AudioLoader's PortSFXDumpIfRequested), and for
// each group renders every sound it holds on its own, by driving MusyX tick by tick rather than
// through the audio device, into sfx/_originales/<NAME>.wav. The editor then plays those directly.

namespace
{

std::vector<bool> g_dumped;
std::string g_dumpDir;
int g_dumpCount = 0;

constexpr uint32_t kNoVoice = 0xFFFFFFFFu;
constexpr int kTicksPerSecond = 200; // 5 ms ticks
constexpr int kMaxSoundTicks = 8 * kTicksPerSecond; // looping sounds get cut here
constexpr int kTailTicks = 30; // the voice ends a little before its last audio leaves the buffers

bool IsSilent(const int16_t* pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++)
        if (pcm[i] > 8 || pcm[i] < -8)
            return false;
    return true;
}

// Runs MusyX until its output has been silent for a few ticks, so one sound's tail doesn't end up
// at the start of the next.
void Drain()
{
    const size_t samples = salPortBufferBytes() / sizeof(int16_t);
    int quiet = 0;
    for (int t = 0; t < 2 * kTicksPerSecond && quiet < 4; t++)
    {
        const int16_t* buf = (const int16_t*)salPortNextBuffer();
        quiet = (buf == nullptr || IsSilent(buf, samples)) ? quiet + 1 : 0;
    }
}

bool WriteWav(const std::string& path, const std::vector<int16_t>& pcm)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        return false;
    auto u32 = [f](uint32_t v) {
        unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
        std::fwrite(b, 1, 4, f);
    };
    auto u16 = [f](uint16_t v) {
        unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) };
        std::fwrite(b, 1, 2, f);
    };
    const uint32_t bytes = (uint32_t)(pcm.size() * sizeof(int16_t));
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(kMixChannels);
    u32(kMixSampleRate);
    u32(kMixSampleRate * kMixChannels * 2);
    u16(kMixChannels * 2);
    u16(16);
    std::fwrite("data", 1, 4, f);
    u32(bytes);
    for (int16_t v : pcm)
        u16((uint16_t)v);
    std::fclose(f);
    return true;
}

// Plays sound `id` alone and records it. False if it isn't in a loaded group (or made no sound).
bool RenderOne(uint16_t id, const std::string& path)
{
    const uint32_t voice = sndFXStartEx(id, 127, 64, 0);
    if (voice == kNoVoice)
        return false;

    const size_t samples = salPortBufferBytes() / sizeof(int16_t);
    std::vector<int16_t> pcm;
    int afterEnd = -1;
    bool keyedOff = false;
    for (int t = 0;; t++)
    {
        const int16_t* buf = (const int16_t*)salPortNextBuffer();
        if (buf == nullptr)
            break;
        pcm.insert(pcm.end(), buf, buf + samples);
        if (afterEnd < 0 && sndFXCheck(voice) == kNoVoice)
            afterEnd = 0;
        if (afterEnd >= 0 && ++afterEnd > kTailTicks)
            break;
        if (t >= kMaxSoundTicks)
        {
            // A looping sound: release it, and record its release too.
            if (!keyedOff)
            {
                sndFXKeyOff(voice);
                keyedOff = true;
            }
            if (t >= kMaxSoundTicks + kTicksPerSecond)
                break;
        }
    }
    Drain();

    // Trim the silence at both ends (the buffers run a couple of ticks behind the engine).
    size_t first = 0, last = pcm.size();
    while (first < last && pcm[first] > -8 && pcm[first] < 8)
        first++;
    while (last > first && pcm[last - 1] > -8 && pcm[last - 1] < 8)
        last--;
    first -= first % kMixChannels;
    last += (kMixChannels - last % kMixChannels) % kMixChannels;
    if (last > pcm.size())
        last = pcm.size();
    if (first >= last)
        return true; // started, but silent: nothing worth a file
    std::vector<int16_t> trimmed(pcm.begin() + (long)first, pcm.begin() + (long)last);
    return WriteWav(path, trimmed);
}

void WriteProgress(const char* state)
{
    if (FILE* f = std::fopen((g_dumpDir + "/_progreso.txt").c_str(), "wb"))
    {
        std::fprintf(f, "%s %d\n", state, g_dumpCount);
        std::fclose(f);
    }
}

} // namespace

extern "C" int PortCustomSFXDumpRequested(void)
{
    const char* e = std::getenv("STRIKERS_DUMP_SFX");
    return (e != nullptr && *e != '\0' && g_haveDir) ? 1 : 0;
}

extern "C" void PortCustomSFXDumpBegin(void)
{
    g_dumpDir = g_sfxDir + "/_originales";
    // sfx/ itself is there (g_haveDir); an existing _originales/ is fine.
#if defined(_WIN32)
    _mkdir(g_dumpDir.c_str());
#else
    mkdir(g_dumpDir.c_str(), 0755);
#endif
    g_dumped.assign(g_soundNames.size(), false);
    g_dumpCount = 0;
    sndMasterVolume(127, 0, 1, 1); // whatever the options menu set, record at full level
    Drain();
    WriteProgress("trabajando");
}

extern "C" void PortCustomSFXDumpLoaded(void)
{
    for (size_t id = 0; id < g_soundNames.size(); id++)
    {
        if (g_dumped[id] || g_soundNames[id].empty())
            continue;
        if (RenderOne((uint16_t)id, g_dumpDir + "/" + g_soundNames[id] + ".wav"))
        {
            g_dumped[id] = true;
            g_dumpCount++;
        }
    }
    WriteProgress("trabajando");
}

extern "C" void PortCustomSFXDumpEnd(void)
{
    WriteProgress("listo");
    std::fprintf(stderr, "[port] custom_sfx: extracted %d sounds into %s\n", g_dumpCount, g_dumpDir.c_str());
}
