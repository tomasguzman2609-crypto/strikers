// Simple WAV-file voice/SFX replacements, mixed on top of MusyX's own output.
//
// Drop a 16-bit PCM .wav named after a sound resource (as SebringSoundDefines.cpp
// spells it, e.g. "SFXCHAR_BOWSER_Activate.wav") into a `sfx` folder beside the
// game's executable. Any sample rate or channel count works; it is converted to
// the mixer's format once, at load time. Nothing needs to be recompiled to swap
// or add a clip - only to wire a new call site up to PortCustomSFXPlay().

#ifndef PORT_CUSTOM_SFX_H
#define PORT_CUSTOM_SFX_H

#ifdef __cplusplus
extern "C" {
#endif

// Locates the sfx/ folder beside the executable. Cheap even if it is missing
// or empty; individual clips are loaded lazily, on first use. Call once at
// startup, alongside PortTexturesInit().
void PortCustomSFXInit(void);

// Starts `name`.wav from the beginning, mixed in on top of whatever MusyX is
// already playing. Returns 1 if sfx/<name>.wav exists (whether or not it
// decoded and started cleanly), 0 if there is no such file - so a call site
// can fall back to the game's own sound only when nothing was dropped in.
//
// Slows down with the sim's slow-motion time scale (see PortSimTimeScalePtr
// in FixedUpdateTask.cpp), like the game's own sounds, but never below 0.75x
// (kSlowMoFloor in custom_sfx.cpp) so it stays intelligible. At normal speed
// it plays at its natural rate.
int PortCustomSFXPlay(const char* name);

// For the voice lines played across a Super Strike / HyperStrike (windup,
// kick, charge, shot): unlike PortCustomSFXPlay these never slow down - they
// always play at normal speed, however slow the cinematic runs (see
// kVoiceLineMinScale in custom_sfx.cpp).
int PortCustomSFXPlayFixed(const char* name);

// sfx/sonidos.txt: replace any of the game's sounds without recompiling, one
// line per sound, '#' for comments:
//   SOUND_NAME = file.wav, volume%, on|off
// file.wav lives in sfx/ ("-" silences the game's sound instead); the volume
// (0-200, default 100) and on/off (default on; off = the game's own sound) are
// optional. Re-read about once a second, so it can be edited with the game
// running. A line for one of the clips the game already plays by name above
// (e.g. SFXCHAR_WARIO_EFFORTS_Electrocute_01) sets its file and volume too.
//
// The game registers every sound name with its MusyX id at startup, and then
// calls PortCustomSFXNamesReady(), which writes them to
// sfx/_lista_de_sonidos.txt for reference.
void PortCustomSFXRegisterName(unsigned long sfxID, const char* name);
void PortCustomSFXNamesReady(void);

// Asked by the platform audio layer before it starts game sound `sfxID`:
// returns 1 if sonidos.txt replaces it (the replacement has started, or it is
// silenced), so the game's own sound must not play; 0 to play it as usual.
// Replacements play unpositioned, at their own volume.
int PortCustomSFXReplace(unsigned long sfxID);

// Answers the sound editor's preview requests (sfx/_preview.txt: play a game
// sound as it really is, or its assigned clip at its volume). Cheap; call once
// per frame from the main thread.
void PortCustomSFXPoll(void);

// Extracting the game's own sounds into sfx/_originales/<NAME>.wav, for the
// sound editor: with STRIKERS_DUMP_SFX set, the game calls DumpBegin, then
// DumpLoaded once for each sound group it loads (it records every sound the
// loaded groups hold that it hasn't yet, each on its own, driving MusyX
// directly rather than through the audio device), then DumpEnd, and exits.
// sfx/_originales/_progreso.txt holds "trabajando <n>" and then "listo <n>".
int PortCustomSFXDumpRequested(void);
void PortCustomSFXDumpBegin(void);
void PortCustomSFXDumpLoaded(void);
void PortCustomSFXDumpEnd(void);

// Adds any currently-playing custom clips into `pcm` (interleaved, signed
// 16-bit, stereo, `frames` sample pairs), with saturation. Called once per
// audio buffer from the platform audio backend, after MusyX has rendered it.
void PortCustomSFXMix(short* pcm, unsigned int frames);

#ifdef __cplusplus
}
#endif

#endif // PORT_CUSTOM_SFX_H
