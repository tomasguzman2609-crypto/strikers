// "Challenge roulette" support: a tiny external companion app writes a challenge ID into
// mods/challenge.txt (a single integer, beside the game's executable); the game polls that file
// and applies the matching modifier until the current match ends, at which point the game clears
// it itself (rewriting the file back to "0") so the companion app's UI can reset for the next
// spin. Originally every challenge applied to Player 1 (pad 0) only; every call site now applies
// to every real local player instead (any fielder with a non-NULL cPlayer::GetGlobalPad()).
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

// Naming note for the companion app's UI (this header only has the stable numeric IDs - the
// display strings live in that separate app):
//   - PORT_CHALLENGE_NO_TACKLES (8) blocks the slide (ACTION_SLIDE_ATTACK) and should be labeled
//     "Barrida Nerfeada" in the companion app. Behavior is unchanged from before.
//   - PORT_CHALLENGE_NO_HITS (9) is new: it blocks the shoulder-charge hit (ACTION_HIT, a
//     different button/move than the slide) and should be labeled "Sin Tackles" in the companion
//     app.
enum ePortChallenge
{
    PORT_CHALLENGE_NONE = 0,
    PORT_CHALLENGE_ONLY_MUSHROOM = 1,       // every real player's team always rolls POWER_UP_MUSHROOM
    PORT_CHALLENGE_INVERTED_CONTROLS = 2,   // every real player's left stick is negated on both axes
    PORT_CHALLENGE_NO_POWERUPS = 3,         // every real player's team never has a powerup
    PORT_CHALLENGE_INFINITE_POWERUPS = 4,   // every real player's team always has one ready (re-rolled the instant it's used)
    PORT_CHALLENGE_SLOW = 5,                // every real player moves at half speed
    PORT_CHALLENGE_FAST = 6,                // every real player moves at 1.5x speed
    PORT_CHALLENGE_NO_PASSES = 7,           // every real player can't cross/lob a pass (normal grounded passes still work)
    PORT_CHALLENGE_NO_TACKLES = 8,          // every real player's slide (ACTION_SLIDE_ATTACK) is blocked - "Barrida Nerfeada"
    PORT_CHALLENGE_NO_HITS = 9,             // every real player's shoulder-charge hit (ACTION_HIT) is blocked - "Sin Tackles"
    PORT_CHALLENGE_OPPONENT_FAST = 10,      // every CPU-controlled fielder moves at 1.2x speed (a buff to the opponent, not a nerf to P1)
    PORT_CHALLENGE_OPPONENT_POWERUPS = 11,  // CPU teams can roll Star/Chain Chomp regardless of the current goal difference
    PORT_CHALLENGE_LONG_ELECTROCUTION = 12, // a real player stunned by the stadium's electric fence stays stunned twice as long (CPU unaffected)
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