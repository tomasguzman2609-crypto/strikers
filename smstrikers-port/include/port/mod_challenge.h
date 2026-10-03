// "Challenge roulette" support: a tiny external companion app writes a challenge ID into
// mods/challenge.txt (a single integer, beside the game's executable); the game polls that file
// and applies the matching modifier to Player 1 (controller pad 0) only, until the current match
// ends, at which point the game clears it itself (rewriting the file back to "0") so the
// companion app's UI can reset for the next spin.
//
// Nothing here is Mario-Strikers-specific - the actual meaning of each ePortChallenge value (which
// pad action it blocks, which powerup it forces, etc.) lives in the Game/ call sites that query
// PortModChallengeGetActive(), the same way custom_sfx.cpp knows nothing about which character
// plays which clip.

#ifndef PORT_MOD_CHALLENGE_H
#define PORT_MOD_CHALLENGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum ePortChallenge
{
    PORT_CHALLENGE_NONE = 0,
    PORT_CHALLENGE_ONLY_MUSHROOM = 1,     // P1's team always rolls POWER_UP_MUSHROOM
    PORT_CHALLENGE_INVERTED_CONTROLS = 2, // P1's left stick is negated on both axes
    PORT_CHALLENGE_NO_POWERUPS = 3,       // P1's team never has a powerup
    PORT_CHALLENGE_INFINITE_POWERUPS = 4, // P1's team always has one ready (re-rolled the instant it's used)
    PORT_CHALLENGE_SLOW = 5,              // P1 moves at half speed
    PORT_CHALLENGE_FAST = 6,              // P1 moves at 1.5x speed
    PORT_CHALLENGE_NO_PASSES = 7,         // P1's PAD_PASS (pass/cross) is blocked
    PORT_CHALLENGE_NO_TACKLES = 8,        // P1's PAD_SLIDE_ATTACK (tackle) is blocked
};

// Locates the mods/ folder beside the executable. Cheap even if it is missing; the challenge file
// is (re-)read lazily by PortModChallengeUpdate(). Call once at startup, alongside
// PortCustomSFXInit().
void PortModChallengeInit(void);

// Polls mods/challenge.txt (throttled internally - safe to call every frame) and refreshes the
// active challenge if the file changed. Call once per frame from the main game-logic tick.
void PortModChallengeUpdate(void);

// The currently active challenge (an ePortChallenge value). PORT_CHALLENGE_NONE if no mods/
// folder, no challenge.txt, or its contents don't parse as a known value.
int PortModChallengeGetActive(void);

// Resets the active challenge to PORT_CHALLENGE_NONE, both in memory and by rewriting
// challenge.txt, so the companion app can tell the match ended and offer another spin. Call this
// exactly once per match end (not every frame).
void PortModChallengeClear(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_MOD_CHALLENGE_H
