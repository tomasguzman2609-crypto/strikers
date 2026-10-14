// PORT: challenge roulette - "Jogo Bonito". See Game/JogoBonito.h.
#include "Game/JogoBonito.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/ShockFantasma.h"
#include "Game/Ball.h"
#include "Game/CharacterTriggers.h"
#include "Game/Game.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "NL/globalpad.h"
#include "port/mod_challenge.h"

namespace
{

const int kMeterTarget = 5;
const float kStarSeconds = 1.3f;
// A shell "grazes" a fielder when it comes this close (2D) without hitting, and the graze counts once
// it is this far away again with the fielder still on their feet.
const float kGrazeNear = 1.3f;
const float kGrazeFar = 2.5f;

int s_nMeter = 0;
bool s_bTriggersWereDown = false;

// Shells currently grazing one of our pad-controlled fielders.
struct Graze
{
    PowerupBase* pShell;
    cFielder* pFielder;
};
Graze s_grazes[8];

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

// The team with a human on it, the first if both have one; NULL if neither.
cTeam* OurTeam()
{
    for (int side = 0; side < 2; side++)
    {
        if (TeamHasHuman(g_pTeams[side]))
            return g_pTeams[side];
    }
    return NULL;
}

bool Active()
{
    return PortModChallengeGetActive() == PORT_CHALLENGE_JOGO_BONITO;
}

// One of our fielders that a pad is driving right now.
bool IsOurControlled(cPlayer* pPlayer)
{
    cTeam* pOurs = OurTeam();
    return pPlayer != NULL && pOurs != NULL && pPlayer->m_eClassType == FIELDER && pPlayer->m_pTeam == pOurs
        && pPlayer->GetGlobalPad() != NULL;
}

void Add(int nPoints)
{
    if (!Active())
        return;
    s_nMeter += nPoints;
    if (s_nMeter > kMeterTarget)
        s_nMeter = kMeterTarget;
    PortModChallengeSetMeter(s_nMeter, kMeterTarget);
}

bool IsShell(const PowerupBase* pShell)
{
    return pShell->m_eType == POWER_UP_GREEN_SHELL || pShell->m_eType == POWER_UP_RED_SHELL
        || pShell->m_eType == POWER_UP_SPINY_SHELL || pShell->m_eType == POWER_UP_FREEZE_SHELL;
}

float DistSq2D(const nlVector3& a, const nlVector3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Graze* FindGraze(PowerupBase* pShell)
{
    for (int i = 0; i < 8; i++)
    {
        if (s_grazes[i].pShell == pShell)
            return &s_grazes[i];
    }
    return NULL;
}

// Every fielder of our team gets the Star, the way Estrella Intermitente hands it out (effect timer +
// EmitStar, expired by the fielder's own update through KillStar). A fielder mid-throw is skipped:
// the throw's trigger would otherwise re-run ThrowPowerup on the Star and make it a full-length one.
void GiveTeamStar(cTeam* pTeam)
{
    for (int i = 0; i < 4; i++)
    {
        cFielder* pFielder = pTeam->GetFielder(i);
        if (pFielder == NULL || PortShockFantasmaIsEliminated(pFielder) || pFielder->IsPlayingPowerupAnim())
            continue;
        const bool bEffectRunning = pFielder->m_tPowerupEffectTime.m_uPackedTime != 0;
        if (pFielder->m_ePowerup == POWER_UP_STAR && bEffectRunning)
            continue; // already a Star
        if (pFielder->m_ePowerup == POWER_UP_MUSHROOM && bEffectRunning)
            KillMushroom(pFielder);
        pFielder->m_ePowerup = POWER_UP_STAR;
        pFielder->mnNumPowerups = 1;
        pFielder->m_tPowerupEffectTime.SetSeconds(kStarSeconds);
        EmitStar(pFielder);
    }
}

// L and R held together on any pad of our team (analog pull or the digital click).
bool TriggersDown(cTeam* pTeam)
{
    for (int i = 0; i < 5; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        cGlobalPad* pPad = pPlayer != NULL ? pPlayer->GetGlobalPad() : NULL;
        if (pPad != NULL && pPad->GetPressure(0x40, false) >= 0.5f && pPad->GetPressure(0x20, false) >= 0.5f)
            return true;
    }
    return false;
}

} // namespace

void PortJogoBonitoGoal(bool bChip)
{
    if (bChip)
        Add(3);
}

void PortJogoBonitoDeke(cFielder* pFielder)
{
    if (IsOurControlled(pFielder))
        Add(1);
}

void PortJogoBonitoBallTaken(cPlayer* pTaker)
{
    if (!Active() || !IsOurControlled(pTaker) || g_pBall == NULL)
        return;
    cPlayer* pTarget = g_pBall->m_pPassTarget;
    cPlayer* pPasser = g_pBall->m_pPrevOwner;
    if (pTarget == NULL || pPasser == NULL)
        return;
    if (pTarget->m_pTeam != pTaker->m_pTeam && pPasser->m_pTeam != pTaker->m_pTeam)
    {
        // Cut out the opponent's pass - unless its receiver was knocked over to get it.
        if (pTarget->m_eClassType != FIELDER || !((cFielder*)pTarget)->IsFallenDown(0.0f))
            Add(1);
    }
    else if (g_pBall->mbHyperSTS && pTarget == pTaker && pPasser->m_pTeam == pTaker->m_pTeam && pPasser != pTaker)
        Add(1); // our perfect pass, received
}

void PortJogoBonitoShellTick(PowerupBase* pShell)
{
    if (!Active() || !IsShell(pShell))
        return;

    Graze* pGraze = FindGraze(pShell);
    if (pGraze != NULL)
    {
        cFielder* pFielder = pGraze->pFielder;
        if (pFielder->IsFallenDown(0.0f))
        {
            pGraze->pShell = NULL; // it got them
        }
        else if (DistSq2D(pShell->m_v3Position, pFielder->m_v3Position) > kGrazeFar * kGrazeFar)
        {
            pGraze->pShell = NULL;
            Add(1); // went past
        }
        return;
    }

    // Only the opponent's shells, and only against the fielder a pad is driving.
    cTeam* pOurs = OurTeam();
    if (pOurs == NULL || pShell->m_pThrower == NULL || pShell->m_pThrower->m_pTeam == pOurs)
        return;
    for (int i = 0; i < 4; i++)
    {
        cFielder* pFielder = pOurs->GetFielder(i);
        if (pFielder == NULL || pFielder->GetGlobalPad() == NULL || pFielder->IsFallenDown(0.0f) || pFielder->IsInvincible())
            continue;
        if (DistSq2D(pShell->m_v3Position, pFielder->m_v3Position) > kGrazeNear * kGrazeNear)
            continue;
        for (int j = 0; j < 8; j++)
        {
            if (s_grazes[j].pShell == NULL)
            {
                s_grazes[j].pShell = pShell;
                s_grazes[j].pFielder = pFielder;
                break;
            }
        }
        return;
    }
}

void PortJogoBonitoShellGone(PowerupBase* pShell)
{
    // Destroyed mid-graze (it hit someone, or a wall): no point.
    Graze* pGraze = FindGraze(pShell);
    if (pGraze != NULL)
        pGraze->pShell = NULL;
}

void PortJogoBonitoUpdate(bool bInPlay)
{
    if (!Active())
    {
        if (s_nMeter != 0)
            PortJogoBonitoReset();
        return;
    }
    PortModChallengeSetMeter(s_nMeter, kMeterTarget);

    cTeam* pOurs = OurTeam();
    const bool bDown = pOurs != NULL && TriggersDown(pOurs);
    const bool bPressed = bDown && !s_bTriggersWereDown;
    s_bTriggersWereDown = bDown;

    if (bPressed && s_nMeter >= kMeterTarget && bInPlay && !g_pGame->mbCaptainShotToScoreOn)
    {
        GiveTeamStar(pOurs);
        s_nMeter = 0;
        PortModChallengeSetMeter(s_nMeter, kMeterTarget);
    }
}

void PortJogoBonitoReset()
{
    s_nMeter = 0;
    s_bTriggersWereDown = false;
    for (int i = 0; i < 8; i++)
        s_grazes[i].pShell = NULL;
    PortModChallengeSetMeter(0, 0);
}
