#ifndef _REINFORCEMENTS_H_
#define _REINFORCEMENTS_H_

// PORT: challenge roulette - "Refuerzos" (PORT_CHALLENGE_REINFORCEMENTS). A team with a real local
// player starts the match with only its captain on the pitch, however many local players it has; the
// rest wait on the bench - parked through Shock Fantasma's machinery - and run on
// one at a time, 10s, 25s and 45s into play, each with a Mushroom. Only at the start of the match.
//
// The passing code always needs a teammate to pass to, so while a team is down to one fielder its pad
// can't pass (PortReinforcementsBlocksPass), and a kickoff brings the next one on early.

class cFielder;

// Once per frame from cGame::Update. bInPlay: GS_GAMEPLAY or GS_OVERTIME. The bench empties as soon
// as play first starts (the opening kickoff is taken with the full team).
void PortReinforcementsUpdate(float fDeltaT, bool bInPlay);

// From cGame::InitGameState(GS_KICKOFF), before ResetForKickOff: a benched team down to one fielder
// gets its next one now, so the kickoff pass has a teammate.
void PortReinforcementsBeforeKickoff();

// True if pFielder's pad may not pass: its team is down to a single fielder on the pitch.
bool PortReinforcementsBlocksPass(cFielder* pFielder);

// True if pFielder is on the bench - a second local player's pad can be parked there until its
// fielder comes on (it is brought on first), and does nothing meanwhile.
bool PortReinforcementsPadOnBench(cFielder* pFielder);

// From cGame::BeginGame, after PortShockFantasmaReset.
void PortReinforcementsReset();

#endif // _REINFORCEMENTS_H_
