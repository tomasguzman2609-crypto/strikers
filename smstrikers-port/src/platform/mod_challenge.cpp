// See include/port/mod_challenge.h.

#include "port/mod_challenge.h"
#include "port/host.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// The Challenge Super Bowser Cup's curse for the match being played (PORT_CHALLENGE_NONE outside
// one). While set, it stands in for whatever challenge.txt selects.
int g_cupCurse = PORT_CHALLENGE_NONE;
// And whether that match has the cup's "Compañeros únicos rival" switch on.
bool g_cupSidekicksThisMatch = false;

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

// What actually applies: the cup's curse during a Challenge Super Bowser Cup match, else the file's.
int EffectiveSelected()
{
    return g_cupCurse != PORT_CHALLENGE_NONE ? g_cupCurse : g_activeChallenge;
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
    if (g_cupCurse != PORT_CHALLENGE_NONE)
        return; // the cup's curse rules this match; the file's pick waits for the next one
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

static int g_extraRivalSidekicks = 0;

// mods/extras.txt: "name=value" lines; a missing file or line means off.
static void ReadExtras(void)
{
    int rivalSidekicks = 0;
    if (FILE* f = fopen((g_modsDir + "/extras.txt").c_str(), "rb"))
    {
        char line[128];
        while (fgets(line, sizeof line, f) != nullptr)
        {
            const char* key = "rival_sidekicks=";
            if (strncmp(line, key, strlen(key)) == 0)
                rivalSidekicks = atoi(line + strlen(key)) != 0 ? 1 : 0;
        }
        fclose(f);
    }
    g_extraRivalSidekicks = rivalSidekicks;
}

extern "C" int PortModExtraRivalSidekicks(void)
{
    return (g_extraRivalSidekicks || g_cupSidekicksThisMatch) ? 1 : 0;
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
    ReadExtras();

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
    const int selected = EffectiveSelected();
    if (selected == PORT_CHALLENGE_ROTATING_CURSE)
        return g_rotatingDraw;
    return selected;
}

extern "C" int PortModChallengeGetSelected(void)
{
    return EffectiveSelected();
}

extern "C" void PortModChallengeReroll(void)
{
    if (EffectiveSelected() == PORT_CHALLENGE_ROTATING_CURSE)
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
        "Refuerzos",
        "Súper Ítems",
    };
    if (challenge <= PORT_CHALLENGE_NONE || challenge >= PORT_CHALLENGE_COUNT)
        return "";
    return kNames[challenge];
}

extern "C" const char* PortModChallengeDescription(int challenge)
{
    // UTF-8, in ID order: what the challenge does, in a few words, for the on-screen tag.
    static const char* const kDescriptions[PORT_CHALLENGE_COUNT] = {
        "",
        "Tu equipo solo recibe champiñones",
        "El stick va al revés",
        "Tu equipo no recibe ítems",
        "Siempre tienes un ítem listo",
        "Tus jugadores corren más lento",
        "Tus jugadores corren más rápido",
        "No puedes hacer pases elevados",
        "No puedes barrer",
        "No puedes empujar con el hombro",
        "La CPU corre más rápido",
        "La CPU recibe Chomps y Estrellas más seguido",
        "La cerca eléctrica te aturde el doble",
        "Todos tus tiros salen por arriba",
        "La cerca casi no aturde a la CPU",
        "Los goles de la CPU valen doble",
        "Electrocutado por la cerca = fuera del partido",
        "Solo tu capitán puede tirar al arco",
        "Si la CPU se pone adelante, pierdes",
        "Bowser viene más seguido y cae sobre ti",
        "Más de 2 s con el balón y explota",
        "Los golpes de la CPU te electrocutan",
        "La maldición cambia con cada gol",
        "La CPU tiene estrella 1 s cada 7 s",
        "Un esquive exitoso da estrella 1.5 s",
        "Tu arquero se marea con cada tiro",
        "Cada 10 faltas tuyas, un Chomp va por ti",
        "Gol justo después de una falta tuya: anulado",
        "El arquero rival te devuelve los tiros",
        "Jugadas con estilo llenan la barra; L+R = estrella",
        "Empiezas solo con el capitán; el resto llega después",
        "La CPU recibe más ítems y más fuertes",
    };
    if (challenge <= PORT_CHALLENGE_NONE || challenge >= PORT_CHALLENGE_COUNT)
        return "";
    return kDescriptions[challenge];
}

static unsigned int g_matchSerial = 0;

extern "C" unsigned int PortModChallengeMatchSerial(void)
{
    return g_matchSerial;
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
    g_cupCurse = PORT_CHALLENGE_NONE;
    g_cupSidekicksThisMatch = false;
    g_activeChallenge = PORT_CHALLENGE_NONE;
    g_rotatingDraw = PORT_CHALLENGE_NONE;
    g_lastContent = "0";
    if (g_havePath)
        WriteFile("0");
}

// ---- Challenge Super Bowser Cup ----
//
// mods/challenge_cup.txt remembers, across sessions, whether the Super Bowser Cup in progress is the
// challenge kind, which curse the current round drew (so quitting and replaying a match can't
// re-roll it), the curse lined up for the next match, the last curse played, how often each curse
// has come up this cup, and the cup's "Compañeros únicos rival" switch:
//   active=0/1, round=<key>, curse=<id>, pending=<id>, last=<id>, sidekicks=0/1, used<id>=<count>
// The companion app reads it too, and may rewrite pending= between matches to spend a roulette
// skip, so the game re-reads it before every use rather than keeping its own copy.

namespace
{

// The curses this cup draws from: every challenge except 1, 2, 8, 11, 12, 14 and 19 (the companion
// app's skip keeps its own copy of this list).
const int kCupPool[] = { 3, 4, 5, 6, 7, 9, 10, 13, 15, 16, 17, 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
constexpr int kCupPoolSize = (int)(sizeof kCupPool / sizeof kCupPool[0]);

struct CupState
{
    int active = 0;
    int round = -1000000;
    int curse = PORT_CHALLENGE_NONE;
    int pending = PORT_CHALLENGE_NONE;
    int last = PORT_CHALLENGE_NONE;
    int sidekicks = 0;
    int used[PORT_CHALLENGE_COUNT] = {};
};

CupState g_cup;

bool InCupPool(int id)
{
    for (int i = 0; i < kCupPoolSize; i++)
        if (kCupPool[i] == id)
            return true;
    return false;
}

std::string CupPath()
{
    return g_modsDir + "/challenge_cup.txt";
}

void LoadCup()
{
    if (!g_havePath)
        return;
    FILE* f = fopen(CupPath().c_str(), "rb");
    if (f == nullptr)
        return; // keep what is in memory
    g_cup = CupState();
    char line[64];
    while (fgets(line, sizeof line, f) != nullptr)
    {
        int id, value;
        if (sscanf(line, "used%d=%d", &id, &value) == 2)
        {
            if (id > PORT_CHALLENGE_NONE && id < PORT_CHALLENGE_COUNT && value >= 0)
                g_cup.used[id] = value;
        }
        else if (sscanf(line, "active=%d", &value) == 1)
            g_cup.active = value != 0 ? 1 : 0;
        else if (sscanf(line, "round=%d", &value) == 1)
            g_cup.round = value;
        else if (sscanf(line, "curse=%d", &value) == 1)
            g_cup.curse = value;
        else if (sscanf(line, "pending=%d", &value) == 1)
            g_cup.pending = value;
        else if (sscanf(line, "last=%d", &value) == 1)
            g_cup.last = value;
        else if (sscanf(line, "sidekicks=%d", &value) == 1)
            g_cup.sidekicks = value != 0 ? 1 : 0;
    }
    fclose(f);
}

void SaveCup()
{
    if (!g_havePath)
        return;
    FILE* f = fopen(CupPath().c_str(), "wb");
    if (f == nullptr)
        return;
    fprintf(f, "active=%d\nround=%d\ncurse=%d\npending=%d\nlast=%d\nsidekicks=%d\n", g_cup.active, g_cup.round,
            g_cup.curse, g_cup.pending, g_cup.last, g_cup.sidekicks);
    for (int id = PORT_CHALLENGE_NONE + 1; id < PORT_CHALLENGE_COUNT; id++)
        if (g_cup.used[id] > 0)
            fprintf(f, "used%d=%d\n", id, g_cup.used[id]);
    fclose(f);
}

// Never `exclude` (the curse the next match would follow); otherwise every curse can come up, but
// each time one does its chance halves for the rest of the cup.
int DrawCupCurse(int exclude)
{
    double weights[kCupPoolSize];
    double total = 0.0;
    for (int i = 0; i < kCupPoolSize; i++)
    {
        const int id = kCupPool[i];
        const int uses = g_cup.used[id] < 30 ? g_cup.used[id] : 30;
        weights[i] = id == exclude ? 0.0 : 1.0 / (double)(1u << uses);
        total += weights[i];
    }
    double roll = (double)(NextRandom() & 0xFFFFFF) / (double)0x1000000 * total;
    for (int i = 0; i < kCupPoolSize; i++)
    {
        if (weights[i] <= 0.0)
            continue;
        if (roll < weights[i])
            return kCupPool[i];
        roll -= weights[i];
    }
    for (int i = kCupPoolSize - 1; i >= 0; i--)
        if (weights[i] > 0.0)
            return kCupPool[i];
    return kCupPool[0];
}

} // namespace

extern "C" int PortChallengeCupIsActive(void)
{
    LoadCup();
    return g_cup.active;
}

extern "C" void PortChallengeCupStart(int challenge)
{
    LoadCup();
    const int sidekicks = g_cup.sidekicks;
    g_cup = CupState();
    g_cup.sidekicks = sidekicks;
    g_cup.active = challenge != 0 ? 1 : 0;
    // The first match's curse is drawn up front, so the companion app can show it (and skip it).
    if (g_cup.active)
        g_cup.pending = DrawCupCurse(PORT_CHALLENGE_NONE);
    SaveCup();
}

extern "C" void PortChallengeCupEnd(void)
{
    LoadCup();
    if (!g_cup.active)
        return;
    g_cup.active = 0;
    g_cup.pending = PORT_CHALLENGE_NONE;
    SaveCup();
}

extern "C" void PortChallengeCupBeginMatch(int inSuperBowserCup, int roundKey)
{
    LoadCup();
    g_matchSerial++;
    g_cupCurse = PORT_CHALLENGE_NONE;
    g_cupSidekicksThisMatch = false;
    if (inSuperBowserCup && g_cup.active)
    {
        if (roundKey != g_cup.round || !InCupPool(g_cup.curse))
        {
            g_cup.curse = InCupPool(g_cup.pending) && g_cup.pending != g_cup.last ? g_cup.pending
                                                                                  : DrawCupCurse(g_cup.last);
            g_cup.round = roundKey;
            g_cup.last = g_cup.curse;
            g_cup.used[g_cup.curse]++;
            // Line up the next match's curse now, so it can be seen (and skipped) between matches.
            g_cup.pending = DrawCupCurse(g_cup.curse);
            SaveCup();
        }
        g_cupCurse = g_cup.curse;
        g_cupSidekicksThisMatch = g_cup.sidekicks != 0;
    }
    // The rotating draw follows whichever selection now applies.
    if (EffectiveSelected() == PORT_CHALLENGE_ROTATING_CURSE)
    {
        if (g_rotatingDraw == PORT_CHALLENGE_NONE)
            DrawRotating();
    }
    else
        g_rotatingDraw = PORT_CHALLENGE_NONE;
}

extern "C" int PortChallengeCupCurse(void)
{
    return g_cupCurse;
}

extern "C" int PortChallengeCupSidekicks(void)
{
    LoadCup();
    return g_cup.sidekicks;
}

extern "C" void PortChallengeCupSetSidekicks(int on)
{
    LoadCup();
    g_cup.sidekicks = on != 0 ? 1 : 0;
    SaveCup();
}
