// PORT: challenge roulette - see Game/AchievementHooks.h.
#include "Game/AchievementHooks.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ShotMeter.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "port/achievements.h"

namespace
{

// The last shot taken, so a goal can be matched to it.
cFielder* s_pLastShooter = NULL;
bool s_bLastShotWasChip = false;

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

void PortAchievementHooksBeginMatch()
{
    s_pLastShooter = NULL;
    s_bLastShotWasChip = false;
    if (OurSide() >= 0)
        PortAchievementsBeginMatch();
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
}

void PortAchievementHooksElectrocuted(cFielder* pFielder)
{
    const int ourSide = OurSide();
    if (ourSide < 0 || pFielder->m_pTeam == g_pTeams[ourSide])
        return;
    if (pFielder == pFielder->m_pTeam->GetCaptain())
        PortAchievementsOpponentCaptainShocked();
}

void PortAchievementHooksMatchEnd()
{
    const int ourSide = OurSide();
    if (ourSide < 0 || g_pTeams[0] == NULL || g_pTeams[1] == NULL)
        return;
    cTeam* pOurs = g_pTeams[ourSide];
    PortAchievementsMatchEnd(pOurs->m_nScore, pOurs->GetOtherTeam()->m_nScore);
}
