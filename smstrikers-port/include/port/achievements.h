// Per-match achievements for the challenge roulette mod: two are drawn at random when a match
// begins, the game reports what happens on the pitch through the event calls below, and the overlay
// reads the state back for display (a counter beside the challenge tag, a notice when one is won or
// lost, and the full list while the pause menu is open).
//
// Like mod_challenge.h this knows nothing about how the game detects an event - the Game/ call sites
// decide what counts as "our" goal or a tackle and report it here. "Our" side is the team with a real
// local player on it.

#ifndef PORT_ACHIEVEMENTS_H
#define PORT_ACHIEVEMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

enum ePortAchievementState
{
    PORT_ACH_PENDING = 0,
    PORT_ACH_DONE = 1,
    PORT_ACH_FAILED = 2,
};

typedef struct PortAchievementView
{
    const char* name;        // UTF-8, Spanish
    const char* description; // UTF-8, Spanish
    int progress;
    int target;              // 1 for a one-off; >1 shows as "progress/target"
    int state;               // ePortAchievementState
} PortAchievementView;

// Draws this match's two achievements and resets all progress. From cGame::BeginGame.
void PortAchievementsBeginMatch(void);

// No achievements this match (no human side, e.g. a demo).
void PortAchievementsClear(void);

// A goal. forUs: scored by our side. Scores are after the goal. isChip: a lobbed "globito" shot;
// isSuper: a Super Strike; byCaptain: our captain scored it.
void PortAchievementsGoal(int forUs, int scoreFor, int scoreAgainst, float gameSeconds, int isChip, int isSuper,
                          int byCaptain);

// One of our fielders landed a tackle (shoulder charge or slide) on an opponent; interruptedCharge
// when the victim was winding up a shot.
void PortAchievementsTackle(int interruptedCharge);

// The opposing captain was electrocuted.
void PortAchievementsOpponentCaptainShocked(void);

// One item thrown by our side hit three players (either team's).
void PortAchievementsItemTripleHit(void);

// One of our fielders got the speed boost a successful deke with turbo gives.
void PortAchievementsDekeBoost(void);

// The match ended with this final score. From GS_END_GAME.
void PortAchievementsMatchEnd(int scoreFor, int scoreAgainst);

// Non-zero once both of this match's achievements are done, which banks a roulette skip
// (PortModChallengeAwardSkip) - once per match.
int PortAchievementsSkipAwarded(void);

// 0 before the first match (or when the match has no human side), else 2.
int PortAchievementsCount(void);
void PortAchievementsGet(int index, PortAchievementView* out);

// The pause menu calls this every frame it updates, so the overlay can show the full list exactly
// while the menu is up (it compares the count across its own frames).
void PortAchievementsNotePauseFrame(void);
unsigned int PortAchievementsPauseFrameCount(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_ACHIEVEMENTS_H
