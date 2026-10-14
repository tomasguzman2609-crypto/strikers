#ifndef _CHOMPREFEREE_H_
#define _CHOMPREFEREE_H_

// PORT: challenge roulette - "Chomp Árbitro" (PORT_CHALLENGE_CHOMP_REFEREE). Every tackle or slide a
// team with a real local player lands on its opponent is a "foul"; the 10th sends a Chain Chomp after
// that team, as if the opponent had thrown one, and the count starts over. The count is shown on
// screen through PortModChallengeSetMeter (a red bar, unlike the achievements' gold one).

class cFielder;
class cPlayer;

// From the start of the hit and slide reactions, next to PortAchievementHooksTackle.
void PortChompRefereeTackle(cFielder* pVictim, cPlayer* pAttacker);

// Once per frame from cGame::Update. bInPlay: GS_GAMEPLAY or GS_OVERTIME. Releases a due Chomp as
// soon as the field is clear for it (no Chomp or Bowser out, no Super Strike), and forgets the count
// while the challenge isn't active (e.g. Maldición Rotativa drew another one).
void PortChompRefereeUpdate(bool bInPlay);

// From cGame::BeginGame.
void PortChompRefereeReset();

#endif // _CHOMPREFEREE_H_
