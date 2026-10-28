#ifndef _RIVALSIDEKICKS_H_
#define _RIVALSIDEKICKS_H_

// PORT: challenge roulette extras - "Compañeros únicos rival", a switch in the companion app
// (mods/extras.txt, see PortModExtraRivalSidekicks) independent of the roulette's challenge. With it
// on, each sidekick of a CPU-only team gets a buff of its own:
//   - Toad:        moves 25% faster.
//   - Hammer Bros: regular shots fly 50% faster (same target, shorter flight time).
//   - Koopa:       spiky shell - a tackle or slide from a team with a real local player comes
//                  straight back: the Koopa takes it as usual, and the attacker gets the same hit
//                  or slide too.
//   - Birdo:       passes 40% faster.
// Teams with a real local player are never affected.

class cFielder;
class cPlayer;

// Multiplier on a fielder's desired and top speed (Toad).
float PortRivalSidekickSpeedScale(cFielder* pFielder);

// Multiplier on a regular shot's flight time (Hammer Bros: less than 1).
float PortRivalSidekickShotTimeScale(cFielder* pShooter);

// Multiplier on a pass's ground speed (Birdo).
float PortRivalSidekickPassSpeedScale(cPlayer* pPasser);

// From the start of the hit (bSlide false) and slide (bSlide true) reactions: if pVictim is a CPU
// Koopa, sends the move back at pAttacker too (true if it did), on the next PortRivalSidekicksUpdate.
// The victim's own reaction still goes ahead.
bool PortRivalSidekickSpikes(cFielder* pVictim, cPlayer* pAttacker, bool bSlide);

// Applies the bounces PortRivalSidekickSpikes queued, outside the collision code that queued them.
// Once per frame from cGame::Update; bPlaying is false outside open play.
void PortRivalSidekicksUpdate(bool bPlaying);

// Forgets any queued bounce; at the start of every match.
void PortRivalSidekicksReset();

#endif // _RIVALSIDEKICKS_H_
