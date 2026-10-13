// PORT: challenge roulette - see Game/AchievementHooks.h.
#include "Game/AchievementHooks.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ShotMeter.h"
#include "Game/Ball.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "port/achievements.h"
#include "port/mod_challenge.h"

namespace
{

// The last shot taken, so a goal can be matched to it.
cFielder* s_pLastShooter = NULL;
bool s_bLastShotWasChip = false;

// "Triple Impacto": the distinct fielders each fielder's current powerup use has hit, indexed by
// g_pTeams side and fielder slot.
struct UseHits
{
    cFielder* pVictims[3];
    int nVictims;
};
UseHits s_useHits[2][4];

bool Locate(cPlayer* pPlayer, int& side, int& index)
{
    for (side = 0; side < 2; side++)
    {
        if (g_pTeams[side] == NULL || pPlayer->m_pTeam != g_pTeams[side])
            continue;
        for (index = 0; index < 4; index++)
        {
            if (g_pTeams[side]->GetFielder(index) == pPlayer)
                return true;
        }
    }
    return false;
}

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

// Our side: the team with a human on it, side 0 if both have one (local versus), -1 if neither.
int OurSide()
{
    for (int side = 0; side < 2; side++)
    {
        if (TeamHasHuman(g_pTeams[side]))
            return side;
    }
    return -1;
}

} // namespace

// "Slow-No": an opponent's perfect pass (the slow-motion one, flagged by mbHyperSTS) is in the air
// and pVictim - one of theirs - is the one it is meant for.
static bool IsOpponentsPerfectPassReceiver(cFielder* pVictim)
{
    const int ourSide = OurSide();
    return ourSide >= 0 && g_pBall != NULL && g_pBall->mbHyperSTS && g_pBall->m_pPassTarget == pVictim
        && pVictim->m_pTeam != g_pTeams[ourSide];
}

void PortAchievementHooksBeginMatch()
{
    s_pLastShooter = NULL;
    s_bLastShotWasChip = false;
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < 4; i++)
            s_useHits[side][i].nVictims = 0;
    if (OurSide() >= 0)
        PortAchievementsBeginMatch(PortModChallengeGetActive());
    else
        PortAchievementsClear();
}

void PortAchievementHooksNoteShot(cFielder* pShooter, bool bIsChipShot)
{
    s_pLastShooter = pShooter;
    s_bLastShotWasChip = bIsChipShot;
}

void PortAchievementHooksGoal(int nScoringSide, cPlayer* pScorer, bool bSuperStrike, float fGameSeconds)
{
    const int ourSide = OurSide();
    if (ourSide < 0 || g_pTeams[0] == NULL || g_pTeams[1] == NULL)
        return;

    cTeam* pOurs = g_pTeams[ourSide];
    const bool bForUs = nScoringSide == ourSide;
    const bool bChip = bForUs && pScorer != NULL && pScorer == s_pLastShooter && s_bLastShotWasChip;
    const bool bByCaptain = bForUs && pScorer != NULL && pScorer == pOurs->GetCaptain();
    PortAchievementsGoal(bForUs ? 1 : 0, pOurs->m_nScore, pOurs->GetOtherTeam()->m_nScore, fGameSeconds,
                         bChip ? 1 : 0, (bForUs && bSuperStrike) ? 1 : 0, bByCaptain ? 1 : 0);
}

void PortAchievementHooksTackle(cFielder* pVictim, cPlayer* pAttacker)
{
    const int ourSide = OurSide();
    if (ourSide < 0 || pAttacker == NULL || pAttacker->m_eClassType != FIELDER)
        return;
    if (!((cFielder*)pAttacker)->IsAttacking())
        return;
    if (pAttacker->m_pTeam != g_pTeams[ourSide] || pVictim->m_pTeam == pAttacker->m_pTeam)
        return;

    // Winding up a shot: the charge desire, a live shot meter, or mid Super Strike.
    const bool bCharging = pVictim->m_eFielderDesireState == FIELDERDESIRE_WINDUP_SHOT
                        || ShotMeter::IsActive(pVictim->m_pShotMeter->m_eShotMeterState)
                        || pVictim->m_eActionState == ACTION_SHOOT_TO_SCORE;
    PortAchievementsTackle(bCharging ? 1 : 0);
    if (IsOpponentsPerfectPassReceiver(pVictim))
        PortAchievementsSlowNo();
}

void PortAchievementHooksElectrocuted(cFielder* pFielder)
{
    const int ourSide = OurSide();
    if (ourSide < 0 || pFielder->m_pTeam == g_pTeams[ourSide])
        return;
    if (pFielder == pFielder->m_pTeam->GetCaptain())
        PortAchievementsOpponentCaptainShocked();
}

static bool IsOurs(cPlayer* pPlayer)
{
    const int ourSide = OurSide();
    return ourSide >= 0 && pPlayer != NULL && pPlayer->m_pTeam == g_pTeams[ourSide];
}

void PortAchievementHooksPowerupUsed(cFielder* pUser)
{
    int side, index;
    if (pUser != NULL && Locate(pUser, side, index))
        s_useHits[side][index].nVictims = 0;
}

void PortAchievementHooksPowerupHit(cPlayer* pUser, cFielder* pVictim)
{
    int side, index;
    if (pVictim == NULL || pVictim == pUser || !IsOurs(pUser) || !Locate(pUser, side, index))
        return;

    if (IsOpponentsPerfectPassReceiver(pVictim))
        PortAchievementsSlowNo();

    UseHits& use = s_useHits[side][index];
    if (use.nVictims >= 3)
        return; // already counted
    for (int i = 0; i < use.nVictims; i++)
    {
        if (use.pVictims[i] == pVictim)
            return;
    }
    use.pVictims[use.nVictims++] = pVictim;
    if (use.nVictims == 3)
        PortAchievementsItemTripleHit();
}

void PortAchievementHooksTick(float fGameSeconds)
{
    if (OurSide() >= 0)
        PortAchievementsTick(fGameSeconds);
}

void PortAchievementHooksBallTaken(cPlayer* pTaker)
{
    const int ourSide = OurSide();
    if (ourSide < 0 || pTaker == NULL || pTaker->m_pTeam != g_pTeams[ourSide] || g_pBall == NULL)
        return;
    // Taking the ball while an opponent's perfect pass is still headed for one of theirs.
    cPlayer* pTarget = g_pBall->m_pPassTarget;
    if (g_pBall->mbHyperSTS && pTarget != NULL && pTarget->m_pTeam != g_pTeams[ourSide])
        PortAchievementsSlowNo();
}

void PortAchievementHooksDekeBoost(cFielder* pFielder)
{
    if (IsOurs(pFielder))
        PortAchievementsDekeBoost();
}

void PortAchievementHooksMatchEnd()
{
    const int ourSide = OurSide();
    if (ourSide < 0 || g_pTeams[0] == NULL || g_pTeams[1] == NULL)
        return;
    cTeam* pOurs = g_pTeams[ourSide];
    PortAchievementsMatchEnd(pOurs->m_nScore, pOurs->GetOtherTeam()->m_nScore);
}
