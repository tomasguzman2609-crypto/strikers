// PORT: challenge roulette - "Capitán Egoísta" (PORT_CHALLENGE_SELFISH_CAPTAIN). See
// Game/AI/SelfishCaptain.h for the overall approach.
#include "Game/AI/SelfishCaptain.h"
#include "Game/AI/Fielder.h"
#include "Game/Reinforcements.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "NL/globalpad.h"
#include "port/mod_challenge.h"

namespace
{

bool TeamHasHuman(cTeam* pTeam)
{
    for (int i = 0; i < 5; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        if (pPlayer != NULL && pPlayer->GetGlobalPad() != NULL)
            return true;
    }
    return false;
}

// Pad side: PAD_SHOOT reads as not pressed while the pad drives a fielder that may not shoot. Only
// the shoot action is touched, so passing, slides, hits and switching still work for everyone.
// PORT: challenge roulette - the one action filter the pad layer has, so it also serves "Refuerzos":
// PAD_PASS reads as not pressed while the pad's team is down to a single fielder.
int ActionFilter(int padIndex, int action)
{
    const bool bShoot = action == PAD_SHOOT && PortModChallengeGetActive() == PORT_CHALLENGE_SELFISH_CAPTAIN;
    const bool bPass = action == PAD_PASS && PortModChallengeGetActive() == PORT_CHALLENGE_REINFORCEMENTS;
    if (!bShoot && !bPass)
        return 0;

    for (int side = 0; side < 2; side++)
    {
        cTeam* pTeam = g_pTeams[side];
        if (pTeam == NULL)
            continue;
        for (int i = 0; i < 4; i++)
        {
            cFielder* pFielder = pTeam->GetFielder(i);
            cGlobalPad* pPad = pFielder->GetGlobalPad();
            if (pPad != NULL && pPad->m_padIndex == padIndex)
            {
                if (bPass)
                    return PortReinforcementsBlocksPass(pFielder) ? 1 : 0;
                return PortSelfishCaptainBlocksShot(pFielder) ? 1 : 0;
            }
        }
    }
    return 0;
}

} // namespace

bool PortSelfishCaptainBlocksShot(cFielder* pFielder)
{
    if (PortModChallengeGetActive() != PORT_CHALLENGE_SELFISH_CAPTAIN || pFielder == NULL)
        return false;

    cTeam* pTeam = pFielder->m_pTeam;
    // GetCaptain() (player slot 0) rather than IsCaptain(), which goes by character class and would
    // let a team with several captain-class characters have more than one shooter.
    return pTeam != NULL && pFielder != pTeam->GetCaptain() && TeamHasHuman(pTeam);
}

void PortSelfishCaptainInit()
{
    PortModChallengeSetActionFilter(ActionFilter);
}
