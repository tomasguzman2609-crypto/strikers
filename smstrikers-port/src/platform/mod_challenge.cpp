// See include/port/mod_challenge.h.

#include "port/mod_challenge.h"
#include "port/host.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

// Re-stat the file at most this often; a spin only needs to land within a fraction of a second,
// and stat'ing a tiny file every single frame is needless I/O.
constexpr int kPollEveryNFrames = 30;

std::string g_path;
bool g_havePath = false;
int g_pollCountdown = 0;
int g_activeChallenge = PORT_CHALLENGE_NONE;

// mtime isn't exposed by port_host.h, so change detection is by content instead: cheap enough for
// a one-line file, and it sidesteps filesystems with coarse mtime resolution.
std::string g_lastContent;

bool ReadFile(std::string& out)
{
    FILE* f = fopen(g_path.c_str(), "rb");
    if (f == nullptr)
        return false;
    char buf[64];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    out.assign(buf);
    return true;
}

void WriteFile(const char* content)
{
    FILE* f = fopen(g_path.c_str(), "wb");
    if (f == nullptr)
        return;
    fputs(content, f);
    fclose(f);
}

int ParseChallenge(const std::string& content)
{
    int value = atoi(content.c_str());
    if (value < PORT_CHALLENGE_NONE || value > PORT_CHALLENGE_OPPONENT_DOUBLE_GOALS)
        return PORT_CHALLENGE_NONE;
    return value;
}

} // namespace

extern "C" void PortModChallengeInit(void)
{
    char dir[1024];
    if (port_executable_dir(dir, sizeof dir) != 0)
    {
        g_havePath = false;
        return;
    }
    g_path = std::string(dir) + "/mods/challenge.txt";
    g_havePath = true;
    g_pollCountdown = 0;
    g_activeChallenge = PORT_CHALLENGE_NONE;
    g_lastContent.clear();

    std::string content;
    if (ReadFile(content))
    {
        g_lastContent = content;
        g_activeChallenge = ParseChallenge(content);
    }
}

extern "C" void PortModChallengeUpdate(void)
{
    if (!g_havePath)
        return;
    if (g_pollCountdown > 0)
    {
        g_pollCountdown--;
        return;
    }
    g_pollCountdown = kPollEveryNFrames;

    std::string content;
    if (!ReadFile(content))
        return; // no mods/challenge.txt yet (or it was deleted) - leave the last known state alone
    if (content == g_lastContent)
        return;
    g_lastContent = content;
    g_activeChallenge = ParseChallenge(content);
}

extern "C" int PortModChallengeGetActive(void)
{
    return g_activeChallenge;
}

extern "C" void PortModChallengeClear(void)
{
    g_activeChallenge = PORT_CHALLENGE_NONE;
    g_lastContent = "0";
    if (g_havePath)
        WriteFile("0");
}
