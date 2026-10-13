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
//   - PORT_CHALLENGE_ONLY_CHIP_SHOTS (13) forces every shot on goal into a lobbed "globito" (chip
//     shot) and should be labeled "Modo Globito" in the companion app.
//   - PORT_CHALLENGE_SHORT_ELECTROCUTION_CPU (14) shortens the CPU's stun time on the electric
//     fence to 0.15s and should be labeled "Shocks Cortos" in the companion app.
//   - PORT_CHALLENGE_OPPONENT_DOUBLE_GOALS (15) doubles however many goals a CPU-only team's goal
//     is worth (1 -> 2 for a normal goal, 2 -> 4 for a Super Strike) and should be labeled
//     "Gol Doble Rival" in the companion app.
//   - PORT_CHALLENGE_VANISH_ON_ELECTROCUTION (16) eliminates any fielder of a team with a real local
//     player (controlled or not) for the rest of the match when the electric fence's shock ends (it
//     vanishes and its team plays a man down, never below 2 fielders), and should be labeled
//     "Shock Fatal" in the companion app. Unlike most challenges it is team-wide, not just the
//     pad holder: the fence only shocks fielders knocked into it, rarely the one being controlled.
//   - PORT_CHALLENGE_SELFISH_CAPTAIN (17) lets only the captain of a team with a real local player
//     shoot at goal (every kind of shot); for anyone else the shoot button does nothing, and AI
//     teammates pass instead. Should be labeled "Capitán Egoísta" in the companion app.
//   - PORT_CHALLENGE_OPPONENT_LEAD_LOSES (18) ends the match on the spot, as a loss, the moment a
//     CPU-only team goes ahead of a team with a real local player (like a sudden-death goal). Should
//     be labeled "Ventaja Oponente = Perdiste" in the companion app.
//   - PORT_CHALLENGE_AGGRESSIVE_BOWSER (19) makes Bowser show up every match and come back every
//     12-20s after he leaves, and his landing/jump/stomp aim at the ball carrier whenever that is a
//     fielder of a team with a real local player. Should be labeled "Bowser Agresivo" in the
//     companion app. Still needs Bowser attacks enabled in the match options.
//   - PORT_CHALLENGE_HOT_POTATO (20) blows up any fielder of a team with a real local player who
//     keeps the ball for more than 1.5s: an explosion, a bomb knockdown, and the ball goes loose.
//     Should be labeled "Papa Caliente" in the companion app.
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
    PORT_CHALLENGE_OPPONENT_POWERUPS = 11,  // CPU teams can roll Star/Chain Chomp regardless of the current goal difference, at a raised chance
    PORT_CHALLENGE_LONG_ELECTROCUTION = 12, // a real player stunned by the stadium's electric fence stays stunned twice as long (CPU unaffected)
    PORT_CHALLENGE_ONLY_CHIP_SHOTS = 13,    // every real player's shot on goal is forced to be a lobbed "globito" (chip shot) - "Modo Globito"
    PORT_CHALLENGE_SHORT_ELECTROCUTION_CPU = 14, // a CPU-controlled fielder stunned by the electric fence only stays stunned 0.15s (real players unaffected) - "Shocks Cortos"
    PORT_CHALLENGE_OPPONENT_DOUBLE_GOALS = 15,   // a CPU-only team's goal is worth double (normal goal counts as 2, a Super Strike counts as 4) - "Gol Doble Rival"
    PORT_CHALLENGE_VANISH_ON_ELECTROCUTION = 16, // any fielder of a human team shocked by the electric fence vanishes for the rest of the match (team keeps >= 2 fielders) - "Shock Fatal"
    PORT_CHALLENGE_SELFISH_CAPTAIN = 17,         // on a human team only the captain can shoot at goal; the shoot button does nothing for anyone else - "Capitán Egoísta"
    PORT_CHALLENGE_OPPONENT_LEAD_LOSES = 18,     // the match ends immediately (a loss) as soon as a CPU-only team leads a human team - "Ventaja Oponente = Perdiste"
    PORT_CHALLENGE_AGGRESSIVE_BOWSER = 19,       // Bowser comes far more often and lands on a human team's ball carrier - "Bowser Agresivo"
    PORT_CHALLENGE_HOT_POTATO = 20,              // a human team's fielder holding the ball over 1.5s explodes and loses it - "Papa Caliente"
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

// Optional veto on a pad action, for challenges that depend on game state the input layer can't see
// (e.g. which fielder a pad currently controls). The game registers one at startup; the platform pad
// code asks PortModChallengeActionBlocked() before reporting an action as pressed/released. action is
// the game's ePadActions id; return non-zero to make that action read as not pressed for that pad.
typedef int (*PortModChallengeActionFilter)(int padIndex, int action);
void PortModChallengeSetActionFilter(PortModChallengeActionFilter filter);
int PortModChallengeActionBlocked(int padIndex, int action);

// Resets the active challenge to PORT_CHALLENGE_NONE, both in memory and by rewriting
// challenge.txt, so the companion app can tell the match ended and offer another spin. Call this
// exactly once per match end (not every frame).
void PortModChallengeClear(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_MOD_CHALLENGE_H