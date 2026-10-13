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

// Distinct fielders each live item has hit, for "Triple Impacto". Items come from a small pool and
// are forgotten in PortAchievementHooksItemGone, so a few slots are plenty.
struct ItemHits
{
    const void* pItem;
    cFielder* pVictims[3];
    int nVictims;
};
ItemHits s_itemHits[8];

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
    for (int i = 0; i < 8; i++)
        s_itemHits[i].pItem = NULL;
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

static bool IsOurs(cPlayer* pPlayer)
{
    const int ourSide = OurSide();
    return ourSide >= 0 && pPlayer != NULL && pPlayer->m_pTeam == g_pTeams[ourSide];
}

void PortAchievementHooksItemHit(const void* pItem, cPlayer* pThrower, cFielder* pVictim)
{
    if (pItem == NULL || pVictim == NULL || !IsOurs(pThrower))
        return;

    ItemHits* pEntry = NULL;
    ItemHits* pFree = NULL;
    for (int i = 0; i < 8; i++)
    {
        if (s_itemHits[i].pItem == pItem)
            pEntry = &s_itemHits[i];
        else if (s_itemHits[i].pItem == NULL && pFree == NULL)
            pFree = &s_itemHits[i];
    }
    if (pEntry == NULL)
    {
        if (pFree == NULL)
            return;
        pEntry = pFree;
        pEntry->pItem = pItem;
        pEntry->nVictims = 0;
    }

    for (int i = 0; i < pEntry->nVictims; i++)
    {
        if (pEntry->pVictims[i] == pVictim)
            return;
    }
    if (pEntry->nVictims < 3)
        pEntry->pVictims[pEntry->nVictims++] = pVictim;
    if (pEntry->nVictims == 3)
        PortAchievementsItemTripleHit();
}

void PortAchievementHooksItemGone(const void* pItem)
{
    for (int i = 0; i < 8; i++)
    {
        if (s_itemHits[i].pItem == pItem)
            s_itemHits[i].pItem = NULL;
    }
}

void PortAchievementHooksBlastHits(cPlayer* pThrower, int nHits)
{
    if (nHits >= 3 && IsOurs(pThrower))
        PortAchievementsItemTripleHit();
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
