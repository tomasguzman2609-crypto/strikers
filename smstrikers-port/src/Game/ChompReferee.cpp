// PORT: challenge roulette - "Chomp Árbitro". See Game/ChompReferee.h.
#include "Game/ChompReferee.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ShockFantasma.h"
#include "Game/Ball.h"
#include "Game/BasicStadium.h"
#include "Game/Game.h"
#include "Game/Player.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/ChainChomp.h"
#include "Game/Render/NPCManager.h"
#include "Game/Team.h"
#include "port/mod_challenge.h"

namespace
{

const int kFoulsPerChomp = 10;

// Indexed by g_pTeams side; a side's count stays at kFoulsPerChomp while its Chomp waits to be let out.
int s_nFouls[2];

bool TeamHasHuman(cTeam* pTeam)
{
    if (pTeam == NULL)
        return false;
    for (int i = 0; i < 5; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        if (pPlayer != NULL && pPlayer->GetGlobalPad() != NULL)
            return true;
    }
    return false;
}

int SideOf(cTeam* pTeam)
{
    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] != NULL && g_pTeams[side] == pTeam)
            return side;
    }
    return -1;
}

// The side shown on screen: the one with a human, side 0 if both have one.
void PublishMeter()
{
    for (int side = 0; side < 2; side++)
    {
        if (TeamHasHuman(g_pTeams[side]))
        {
            PortModChallengeSetMeter(s_nFouls[side], kFoulsPerChomp);
            return;
        }
    }
    PortModChallengeSetMeter(0, 0);
}

// Who the Chomp goes for: the team's ball carrier if it has the ball, else whoever of the team is
// closest to the ball and still on their feet (and on the pitch).
cFielder* PickTarget(cTeam* pTeam)
{
    cFielder* pCarrier = g_pBall->GetOwnerFielder();
    if (pCarrier != NULL && pCarrier->m_pTeam == pTeam)
        return pCarrier;

    cFielder* pBest = NULL;
    float fBestDistSq = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        cFielder* pCandidate = pTeam->GetFielder(i);
        if (pCandidate == NULL || pCandidate->IsFallenDown(0.0f) || PortShockFantasmaIsEliminated(pCandidate))
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
    return pBest;
}

} // namespace

void PortChompRefereeTackle(cFielder* pVictim, cPlayer* pAttacker)
{
    if (PortModChallengeGetActive() != PORT_CHALLENGE_CHOMP_REFEREE)
        return;
    if (pAttacker == NULL || pAttacker->m_eClassType != FIELDER || !((cFielder*)pAttacker)->IsAttacking())
        return;
    if (pVictim->m_pTeam == pAttacker->m_pTeam || !TeamHasHuman(pAttacker->m_pTeam))
        return;

    const int side = SideOf(pAttacker->m_pTeam);
    if (side < 0 || s_nFouls[side] >= kFoulsPerChomp)
        return; // a Chomp is already on its way
    s_nFouls[side]++;
    PublishMeter();
}

void PortChompRefereeUpdate(bool bInPlay)
{
    if (PortModChallengeGetActive() != PORT_CHALLENGE_CHOMP_REFEREE)
    {
        PortChompRefereeReset();
        return;
    }
    PublishMeter();

    if (!bInPlay || g_pBall == NULL || g_pGame->mbCaptainShotToScoreOn)
        return;

    BasicStadium* pStadium = BasicStadium::GetCurrentStadium();
    if (pStadium == NULL || pStadium->mpNPCManager == NULL)
        return;
    ChainChomp* pChomp = pStadium->mpNPCManager->mpChainChomp;
    Bowser* pBowser = pStadium->mpNPCManager->mpBowser;
    if (pChomp == NULL || !pChomp->IsHidden() || (pBowser != NULL && pBowser->meBowserState != BOWSER_STATE_HIDDEN))
        return;

    for (int side = 0; side < 2; side++)
    {
        cTeam* pTeam = g_pTeams[side];
        if (s_nFouls[side] < kFoulsPerChomp || pTeam == NULL || pTeam->GetOtherTeam() == NULL)
            continue;
        cFielder* pTarget = PickTarget(pTeam);
        if (pTarget == NULL)
            continue;
        // "Thrown" by the opposing captain, so the hits count as theirs.
        pChomp->Fall(pTeam->GetOtherTeam()->GetCaptain(), pTarget);
        s_nFouls[side] = 0;
        PublishMeter();
        return; // one Chomp at a time
    }
}

void PortChompRefereeReset()
{
    s_nFouls[0] = s_nFouls[1] = 0;
    PortModChallengeSetMeter(0, 0);
}
