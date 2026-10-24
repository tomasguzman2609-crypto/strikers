#ifndef _RIVALSIDEKICKS_H_
#define _RIVALSIDEKICKS_H_

// PORT: challenge roulette extras - "Compañeros únicos rival", a switch in the companion app
// (mods/extras.txt, see PortModExtraRivalSidekicks) independent of the roulette's challenge. With it
// on, each sidekick of a CPU-only team gets a buff of its own:
//   - Toad:        moves 25% faster.
//   - Hammer Bros: regular shots fly 50% faster (same target, shorter flight time).
//   - Koopa:       its shell - a 50% chance to shrug off a tackle or slide from a team with a real
//                  local player, without falling or losing the ball.
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

// From the start of the hit and slide reactions: true if pVictim (a Koopa) shrugs the tackle off,
// in which case the reaction must not happen at all.
bool PortRivalSidekickResistsTackle(cFielder* pVictim, cPlayer* pAttacker);

#endif // _RIVALSIDEKICKS_H_
