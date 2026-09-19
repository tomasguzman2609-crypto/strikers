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
int PortCustomSFXPlay(const char* name);

// Adds any currently-playing custom clips into `pcm` (interleaved, signed
// 16-bit, stereo, `frames` sample pairs), with saturation. Called once per
// audio buffer from the platform audio backend, after MusyX has rendered it.
void PortCustomSFXMix(short* pcm, unsigned int frames);

#ifdef __cplusplus
}
#endif

#endif // PORT_CUSTOM_SFX_H
