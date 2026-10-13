// See include/port/achievements.h.

#include "port/achievements.h"

#include <ctime>

namespace
{

enum eAchievement
{
    ACH_FAST_GOAL,      // score within the first 25s of game time
    ACH_CLEAN_SHEET,    // concede no goals
    ACH_TACKLES,        // 15 tackles on the opponent
    ACH_SHOCK_CAPTAIN,  // electrocute the opposing captain 3 times
    ACH_CHIP_GOAL,      // score with a chip shot
    ACH_SUPER_GOAL,     // score a Super Strike
    ACH_INTERRUPT,      // tackle an opponent who is winding up a shot
    ACH_ROUT,           // win by 3 or more
    ACH_CAPTAIN_BRACE,  // our captain scores 2
    ACH_COMEBACK,       // win after having been behind
    ACH_COUNT
};

struct Definition
{
    const char* name;
    const char* description;
    int target;
};

const Definition kDefinitions[ACH_COUNT] = {
    { "Gol Relámpago", "Marca un gol antes de los 25 segundos", 1 },
    { "Muralla", "Termina el partido sin recibir goles", 1 },
    { "Tanque", "Haz 15 tackles al rival", 15 },
    { "Electricista", "Electrocuta 3 veces al capitán rival", 3 },
    { "Golazo de Globito", "Marca un gol de globito", 1 },
    { "Súper Gol", "Marca un gol con Súper Tiro", 1 },
    { "Cortacorrientes", "Interrumpe un tiro cargado del rival", 1 },
    { "Goleada", "Gana por 3 goles o más", 1 },
    { "Capitán Goleador", "Tu capitán marca 2 goles", 2 },
    { "Remontada", "Gana después de ir perdiendo", 1 },
};

const float kFastGoalSeconds = 25.0f;

struct Slot
{
    int id;
    int progress;
    int state;
};

Slot g_slots[2];
int g_count = 0;
bool g_wasBehind = false;
unsigned int g_pauseFrames = 0;

unsigned int g_rng = 0;

unsigned int NextRandom()
{
    if (g_rng == 0)
        g_rng = (unsigned int)time(nullptr) * 2654435761u | 1u;
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 8;
}

// Adds to progress on an active achievement and completes it at its target. No-op if this match
// didn't draw it or it is already settled.
void Advance(int id, int amount)
{
    for (int i = 0; i < g_count; i++)
    {
        Slot& slot = g_slots[i];
        if (slot.id != id || slot.state != PORT_ACH_PENDING)
            continue;
        slot.progress += amount;
        if (slot.progress >= kDefinitions[id].target)
        {
            slot.progress = kDefinitions[id].target;
            slot.state = PORT_ACH_DONE;
        }
    }
}

void Settle(int id, bool bDone)
{
    for (int i = 0; i < g_count; i++)
    {
        Slot& slot = g_slots[i];
        if (slot.id != id || slot.state != PORT_ACH_PENDING)
            continue;
        slot.state = bDone ? PORT_ACH_DONE : PORT_ACH_FAILED;
        if (bDone)
            slot.progress = kDefinitions[id].target;
    }
}

} // namespace

extern "C" void PortAchievementsBeginMatch(void)
{
    int first = (int)(NextRandom() % ACH_COUNT);
    int second = (int)(NextRandom() % (ACH_COUNT - 1));
    if (second >= first)
        second++;

    g_slots[0] = { first, 0, PORT_ACH_PENDING };
    g_slots[1] = { second, 0, PORT_ACH_PENDING };
    g_count = 2;
    g_wasBehind = false;
}

extern "C" void PortAchievementsClear(void)
{
    g_count = 0;
}

extern "C" void PortAchievementsGoal(int forUs, int scoreFor, int scoreAgainst, float gameSeconds, int isChip,
                                     int isSuper, int byCaptain)
{
    if (scoreFor < scoreAgainst)
        g_wasBehind = true;

    if (!forUs)
    {
        Settle(ACH_CLEAN_SHEET, false);
        return;
    }

    if (gameSeconds < kFastGoalSeconds)
        Advance(ACH_FAST_GOAL, 1);
    if (isChip)
        Advance(ACH_CHIP_GOAL, 1);
    if (isSuper)
        Advance(ACH_SUPER_GOAL, 1);
    if (byCaptain)
        Advance(ACH_CAPTAIN_BRACE, 1);
}

extern "C" void PortAchievementsTackle(int interruptedCharge)
{
    Advance(ACH_TACKLES, 1);
    if (interruptedCharge)
        Advance(ACH_INTERRUPT, 1);
}

extern "C" void PortAchievementsOpponentCaptainShocked(void)
{
    Advance(ACH_SHOCK_CAPTAIN, 1);
}

extern "C" void PortAchievementsMatchEnd(int scoreFor, int scoreAgainst)
{
    const bool bWon = scoreFor > scoreAgainst;
    Settle(ACH_CLEAN_SHEET, scoreAgainst == 0);
    Settle(ACH_ROUT, scoreFor - scoreAgainst >= 3);
    Settle(ACH_COMEBACK, bWon && g_wasBehind);

    // Everything still open at the whistle was missed.
    for (int i = 0; i < g_count; i++)
    {
        if (g_slots[i].state == PORT_ACH_PENDING)
            g_slots[i].state = PORT_ACH_FAILED;
    }
}

extern "C" int PortAchievementsCount(void)
{
    return g_count;
}

extern "C" void PortAchievementsGet(int index, PortAchievementView* out)
{
    if (index < 0 || index >= g_count)
    {
        *out = PortAchievementView{ "", "", 0, 1, PORT_ACH_PENDING };
        return;
    }
    const Slot& slot = g_slots[index];
    const Definition& def = kDefinitions[slot.id];
    out->name = def.name;
    out->description = def.description;
    out->progress = slot.progress;
    out->target = def.target;
    out->state = slot.state;
}

extern "C" void PortAchievementsNotePauseFrame(void)
{
    g_pauseFrames++;
}

extern "C" unsigned int PortAchievementsPauseFrameCount(void)
{
    return g_pauseFrames;
}
