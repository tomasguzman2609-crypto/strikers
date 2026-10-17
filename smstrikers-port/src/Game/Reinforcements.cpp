// PORT: challenge roulette - "Refuerzos". See Game/Reinforcements.h.
#include "Game/Reinforcements.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ShockFantasma.h"
#include "Game/CharacterTriggers.h"
#include "Game/Field.h"
#include "Game/Game.h"
#include "Game/GameTweaks.h"
#include "Game/Net.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "port/mod_challenge.h"

namespace
{

// Seconds of play after which the 2nd, 3rd and 4th fielder come on.
const float kArrivals[3] = { 10.0f, 25.0f, 45.0f };

bool s_bStarted = false; // the bench has been set up this match
float s_fPlayTime = 0.0f;
int s_nArrived = 0; // how many of kArrivals have passed

bool Active()
{
    return PortModChallengeGetActive() == PORT_CHALLENGE_REINFORCEMENTS;
}

int CountHumans(cTeam* pTeam)
{
    int nHumans = 0;
    for (int i = 0; i < 5; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        if (pPlayer != NULL && pPlayer->GetGlobalPad() != NULL)
            nHumans++;
    }
    return nHumans;
}

int CountOnPitch(int side)
{
    int n = 0;
    for (int i = 0; i < 4; i++)
    {
        if (!PortShockFantasmaIsBenched(side, i) && !PortShockFantasmaIsEliminated(g_pTeams[side]->GetFielder(i)))
            n++;
    }
    return n;
}

bool AnyBenched(int side)
{
    for (int i = 0; i < 4; i++)
    {
        if (PortShockFantasmaIsBenched(side, i))
            return true;
    }
    return false;
}

void PublishMeter()
{
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] != NULL && CountHumans(g_pTeams[side]) > 0)
        {
            PortModChallengeSetMeter(CountOnPitch(side), 4);
            return;
        }
    }
}

// The next benched fielder to come on: one a waiting local player's pad is parked on, if any, so
// that player is back in the game first; else the first one on the bench. -1 if the bench is empty.
int NextOffBench(int side)
{
    int nFirst = -1;
    for (int i = 0; i < 4; i++)
    {
        if (!PortShockFantasmaIsBenched(side, i))
            continue;
        if (g_pTeams[side]->GetFielder(i)->GetGlobalPad() != NULL)
            return i;
        if (nFirst < 0)
            nFirst = i;
    }
    return nFirst;
}

// Brings one fielder off the bench: into its own half, with a Mushroom to get it into the game.
// bMushroom false for a kickoff, which places everyone itself.
void BringOnNext(int side, bool bMushroom)
{
    cTeam* pTeam = g_pTeams[side];
    const int i = NextOffBench(side);
    if (i >= 0)
    {
        PortShockFantasmaSetBenched(side, i, false);

        cFielder* pFielder = pTeam->GetFielder(i);
        if (bMushroom)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, 0.5f * pTeam->m_pNet->m_v3NetLocation.x, -6.0f + 4.0f * (float)i, cField::GetGroundZ());
            nlVector3 v3Zero;
            nlVec3Set(v3Zero, 0.0f, 0.0f, 0.0f);
            pFielder->SetPosition(v3Pos);
            pFielder->SetVelocity(v3Zero);

            if (!pFielder->IsPlayingPowerupAnim() && pFielder->m_tPowerupEffectTime.m_uPackedTime == 0)
            {
                pFielder->m_ePowerup = POWER_UP_MUSHROOM;
                pFielder->mnNumPowerups = 1;
                pFielder->m_tPowerupEffectTime.SetSeconds(g_pGame->m_pGameTweaks->fMushroomEffectTime);
                EmitMushroom(pFielder);
                pFielder->InitBlur(0);
            }
        }
    }
}

void EmptyBenches()
{
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] == NULL)
            continue;
        while (AnyBenched(side))
            BringOnNext(side, false);
    }
}

// Benches every fielder of a human team but its captain, however many local players the team has:
// the first pad moves onto the captain, any others stay parked with their benched fielder (and do
// nothing, see PortReinforcementsPadOnBench) until it comes on.
void SetUpBenches()
{
    for (int side = 0; side < 2; side++)
    {
        cTeam* pTeam = g_pTeams[side];
        if (pTeam == NULL || CountHumans(pTeam) == 0)
            continue;
        for (int i = 0; i < 4; i++)
        {
            if (pTeam->GetFielder(i) != pTeam->GetCaptain())
                PortShockFantasmaSetBenched(side, i, true);
        }
    }
}

} // namespace

void PortReinforcementsUpdate(float fDeltaT, bool bInPlay)
{
    if (!Active())
    {
        // The challenge changed mid-match (Maldición Rotativa, a late spin): everyone back on.
        if (s_bStarted)
        {
            EmptyBenches();
            s_bStarted = false;
        }
        return;
    }
    if (!bInPlay)
        return;

    if (!s_bStarted)
    {
        s_bStarted = true;
        s_fPlayTime = 0.0f;
        s_nArrived = 0;
        SetUpBenches();
    }

    s_fPlayTime += fDeltaT;
    while (s_nArrived < 3 && s_fPlayTime >= kArrivals[s_nArrived])
    {
        s_nArrived++;
        for (int side = 0; side < 2; side++)
        {
            if (g_pTeams[side] != NULL && AnyBenched(side))
                BringOnNext(side, true);
        }
    }
    PublishMeter();
}

void PortReinforcementsBeforeKickoff()
{
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] != NULL && AnyBenched(side) && CountOnPitch(side) < 2)
            BringOnNext(side, false);
    }
}

bool PortReinforcementsBlocksPass(cFielder* pFielder)
{
    if (!Active() || pFielder == NULL || pFielder->m_pTeam == NULL)
        return false;
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] == pFielder->m_pTeam)
            return AnyBenched(side) && CountOnPitch(side) < 2;
    }
    return false;
}

bool PortReinforcementsPadOnBench(cFielder* pFielder)
{
    if (!Active() || pFielder == NULL)
        return false;
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] == NULL || g_pTeams[side] != pFielder->m_pTeam)
            continue;
        for (int i = 0; i < 4; i++)
        {
            if (g_pTeams[side]->GetFielder(i) == pFielder)
                return PortShockFantasmaIsBenched(side, i);
        }
    }
    return false;
}

void PortReinforcementsReset()
{
    s_bStarted = false;
    s_fPlayTime = 0.0f;
    s_nArrived = 0;
}
