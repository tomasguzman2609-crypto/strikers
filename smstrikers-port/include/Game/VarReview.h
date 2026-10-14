#ifndef _VARREVIEW_H_
#define _VARREVIEW_H_

// PORT: challenge roulette - "Modo VAR" (PORT_CHALLENGE_VAR). Remembers when each team with a real
// local player last landed a tackle or slide on an opponent, and reviews that team's goals: one
// scored within 2s of game time of such a foul is disallowed. Every review is reported to the
// overlay (PortModChallengeVarVerdict), which shows "VAR: GOL" or "VAR: GOL ANULADO".

class cFielder;
class cPlayer;

// From the start of the hit and slide reactions, next to PortAchievementHooksTackle.
void PortVarTackle(cFielder* pVictim, cPlayer* pAttacker);

// From cGame::CheckForGoal, before the score changes: true if this goal by g_pTeams[nScoringSide]
// is disallowed. Does nothing (false) unless Modo VAR is active and the scoring team has a human.
bool PortVarReviewGoal(int nScoringSide, float fGameSeconds);

// From cGame::BeginGame.
void PortVarReset();

#endif // _VARREVIEW_H_
