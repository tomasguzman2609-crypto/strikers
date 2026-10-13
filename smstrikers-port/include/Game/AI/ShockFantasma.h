#ifndef _SHOCKFANTASMA_H_
#define _SHOCKFANTASMA_H_

// PORT: challenge roulette - "Shock Fantasma" (PORT_CHALLENGE_VANISH_ON_ELECTROCUTION). Any fielder
// on a team with a real local player that gets stunned by the electric fence is eliminated for the
// rest of the match when the stun ends: it vanishes, its team plays a man down, and the pad moves to a teammate.
//
// The game has no notion of a fielder leaving the pitch, so instead of removing it the eliminated
// fielder is kept alive but parked off the pitch, non-solid and invisible, and every system that
// would otherwise hand it the pad, the ball or a pass skips it via PortShockFantasmaIsEliminated().
// Eliminations last until the next match starts, even if the challenge file is cleared first
// (PortModChallengeClear() runs at GS_END_GAME, before the post-game scenes).

class cCharacter;
class cFielder;

// True if pCharacter is a fielder eliminated this match. Safe on NULL, goalies and Bowser.
bool PortShockFantasmaIsEliminated(const cCharacter* pCharacter);

// From InitActionElectrocution: remembers that this stun should eliminate the fielder, if its team
// has a real local player and the challenge is active.
void PortShockFantasmaOnElectrocuted(cFielder* pFielder);

// From the stun's explosion in ActionElectrocution: eliminates the fielder if OnElectrocuted marked
// it. A stun cut short before then (e.g. by the reset after a goal) doesn't eliminate: doing it
// from CleanUpAction instead could land mid-ResetCharacters, after it already picked this fielder
// to take the kickoff.
void PortShockFantasmaOnStunEnd(cFielder* pFielder);

// Once per frame from cGame::Update: keeps every eliminated fielder parked, hidden from the AI and
// without the pad.
void PortShockFantasmaUpdate();

// From cGame::BeginGame: forgets every elimination and turns the fielders' collision back on (a
// rematch reuses the same cTeam objects). The kickoff reset then puts them back on the pitch.
void PortShockFantasmaReset();

#endif // _SHOCKFANTASMA_H_
