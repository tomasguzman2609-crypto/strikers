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

// Spikes bounced back but not yet applied: the hit or slide reaction runs from inside the
// attacker's own collision code, which carries on afterwards as if the attacker were still sliding
// (marks the slide a success, picks the ball up, starts the missed-slide animation...). Knocking the
// attacker down right there left it in a broken state and crashed the game, so the bounce waits for
// cGame::Update, after the collision is over.
struct PendingSpike
{
    cFielder* pHitter;
    cFielder* pKoopa;
    bool bSlide;
    unsigned int uFrame; // PortModChallengeMatchFrameCount() when it hit the shell
};

const int kMaxPending = 4;
PendingSpike s_pending[kMaxPending];
int s_pendingCount = 0;

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

bool PortRivalSidekickSpikes(cFielder* pVictim, cPlayer* pAttacker, bool bSlide)
{
    if (!IsRivalSidekick(pVictim, KOOPA) || pVictim->IsFallenDown(0.0f))
        return false;
    // Only a real tackle or slide by the human side - not powerups, Bowser or a Star.
    if (pAttacker == NULL || pAttacker->m_eClassType != FIELDER || !((cFielder*)pAttacker)->IsAttacking()
        || ((cFielder*)pAttacker)->IsInvincible() || !TeamHasHuman(pAttacker->m_pTeam))
        return false;
    cFielder* pHitter = (cFielder*)pAttacker;

    // Something to see: the shell flashes like a goalie's deflection.
    EmitGoalieCatch(pVictim, "goalie_deflect", false);

    // The same move comes straight back at the attacker, from the Koopa outwards - both go down -
    // once the collision code that got us here has finished (see PendingSpike). Once per attacker.
    for (int i = 0; i < s_pendingCount; i++)
    {
        if (s_pending[i].pHitter == pHitter)
            return true;
    }
    if (s_pendingCount < kMaxPending)
    {
        PendingSpike& spike = s_pending[s_pendingCount++];
        spike.pHitter = pHitter;
        spike.pKoopa = pVictim;
        spike.bSlide = bSlide;
        spike.uFrame = PortModChallengeMatchFrameCount();
    }
    return true;
}

void PortRivalSidekicksUpdate(bool bPlaying)
{
    const unsigned int uNow = PortModChallengeMatchFrameCount();
    for (int i = 0; i < s_pendingCount; i++)
    {
        const PendingSpike& spike = s_pending[i];
        cFielder* pHitter = spike.pHitter;
        // Only straight after the hit, in open play, on someone still on their feet (a goal or the
        // whistle in between, or anything else having knocked them over already, drops it).
        if (!bPlaying || uNow - spike.uFrame > 3 || pHitter->IsFallenDown(0.0f))
            continue;
        if (spike.bSlide)
        {
            pHitter->InitActionSlideAttackReact(spike.pKoopa, false);
        }
        else
        {
            float dx = pHitter->m_v3Position.x - spike.pKoopa->m_v3Position.x;
            float dy = pHitter->m_v3Position.y - spike.pKoopa->m_v3Position.y;
            unsigned short aAway = (unsigned short)(int)(10430.378f * nlATan2f(dy, dx));
            pHitter->InitActionHitReact(spike.pKoopa, aAway, false);
        }
    }
    s_pendingCount = 0;
}

void PortRivalSidekicksReset()
{
    s_pendingCount = 0;
}
