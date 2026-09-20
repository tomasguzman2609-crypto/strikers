// See include/port/custom_sfx.h.

#include "port/custom_sfx.h"
#include "port/host.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

// Must match the mixer's own output format in src/platform/audio_out.cpp
// (kSampleRate / kChannels). Clips are converted to this once, at load time,
// so the per-buffer mix stays a plain add.
constexpr int kMixSampleRate = 32000;
constexpr int kMixChannels = 2;
constexpr int kMaxVoices = 4;

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

std::string g_sfxDir;
bool g_haveDir = false;
std::vector<std::pair<std::string, Clip>> g_clips; // small; linear lookup is fine
Voice g_voices[kMaxVoices];

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

Clip* FindOrLoad(const char* name)
{
    for (auto& kv : g_clips)
    {
        if (kv.first == name)
            return &kv.second;
    }
    g_clips.push_back({std::string(name), Clip{}});
    Clip* c = &g_clips.back().second;
    if (!g_haveDir)
        return c;

    const std::string path = g_sfxDir + "/" + name + ".wav";
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

} // namespace

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
    g_clips.clear();
    for (auto& v : g_voices)
        v.clip = nullptr;
}

extern "C" int PortCustomSFXPlay(const char* name)
{
    if (name == nullptr || *name == '\0')
        return 0;

    Clip* c = FindOrLoad(name);
    if (c == nullptr || !c->exists)
        return 0;

    if (c->loaded && !c->samples.empty())
    {
        for (auto& v : g_voices)
        {
            if (v.clip == nullptr)
            {
                v.clip = c;
                v.framePos = 0.0;
                return 1;
            }
        }
        // All voices busy: steal the oldest-looking slot (index 0) rather than drop the cue.
        g_voices[0].clip = c;
        g_voices[0].framePos = 0.0;
    }
    return 1;
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
        for (unsigned int i = 0; i < frames && v.framePos < (double)total; i++)
        {
            const size_t i0 = (size_t)v.framePos;
            const size_t i1 = (i0 + 1 < total) ? i0 + 1 : i0;
            const float frac = (float)(v.framePos - (double)i0);
            for (int ch = 0; ch < kMixChannels; ch++)
            {
                const float a = (float)s[i0 * kMixChannels + ch];
                const float b = (float)s[i1 * kMixChannels + ch];
                const int32_t sample = (int32_t)(a + (b - a) * frac);
                const int32_t mixed = (int32_t)pcm[i * kMixChannels + ch] + sample;
                int32_t clamped = mixed;
                if (clamped > 32767) clamped = 32767;
                if (clamped < -32768) clamped = -32768;
                pcm[i * kMixChannels + ch] = (int16_t)clamped;
            }
            v.framePos += (double)scale;
        }
        if (v.framePos >= (double)total)
            v.clip = nullptr; // done
    }
}
