// PORT: challenge roulette - "Modo VAR". See Game/VarReview.h.
#include "Game/VarReview.h"
#include "Game/AI/Fielder.h"
#include "Game/Game.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "port/mod_challenge.h"

namespace
{

const float kFoulWindowSeconds = 2.0f;

// Game time of each side's last foul, indexed by g_pTeams side; negative when none this match.
float s_fLastFoul[2] = { -1.0f, -1.0f };

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

} // namespace

void PortVarTackle(cFielder* pVictim, cPlayer* pAttacker)
{
    if (PortModChallengeGetActive() != PORT_CHALLENGE_VAR || g_pGame == NULL)
        return;
    if (pAttacker == NULL || pAttacker->m_eClassType != FIELDER || !((cFielder*)pAttacker)->IsAttacking())
        return;
    if (pVictim->m_pTeam == pAttacker->m_pTeam || !TeamHasHuman(pAttacker->m_pTeam))
        return;

    for (int side = 0; side < 2; side++)
    {
        if (g_pTeams[side] == pAttacker->m_pTeam)
            s_fLastFoul[side] = g_pGame->GetGameTime();
    }
}

bool PortVarReviewGoal(int nScoringSide, float fGameSeconds)
{
    if (PortModChallengeGetActive() != PORT_CHALLENGE_VAR || nScoringSide < 0 || nScoringSide > 1
        || !TeamHasHuman(g_pTeams[nScoringSide]))
        return false;

    const float fLastFoul = s_fLastFoul[nScoringSide];
    const bool bDisallowed = fLastFoul >= 0.0f && fGameSeconds - fLastFoul <= kFoulWindowSeconds;
    PortModChallengeVarVerdict(bDisallowed ? 1 : 0);
    return bDisallowed;
}

void PortVarReset()
{
    s_fLastFoul[0] = s_fLastFoul[1] = -1.0f;
}
