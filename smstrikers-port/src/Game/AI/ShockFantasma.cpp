// PORT: challenge roulette - "Shock Fantasma" (PORT_CHALLENGE_VANISH_ON_ELECTROCUTION). See
// Game/AI/ShockFantasma.h for the overall approach.
#include "Game/AI/ShockFantasma.h"
#include "Game/AI/Fielder.h"
#include "Game/Ball.h"
#include "Game/Field.h"
#include "Game/Team.h"
#include "NL/nlMath.h"
#include "port/mod_challenge.h"

#include <cstdio>
#include <cstdlib>

namespace
{

// Indexed by g_pTeams slot and cTeam::GetFielder() index rather than by pointer, so a match that is
// quit before GS_END_GAME can't leave a dangling pointer that a later match's fielder reuses.
bool sEliminated[2][4];
bool sPending[2][4];

// PORT DEBUG: set STRIKERS_DEBUG_CHALLENGE=1 (same switch as the other challenge hooks) to trace
// each step, e.g. to tell "the hook never ran" from "it ran but declined".
bool DebugEnabled()
{
    return getenv("STRIKERS_DEBUG_CHALLENGE") != NULL;
}

bool Locate(const cCharacter* pCharacter, int& side, int& index)
{
    if (pCharacter == NULL || pCharacter->m_eClassType != FIELDER)
        return false;

    const cFielder* pPlayer = static_cast<const cFielder*>(pCharacter);

    for (side = 0; side < 2; side++)
    {
        cTeam* pTeam = g_pTeams[side];
        if (pTeam == NULL || pPlayer->m_pTeam != pTeam)
            continue;
        for (index = 0; index < 4; index++)
        {
            if (pTeam->GetFielder(index) == pPlayer)
                return true;
        }
    }
    return false;
}

// Real local players on the team: every player slot (goalie included, it can hold a pad briefly)
// currently bound to a cGlobalPad. CPU slots never have one.
int CountHumans(int side)
{
    cTeam* pTeam = g_pTeams[side];

    int nHumans = 0;
    for (int i = 0; i < 5; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        if (pPlayer != NULL && pPlayer->GetGlobalPad() != NULL)
            nHumans++;
    }
    return nHumans;
}

// Fielders a team must keep. Never fewer than 2, so a pass (including the automatic kickoff pass)
// always has a valid teammate to go to - the passing code has no "no target" path. Never fewer than
// the number of real players on the team either, so every human pad still has a fielder to drive
// instead of falling through to the goalie.
bool CanEliminate(int side)
{
    int nHumans = CountHumans(side);

    int nActive = 0;
    for (int i = 0; i < 4; i++)
    {
        if (!sEliminated[side][i])
            nActive++;
    }

    int nMinActive = (nHumans > 2) ? nHumans : 2;
    return nActive - 1 >= nMinActive;
}

// Moves the pad (if any) from an eliminated fielder to the active, uncontrolled teammate closest to
// the ball - the same pick the game's own manual-switch button makes.
void HandOffPad(int side, cFielder* pFielder)
{
    if (pFielder->m_pController == NULL)
        return;

    cTeam* pTeam = g_pTeams[side];
    cFielder* pBest = NULL;
    float fBestDistSq = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        cFielder* pCandidate = pTeam->GetFielder(i);
        if (pCandidate == pFielder || sEliminated[side][i] || pCandidate->m_pController != NULL)
            continue;

        float dx = pCandidate->m_v3Position.x - g_pBall->m_v3Position.x;
        float dy = pCandidate->m_v3Position.y - g_pBall->m_v3Position.y;
        float fDistSq = dx * dx + dy * dy;
        if (pBest == NULL || fDistSq < fBestDistSq)
        {
            pBest = pCandidate;
            fBestDistSq = fDistSq;
        }
    }

    if (pBest != NULL)
    {
        pBest->SetAIPad(pFielder->m_pController);
        pFielder->SetAIPad(NULL);
    }
}

} // namespace

bool PortShockFantasmaIsEliminated(const cCharacter* pCharacter)
{
    int side, index;
    return Locate(pCharacter, side, index) && sEliminated[side][index];
}

void PortShockFantasmaOnElectrocuted(cFielder* pFielder)
{
    int side, index;
    if (!Locate(pFielder, side, index))
        return;

    // Any fielder on a team with a real local player, not just the one holding the pad: the fence
    // only shocks a fielder knocked into it by a hit or a bomb, and by then the player has often
    // switched away (or it was an AI teammate all along), so a pad-holder-only rule almost never
    // fired. The challenge is "your team plays a man down"; CPU-only teams are unaffected.
    sPending[side][index] = CountHumans(side) > 0
                         && PortModChallengeGetActive() == PORT_CHALLENGE_VANISH_ON_ELECTROCUTION;

    if (DebugEnabled())
    {
        fprintf(stderr, "[shock fantasma] shocked side=%d fielder=%d humans=%d activeChallenge=%d pending=%d\n",
                side, index, CountHumans(side), PortModChallengeGetActive(), sPending[side][index] ? 1 : 0);
    }
}

void PortShockFantasmaOnStunEnd(cFielder* pFielder)
{
    int side, index;
    if (!Locate(pFielder, side, index) || !sPending[side][index])
        return;

    sPending[side][index] = false;
    bool bActive = PortModChallengeGetActive() == PORT_CHALLENGE_VANISH_ON_ELECTROCUTION;
    bool bCanEliminate = CanEliminate(side);
    if (DebugEnabled())
    {
        fprintf(stderr, "[shock fantasma] stun end side=%d fielder=%d challengeStillActive=%d canEliminate=%d\n",
                side, index, bActive ? 1 : 0, bCanEliminate ? 1 : 0);
    }
    if (!bActive || !bCanEliminate)
        return;

    sEliminated[side][index] = true;
    // Park it right away rather than next frame, so it can't touch anything in between.
    PortShockFantasmaUpdate();
}

void PortShockFantasmaUpdate()
{
    if (g_pBall == NULL)
        return;

    for (int side = 0; side < 2; side++)
    {
        cTeam* pTeam = g_pTeams[side];
        if (pTeam == NULL)
            continue;

        for (int index = 0; index < 4; index++)
        {
            if (!sEliminated[side][index])
                continue;

            cFielder* pFielder = pTeam->GetFielder(index);
            HandOffPad(side, pFielder);
            pFielder->ClearPassTargetIfAmThePassTarget();

            // Drop it from the marking pairs, so an opposing defender doesn't spend the rest of the
            // match shadowing a player who isn't there.
            pFielder->SetMark(NULL);
            if (pFielder->GetMarker() != NULL)
                pFielder->GetMarker()->SetMark(NULL);

            // Re-applied every frame: other actions (loose-ball passes/shots, CleanUpAction) turn
            // these back on as a matter of course.
            pFielder->m_pPhysicsCharacter->m_CanCollideWithBall = false;
            pFielder->m_pPhysicsCharacter->m_CanCollideWithWall = false;

            // Well past the sideline, one spot per fielder and per side so parked fielders never
            // overlap each other. The AI keeps steering it back toward play, so it is re-parked
            // every frame; it never gets close enough to bump into anyone.
            nlVector3 v3Park;
            nlVec3Set(v3Park,
                -6.0f + 4.0f * (float)index,
                (cField::GetSidelineY(1u) + 30.0f) * (side == 0 ? -1.0f : 1.0f),
                cField::GetGroundZ());
            nlVector3 v3Zero;
            nlVec3Set(v3Zero, 0.0f, 0.0f, 0.0f);
            pFielder->SetPosition(v3Park);
            pFielder->SetVelocity(v3Zero);
        }
    }
}

void PortShockFantasmaReset()
{
    for (int side = 0; side < 2; side++)
    {
        for (int index = 0; index < 4; index++)
        {
            if (sEliminated[side][index] && g_pTeams[side] != NULL)
            {
                cFielder* pFielder = g_pTeams[side]->GetFielder(index);
                pFielder->m_pPhysicsCharacter->m_CanCollideWithBall = true;
                pFielder->m_pPhysicsCharacter->m_CanCollideWithWall = true;
            }
            sEliminated[side][index] = false;
            sPending[side][index] = false;
        }
    }
}
