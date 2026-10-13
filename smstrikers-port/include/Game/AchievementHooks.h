#ifndef _ACHIEVEMENTHOOKS_H_
#define _ACHIEVEMENTHOOKS_H_

// PORT: challenge roulette - turns what happens on the pitch into port/achievements.h events. Each
// function decides whether the moment counts for "our" side (the team with a real local player) and
// does nothing during a demo or other match with no human side.

class cPlayer;
class cFielder;

// From cGame::BeginGame: draws this match's achievements (or none if no team has a human).
void PortAchievementHooksBeginMatch();

// From every shot init (InitActionShot / InitActionOneTimer / InitActionLooseBallShot), with the final
// chip decision, so a goal can later tell whether it came from a "globito".
void PortAchievementHooksNoteShot(cFielder* pShooter, bool bIsChipShot);

// From cGame::CheckForGoal once the score is updated. bSuperStrike: a Super Strike goal.
void PortAchievementHooksGoal(int nScoringSide, cPlayer* pScorer, bool bSuperStrike, float fGameSeconds);

// From the start of the hit and slide reactions, before the victim's state changes.
void PortAchievementHooksTackle(cFielder* pVictim, cPlayer* pAttacker);

// From InitActionElectrocution.
void PortAchievementHooksElectrocuted(cFielder* pFielder);

// From PowerupBase::CollisionCallback, for each fielder an item (shell, banana) hits directly.
void PortAchievementHooksItemHit(const void* pItem, cPlayer* pThrower, cFielder* pVictim);

// From PowerupBase::Destroy: the item is gone, and its slot may be reused by a new one.
void PortAchievementHooksItemGone(const void* pItem);

// From the Bob-omb explosion (Character.cpp, event 0x2C): how many fielders it knocked down.
void PortAchievementHooksBlastHits(cPlayer* pThrower, int nHits);

// From the deke's turbo boost (ActionDeke).
void PortAchievementHooksDekeBoost(cFielder* pFielder);

// From GS_END_GAME.
void PortAchievementHooksMatchEnd();

#endif // _ACHIEVEMENTHOOKS_H_
