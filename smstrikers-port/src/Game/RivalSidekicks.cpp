// PORT: challenge roulette extras - "Compañeros únicos rival". See Game/RivalSidekicks.h.
#include "Game/RivalSidekicks.h"
#include "Game/AI/Fielder.h"
#include "Game/CharacterTriggers.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "NL/nlMath.h"
#include "port/mod_challenge.h"

namespace
{

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

// A sidekick of the given class on a CPU-only team, with the extra switched on.
bool IsRivalSidekick(cPlayer* pPlayer, eCharacterClass eClass)
{
    return pPlayer != NULL && PortModExtraRivalSidekicks() && pPlayer->m_eClassType == FIELDER
        && pPlayer->m_eCharacterClass == eClass && !TeamHasHuman(pPlayer->m_pTeam);
}

} // namespace

float PortRivalSidekickSpeedScale(cFielder* pFielder)
{
    return IsRivalSidekick(pFielder, TOAD) ? 1.25f : 1.0f;
}

float PortRivalSidekickShotTimeScale(cFielder* pShooter)
{
    return IsRivalSidekick(pShooter, HAMMERBROS) ? (1.0f / 1.5f) : 1.0f;
}

float PortRivalSidekickPassSpeedScale(cPlayer* pPasser)
{
    return IsRivalSidekick(pPasser, BIRDO) ? 1.4f : 1.0f;
}

bool PortRivalSidekickResistsTackle(cFielder* pVictim, cPlayer* pAttacker)
{
    if (!IsRivalSidekick(pVictim, KOOPA) || pVictim->IsFallenDown(0.0f))
        return false;
    // Only a real tackle or slide by the human side - not powerups, Bowser or a Star.
    if (pAttacker == NULL || pAttacker->m_eClassType != FIELDER || !((cFielder*)pAttacker)->IsAttacking()
        || ((cFielder*)pAttacker)->IsInvincible() || !TeamHasHuman(pAttacker->m_pTeam))
        return false;
    if (nlRandomf(1.0f, &nlDefaultSeed) >= 0.5f)
        return false;
    // Something to see: the shell flashes like a goalie's deflection.
    EmitGoalieCatch(pVictim, "goalie_deflect", false);
    return true;
}
