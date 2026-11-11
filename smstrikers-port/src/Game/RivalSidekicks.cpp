// PORT: challenge roulette extras - "Compañeros únicos rival". See Game/RivalSidekicks.h.
#include "Game/RivalSidekicks.h"
#include "Game/AI/Fielder.h"
#include "Game/CharacterTriggers.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "NL/nlMath.h"
#include "port/mod_challenge.h"
#include "Game/GameInfo.h"
#include "Game/CharacterTemplate.h"
#include "Game/FE/feHelpFuncs.h"

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

// The attacker's reaction to the bounce plays this much faster: 1 / 2.5 = 40% of the time.
const float kSpikeReactSpeed = 2.5f;
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
        eFielderActionState eReact;
        if (spike.bSlide)
        {
            pHitter->InitActionSlideAttackReact(spike.pKoopa, false);
            eReact = ACTION_SLIDE_ATTACK_REACT;
        }
        else
        {
            float dx = pHitter->m_v3Position.x - spike.pKoopa->m_v3Position.x;
            float dy = pHitter->m_v3Position.y - spike.pKoopa->m_v3Position.y;
            unsigned short aAway = (unsigned short)(int)(10430.378f * nlATan2f(dy, dx));
            pHitter->InitActionHitReact(spike.pKoopa, aAway, false);
            eReact = ACTION_HIT_REACT;
        }
        // The bounce knocks the attacker down for 60% less time than a real hit or slide would:
        // its reaction plays 2.5x as fast (the reaction ends with its animation).
        if (pHitter->m_eActionState == eReact && pHitter->m_pCurrentAnimController != NULL)
        {
            pHitter->m_pCurrentAnimController->m_fPlaybackSpeedScale = kSpikeReactSpeed;
        }
    }
    s_pendingCount = 0;
}

void PortRivalSidekicksReset()
{
    s_pendingCount = 0;
}

// ---- "Companeros mezclados" ----

namespace
{

bool s_bMixDrawn[2] = { false, false };
eCharacterClass s_aMix[2][4];

bool SideHasHumanPad(int nSide)
{
    GameInfoManager* pInfo = nlSingleton<GameInfoManager>::Instance();
    for (unsigned short pad = 0; pad < 4; pad++)
        if (pInfo->GetPlayingSide(pad) == nSide)
            return true;
    return false;
}

bool IsMixableSidekick(eCharacterClass eClass)
{
    return eClass == TOAD || eClass == KOOPA || eClass == HAMMERBROS || eClass == BIRDO;
}

} // namespace

eCharacterClass PortMixedSidekickClass(int nSide, int nIndex, eCharacterClass eOfficial)
{
    if (nSide < 0 || nSide > 1 || nIndex < 1 || nIndex > 3)
        return eOfficial;
    if (!s_bMixDrawn[nSide])
    {
        // Drawn once per match, the first time either the sound loading or the character creation
        // asks, so both see the same three.
        s_bMixDrawn[nSide] = true;
        for (int i = 0; i < 4; i++)
            s_aMix[nSide][i] = eOfficial;
        // A Challenge Super Bowser Cup match follows the cup's own switch; any other, the general one.
        GameInfoManager* pInfo = nlSingleton<GameInfoManager>::Instance();
        const bool bMixed = (pInfo->GetCurrentMode() == GameInfoManager::GM_SUPER_BOWSER_CUP && PortChallengeCupIsActive())
            ? PortChallengeCupMixed() != 0
            : PortModExtraMixedSidekicks() != 0;
        if (bMixed && IsMixableSidekick(eOfficial) && !SideHasHumanPad(nSide))
        {
            // The first stays the team's own kind (its cutscenes and voices are built around it);
            // the other two are two different other kinds.
            eCharacterClass aOthers[3];
            int nOthers = 0;
            const eCharacterClass aAll[4] = { TOAD, KOOPA, HAMMERBROS, BIRDO };
            for (int i = 0; i < 4; i++)
                if (aAll[i] != eOfficial)
                    aOthers[nOthers++] = aAll[i];
            const int nSkip = nlRandom(3, &nlDefaultSeed); // the one of the three left out
            int nOut = 2;
            for (int i = 0; i < 3 && nOut <= 3; i++)
                if (i != nSkip)
                    s_aMix[nSide][nOut++] = aOthers[i];
            // And the order shuffled, so the team's own kind isn't always in the same spot.
            const int nSwap = 1 + nlRandom(3, &nlDefaultSeed);
            eCharacterClass eTmp = s_aMix[nSide][1];
            s_aMix[nSide][1] = s_aMix[nSide][nSwap];
            s_aMix[nSide][nSwap] = eTmp;
        }
    }
    return s_aMix[nSide][nIndex];
}

void PortMixedSidekicksReset()
{
    s_bMixDrawn[0] = s_bMixDrawn[1] = false;
}

// ---- Mixed sidekicks in cutscenes ----
//
// A cutscene's sidekick animations are made for the team's own sidekick kind (the cutscene is picked
// by that kind's name). Played on a sidekick of another kind's skeleton they would come out broken,
// or worse, so such a sidekick takes no part in the cutscene (it isn't drawn), and a goal it scored
// is celebrated by its team's sidekick of the team's own kind.

namespace
{

// True for a sidekick of a mixed team that isn't of its team's own kind.
bool IsOffKindSidekick(int nCharIndex)
{
    if (nCharIndex < 0 || nCharIndex >= 10 || g_pCharacters[nCharIndex] == NULL)
        return false;
    cPlayer* pPlayer = (cPlayer*)g_pCharacters[nCharIndex];
    if (pPlayer->m_eClassType != FIELDER || pPlayer->IsCaptain() || pPlayer->m_pTeam == NULL)
        return false;
    const eCharacterClass eOwn = pPlayer->m_eCharacterClass;
    const eCharacterClass eTeam = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick((short)pPlayer->m_pTeam->m_nSide));
    return eOwn != eTeam && IsMixableSidekick(eOwn) && IsMixableSidekick(eTeam);
}

} // namespace

bool PortNisCharacterFits(int nCharIndex)
{
    return !IsOffKindSidekick(nCharIndex);
}

int PortNisGoalScorerIndex(int nCharIndex)
{
    if (!IsOffKindSidekick(nCharIndex))
        return nCharIndex;
    // The sidekick of the team's own kind celebrates in its place.
    cPlayer* pScorer = (cPlayer*)g_pCharacters[nCharIndex];
    for (int i = 0; i < 10; i++)
    {
        if (i == nCharIndex || g_pCharacters[i] == NULL)
            continue;
        cPlayer* pPlayer = (cPlayer*)g_pCharacters[i];
        if (pPlayer->m_pTeam == pScorer->m_pTeam && pPlayer->m_eClassType == FIELDER && !pPlayer->IsCaptain()
            && !IsOffKindSidekick(i))
            return i;
    }
    return -1; // none: the cutscene's own choice stands
}
