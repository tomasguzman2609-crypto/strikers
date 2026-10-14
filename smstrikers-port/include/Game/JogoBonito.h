#ifndef _JOGOBONITO_H_
#define _JOGOBONITO_H_

// PORT: challenge roulette - "Jogo Bonito" (PORT_CHALLENGE_JOGO_BONITO). Stylish plays by the team with
// a real local player fill a meter to 5 (shown through PortModChallengeSetMeter):
//   - a goal from a chip shot ("globito"): +3
//   - a successful deke with the turbo boost, by the fielder a pad controls: +1
//   - cutting out an opponent's pass by taking the ball (not by knocking anyone over), by the fielder
//     a pad controls: +1
//   - completing a perfect (slow-motion) pass to the fielder a pad controls: +1
//   - an opponent's shell (green, red, spiny or freeze - not bananas or Bob-ombs) grazing the fielder
//     a pad controls and going past without knocking them over: +1
// Once full, pressing L and R together gives the whole team a 1.3s Star and empties the meter.

class cFielder;
class cPlayer;
class PowerupBase;

// A goal by our side; bChip when it came from a chip shot. From PortAchievementHooksGoal.
void PortJogoBonitoGoal(bool bChip);

// From PortAchievementHooksDekeBoost.
void PortJogoBonitoDeke(cFielder* pFielder);

// From PortAchievementHooksBallTaken (cPlayer::PickupBall, before the ball changes hands).
void PortJogoBonitoBallTaken(cPlayer* pTaker);

// From each shell's Update, once its position is current; and from ~PowerupBase.
void PortJogoBonitoShellTick(PowerupBase* pShell);
void PortJogoBonitoShellGone(PowerupBase* pShell);

// Once per frame from cGame::Update. bInPlay: GS_GAMEPLAY or GS_OVERTIME.
void PortJogoBonitoUpdate(bool bInPlay);

// From cGame::BeginGame.
void PortJogoBonitoReset();

#endif // _JOGOBONITO_H_
