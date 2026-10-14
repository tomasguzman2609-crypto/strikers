// See include/port/mod_challenge.h.

#include "port/mod_challenge.h"
#include "port/host.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

namespace
{

// Re-stat the file at most this often; a spin only needs to land within a fraction of a second,
// and stat'ing a tiny file every single frame is needless I/O.
constexpr int kPollEveryNFrames = 30;

std::string g_path;
std::string g_modsDir;
bool g_havePath = false;
int g_pollCountdown = 0;
int g_activeChallenge = PORT_CHALLENGE_NONE;

// mtime isn't exposed by port_host.h, so change detection is by content instead: cheap enough for
// a one-line file, and it sidesteps filesystems with coarse mtime resolution.
std::string g_lastContent;

PortModChallengeActionFilter g_actionFilter = nullptr;

// What PORT_CHALLENGE_ROTATING_CURSE has currently drawn (PORT_CHALLENGE_NONE when it isn't active).
int g_rotatingDraw = PORT_CHALLENGE_NONE;

unsigned int g_matchFrames = 0;

// A private generator, so drawing a challenge doesn't advance the game's own random seed.
unsigned int g_rng = 0;

unsigned int NextRandom()
{
    if (g_rng == 0)
        g_rng = (unsigned int)time(nullptr) | 1u;
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 8;
}

void DrawRotating()
{
    // Any real challenge other than the meta one itself, and not the one just drawn.
    const int first = PORT_CHALLENGE_NONE + 1;
    const int count = PORT_CHALLENGE_COUNT - first;
    int pick;
    do
    {
        pick = first + (int)(NextRandom() % (unsigned int)count);
    } while (pick == PORT_CHALLENGE_ROTATING_CURSE || (pick == g_rotatingDraw && count > 2));
    g_rotatingDraw = pick;
}

// Called whenever the value read from challenge.txt changes.
void SetSelected(int value)
{
    g_activeChallenge = value;
    if (value == PORT_CHALLENGE_ROTATING_CURSE)
        DrawRotating();
    else
        g_rotatingDraw = PORT_CHALLENGE_NONE;
}

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
    if (value < PORT_CHALLENGE_NONE || value >= PORT_CHALLENGE_COUNT)
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
    g_modsDir = std::string(dir) + "/mods";
    g_path = g_modsDir + "/challenge.txt";
    g_havePath = true;
    g_pollCountdown = 0;
    g_activeChallenge = PORT_CHALLENGE_NONE;
    g_lastContent.clear();

    std::string content;
    if (ReadFile(content))
    {
        g_lastContent = content;
        SetSelected(ParseChallenge(content));
    }

    // Whatever the last run left behind (e.g. a crash mid-match), no match is being played yet.
    PortModChallengeSetMatchPlaying(0);
}

extern "C" void PortModChallengeAwardSkip(void)
{
    if (!g_havePath)
        return;
    const std::string path = g_modsDir + "/skips.txt";

    // Read-modify-write: the companion app may have spent some since the last award.
    int skips = 0;
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        char buf[32];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        skips = atoi(buf);
        if (skips < 0)
            skips = 0;
    }
    if (FILE* f = fopen(path.c_str(), "wb"))
    {
        fprintf(f, "%d", skips + 1);
        fclose(f);
    }
}

extern "C" void PortModChallengeSetMatchPlaying(int playing)
{
    if (!g_havePath)
        return;
    if (FILE* f = fopen((g_modsDir + "/match_status.txt").c_str(), "wb"))
    {
        fputs(playing ? "1" : "0", f);
        fclose(f);
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
    SetSelected(ParseChallenge(content));
}

extern "C" int PortModChallengeGetActive(void)
{
    if (g_activeChallenge == PORT_CHALLENGE_ROTATING_CURSE)
        return g_rotatingDraw;
    return g_activeChallenge;
}

extern "C" int PortModChallengeGetSelected(void)
{
    return g_activeChallenge;
}

extern "C" void PortModChallengeReroll(void)
{
    if (g_activeChallenge == PORT_CHALLENGE_ROTATING_CURSE)
        DrawRotating();
}

extern "C" const char* PortModChallengeName(int challenge)
{
    // UTF-8, in ID order; must match the companion app's CHALLENGES list.
    static const char* const kNames[PORT_CHALLENGE_COUNT] = {
        "",
        "Solo champiñones",
        "Controles invertidos",
        "Sin items",
        "Items infinitos",
        "Velocidad reducida",
        "Velocidad aumentada",
        "Sin centros",
        "Barrida Nerfeada",
        "Sin Tackles",
        "Rival Más Rápido",
        "Rival con Chomps y Estrellas",
        "Electrocución Larga",
        "Modo Globito",
        "Shocks Cortos",
        "Gol Doble Rival",
        "Shock Fatal",
        "Capitán Egoísta",
        "Ventaja Oponente = Perdiste",
        "Bowser Agresivo",
        "Papa Caliente",
        "Tackles Eléctricos",
        "Maldición Rotativa",
        "Estrella Intermitente",
        "Esquive Estelar",
        "Arquero de Papel",
        "Chomp Árbitro",
        "Modo VAR",
        "Arquero Reflectante",
        "Jogo Bonito",
    };
    if (challenge <= PORT_CHALLENGE_NONE || challenge >= PORT_CHALLENGE_COUNT)
        return "";
    return kNames[challenge];
}

extern "C" void PortModChallengeNoteMatchFrame(void)
{
    g_matchFrames++;
}

extern "C" unsigned int PortModChallengeMatchFrameCount(void)
{
    return g_matchFrames;
}

static int g_meterProgress = 0;
static int g_meterTarget = 0;

extern "C" void PortModChallengeSetMeter(int progress, int target)
{
    g_meterProgress = progress;
    g_meterTarget = target;
}

extern "C" void PortModChallengeGetMeter(int* progress, int* target)
{
    *progress = g_meterProgress;
    *target = g_meterTarget;
}

static unsigned int g_varReviews = 0;
static int g_varLastDisallowed = 0;

extern "C" void PortModChallengeVarVerdict(int disallowed)
{
    g_varLastDisallowed = disallowed;
    g_varReviews++;
}

extern "C" unsigned int PortModChallengeVarReviewCount(void)
{
    return g_varReviews;
}

extern "C" int PortModChallengeVarLastDisallowed(void)
{
    return g_varLastDisallowed;
}

extern "C" void PortModChallengeSetActionFilter(PortModChallengeActionFilter filter)
{
    g_actionFilter = filter;
}

extern "C" int PortModChallengeActionBlocked(int padIndex, int action)
{
    if (PortModChallengeGetActive() == PORT_CHALLENGE_NONE || g_actionFilter == nullptr)
        return 0;
    return g_actionFilter(padIndex, action);
}

extern "C" void PortModChallengeClear(void)
{
    g_activeChallenge = PORT_CHALLENGE_NONE;
    g_rotatingDraw = PORT_CHALLENGE_NONE;
    g_lastContent = "0";
    if (g_havePath)
        WriteFile("0");
}
