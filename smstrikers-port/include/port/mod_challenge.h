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
//     keeps the ball for more than 2s: an explosion, a bomb knockdown, and the ball goes loose.
//     Should be labeled "Papa Caliente" in the companion app.
//   - PORT_CHALLENGE_ELECTRIC_TACKLES (21) turns a CPU-only team's successful tackles - both the
//     shoulder charge (ACTION_HIT) and the slide (ACTION_SLIDE_ATTACK) - on a fielder of a team with a
//     real local player into an electric-fence shock instead of the usual knockdown. Should be
//     labeled "Tackles Eléctricos" in the companion app.
//   - PORT_CHALLENGE_ROTATING_CURSE (22) is a meta-challenge: the game itself draws one of the other
//     challenges at random when the match begins and a different one after every goal.
//     PortModChallengeGetActive() reports the drawn one, so every hook works unchanged. Should be
//     labeled "Maldición Rotativa" in the companion app.
//   - PORT_CHALLENGE_OPPONENT_STAR_PULSE (23) gives every fielder of a CPU-only team a 1s Star (the
//     invincible, knock-everyone-over powerup) every 7s of play. Should be labeled "Estrella
//     Intermitente" in the companion app. Maldición Rotativa can draw it like any other.
//   - PORT_CHALLENGE_DEKE_STAR (24) is a good one: a successful deke by a fielder of a team with a
//     real local player gives a 1.5s Star instead of the usual short speed boost. Should be labeled
//     "Esquive Estelar" in the companion app.
//   - PORT_CHALLENGE_PAPER_GOALIE (25) makes the goalie of a team with a real local player behave as
//     under the "stunned goalies" cheat: it never catches a shot, and every shot it saves leaves it
//     dazed. Should be labeled "Arquero de Papel" in the companion app.
//   - PORT_CHALLENGE_CHOMP_REFEREE (26) counts the tackles and slides a team with a real local
//     player lands on its opponent; every 10th sends a Chain Chomp after that team. Should be
//     labeled "Chomp Árbitro" in the companion app.
//   - PORT_CHALLENGE_VAR (27) reviews every goal a team with a real local player scores: if one of
//     that team's fielders landed a tackle or slide on an opponent in the 2s before it, the goal is
//     disallowed (it doesn't count, and in sudden death the match goes on). Every review shows a
//     verdict on screen. Should be labeled "Modo VAR" in the companion app.
//   - PORT_CHALLENGE_REFLECTING_GOALIE (28) makes a CPU-only team's goalie fire back every shot it
//     saves from a team with a real local player, the way a goalie swats back a failed Super Strike:
//     hard, straight at the shooter, knocking down whoever it hits. Not when the shot leaves the
//     goalie dazed. Should be labeled "Arquero Reflectante" in the companion app.
//   - PORT_CHALLENGE_JOGO_BONITO (29) is a good one: stylish plays by a team with a real local player
//     (chip-shot goals, dekes, interceptions, perfect passes, dodged shells) fill a meter to 5; once
//     full, L+R together gives the whole team a 1.3s Star. Should be labeled "Jogo Bonito" in the
//     companion app.
//   - PORT_CHALLENGE_REINFORCEMENTS (30) starts a team with a real local player with only its
//     captain on the pitch; the other fielders run on 10s, 25s and 45s into play, each with a
//     Mushroom. While down to one fielder, that pad can't pass. Should be labeled "Refuerzos" in the
//     companion app.
//   - PORT_CHALLENGE_SUPER_ITEMS (31) is an event for both teams: throwable items come in bigger
//     bunches (1 -> 3, 3 -> 5 shells, bananas or Bob-ombs), always large, and Stars, Mushrooms and
//     the Chain Chomp's chase last twice as long. Should be labeled "Súper Ítems" in the companion
//     app.
enum ePortChallenge
{
    PORT_CHALLENGE_NONE = 0,
    PORT_CHALLENGE_ONLY_MUSHROOM = 1,       // every real player's team always rolls POWER_UP_MUSHROOM
    PORT_CHALLENGE_INVERTED_CONTROLS = 2,   // every real player's left stick is negated on both axes
    PORT_CHALLENGE_NO_POWERUPS = 3,         // every real player's team never has a powerup
    PORT_CHALLENGE_INFINITE_POWERUPS = 4,   // every real player's team always has one ready (re-rolled the instant it's used)
    PORT_CHALLENGE_SLOW = 5,                // every real player moves at 0.6x speed
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
    PORT_CHALLENGE_HOT_POTATO = 20,              // a human team's fielder holding the ball over 2s explodes and loses it - "Papa Caliente"
    PORT_CHALLENGE_ELECTRIC_TACKLES = 21,        // a CPU team's hit or slide on a human team's fielder electrocutes them like the fence - "Tackles Eléctricos"
    PORT_CHALLENGE_ROTATING_CURSE = 22,          // meta: a random other challenge, re-drawn at kickoff and after every goal - "Maldición Rotativa"
    PORT_CHALLENGE_OPPONENT_STAR_PULSE = 23,     // every CPU-only team fielder gets a 1s Star every 7s - "Estrella Intermitente"
    PORT_CHALLENGE_DEKE_STAR = 24,               // a human team's successful deke gives a 1.5s Star instead of the speed boost - "Esquive Estelar"
    PORT_CHALLENGE_PAPER_GOALIE = 25,            // a human team's goalie never catches, and every save leaves it dazed - "Arquero de Papel"
    PORT_CHALLENGE_CHOMP_REFEREE = 26,           // every 10 tackles/slides by a human team on the opponent send a Chain Chomp after it - "Chomp Árbitro"
    PORT_CHALLENGE_VAR = 27,                     // a human team's goal within 2s of its own tackle/slide on an opponent is disallowed - "Modo VAR"
    PORT_CHALLENGE_REFLECTING_GOALIE = 28,       // a CPU goalie's saves of a human team's shots are swatted back at the shooter - "Arquero Reflectante"
    PORT_CHALLENGE_JOGO_BONITO = 29,             // stylish plays fill a meter; full, L+R gives the human team a 1.3s Star - "Jogo Bonito"
    PORT_CHALLENGE_REINFORCEMENTS = 30,          // a human team starts with its captain alone; teammates come on at 10s, 25s and 45s - "Refuerzos"
    PORT_CHALLENGE_SUPER_ITEMS = 31,             // both teams: bigger bunches of items, always large; Stars/Mushrooms/Chomps last twice as long - "Súper Ítems"

    PORT_CHALLENGE_COUNT                         // one past the last valid ID
};

// Locates the mods/ folder beside the executable. Cheap even if it is missing; the challenge file
// is (re-)read lazily by PortModChallengeUpdate(). Call once at startup, alongside
// PortCustomSFXInit().
void PortModChallengeInit(void);

// Polls mods/challenge.txt (throttled internally - safe to call every frame) and refreshes the
// active challenge if the file changed. Call once per frame from the main game-logic tick.
void PortModChallengeUpdate(void);

// The currently active challenge (an ePortChallenge value). PORT_CHALLENGE_NONE if no mods/
// folder, no challenge.txt, or its contents don't parse as a known value. Under
// PORT_CHALLENGE_ROTATING_CURSE this is the challenge currently drawn, never the meta value itself.
int PortModChallengeGetActive(void);

// The value challenge.txt actually selects, i.e. PORT_CHALLENGE_ROTATING_CURSE itself rather than
// what it drew. For display.
int PortModChallengeGetSelected(void);

// Under PORT_CHALLENGE_ROTATING_CURSE, draws a new challenge (never the same one twice in a row);
// otherwise does nothing. The game calls this when a match begins and after every goal.
void PortModChallengeReroll(void);

// The Spanish display name of a challenge ID, as the companion app shows it; "" for
// PORT_CHALLENGE_NONE or an unknown ID. Kept in step with the companion app's CHALLENGES list.
const char* PortModChallengeName(int challenge);

// Files beside challenge.txt that the companion app reads, for the achievements' reward:
//   - mods/skips.txt holds the number of roulette re-spins ("skips") the player has banked. The game
//     adds one with PortModChallengeAwardSkip(); the companion app spends them.
//   - mods/match_status.txt is "1" while a match is being played and "0" otherwise, so the companion
//     app only offers a skip between matches.
void PortModChallengeAwardSkip(void);
void PortModChallengeSetMatchPlaying(int playing);

// The game calls this once per frame while a match is being updated, so the on-screen challenge
// banner (overlay.cpp) can tell a match apart from menus and loading screens.
void PortModChallengeNoteMatchFrame(void);
unsigned int PortModChallengeMatchFrameCount(void);

// A challenge's own on-screen counter, for the overlay (e.g. Chomp Árbitro's fouls toward the next
// Chain Chomp): progress out of target, target 0 when the active challenge has none. The game sets
// it; the overlay shows a red bar each time progress goes up.
void PortModChallengeSetMeter(int progress, int target);
void PortModChallengeGetMeter(int* progress, int* target);

// Modo VAR's verdict on a goal, for the overlay's popup: the game reports each review, and the
// overlay shows the latest one whenever the review count goes up.
void PortModChallengeVarVerdict(int disallowed);
unsigned int PortModChallengeVarReviewCount(void);
int PortModChallengeVarLastDisallowed(void);

// Optional veto on a pad action, for challenges that depend on game state the input layer can't see
// (e.g. which fielder a pad currently controls). The game registers one at startup; the platform pad
// code asks PortModChallengeActionBlocked() before reporting an action as pressed/released. action is
// the game's ePadActions id; return non-zero to make that action read as not pressed for that pad.
typedef int (*PortModChallengeActionFilter)(int padIndex, int action);
void PortModChallengeSetActionFilter(PortModChallengeActionFilter filter);
int PortModChallengeActionBlocked(int padIndex, int action);

// Extras: switches in the companion app that apply on top of (and independently of) the roulette's
// challenge, kept in mods/extras.txt as "name=0/1" lines and polled along with challenge.txt.
// "rival_sidekicks": "Compañeros únicos rival" (see Game/RivalSidekicks.h). Non-zero when on.
int PortModExtraRivalSidekicks(void);

// Resets the active challenge to PORT_CHALLENGE_NONE, both in memory and by rewriting
// challenge.txt, so the companion app can tell the match ended and offer another spin. Call this
// exactly once per match end (not every frame).
void PortModChallengeClear(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_MOD_CHALLENGE_H