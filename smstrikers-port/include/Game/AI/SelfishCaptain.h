#ifndef _SELFISHCAPTAIN_H_
#define _SELFISHCAPTAIN_H_

// PORT: challenge roulette - "Capitán Egoísta" (PORT_CHALLENGE_SELFISH_CAPTAIN). On a team with a real
// local player only the captain may shoot at goal. Human sidekicks: the shoot button reads as not
// pressed (via the pad action filter), so no shot of any kind - charged, one-timer, volley or
// loose-ball - ever starts. AI sidekicks: their shot decisions are refused at the desire/loose-ball
// call sites below instead, since they never touch a pad. CPU-only teams are unaffected.

class cFielder;

// True if pFielder may not shoot under the active challenge.
bool PortSelfishCaptainBlocksShot(cFielder* pFielder);

// Registers the pad action filter with the challenge module. Call once at startup.
void PortSelfishCaptainInit();

#endif // _SELFISHCAPTAIN_H_
