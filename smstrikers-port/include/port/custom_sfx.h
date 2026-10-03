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
// Always plays at normal speed, regardless of the sim's current slow-motion
// time scale (see PortSimTimeScalePtr in FixedUpdateTask.cpp). Use this for
// ordinary one-shot drop-ins (a hit, a tackle, the ball clanging off the
// post) that are not themselves part of a slow-mo cinematic/QTE - use
// PortCustomSFXPlayFixed instead for a voice line played across one of
// those (e.g. the Super Strike matrix-cam), which should still audibly
// track that slow-mo.
int PortCustomSFXPlay(const char* name);

// Same as PortCustomSFXPlay, but tracks the sim's slow-motion time scale
// (down to half speed, never slower - see kVoiceLineMinScale in
// custom_sfx.cpp) instead of ignoring it. Use this only at call sites that
// are themselves part of a slow-mo cinematic/QTE (e.g. the Super Strike
// windup/kick and the Super Strike matrix-cam) - everything else should use
// PortCustomSFXPlay, which is immune to slow-mo entirely.
int PortCustomSFXPlayFixed(const char* name);

// Adds any currently-playing custom clips into `pcm` (interleaved, signed
// 16-bit, stereo, `frames` sample pairs), with saturation. Called once per
// audio buffer from the platform audio backend, after MusyX has rendered it.
void PortCustomSFXMix(short* pcm, unsigned int frames);

#ifdef __cplusplus
}
#endif

#endif // PORT_CUSTOM_SFX_H
