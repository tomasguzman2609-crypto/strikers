#include "NL/globalpad.h"
#include "NL/nlMath.h"
#include "port/mod_challenge.h"

#include <cstdio>
#include <cstdlib>

cGlobalPad* cPadManager::m_aPads[PAD_MAX_CONTROLLERS];
s32* cPadManager::m_pRemapArray = nullptr;
float cPadManager::m_DeltaT = 0.0f;

/**
 * Offset/Address/Size: 0xDC | 0x801F01C0 | size: 0x13C
 */
void cGlobalPad::Update(float deltaTime)
{
    float x;
    float y;

    x = this->AnalogLeftX();
    y = this->AnalogLeftY();

    // PORT: challenge roulette - "inverted controls" flips Player 1's (pad 0's) movement stick on
    // both axes, upstream of everything that reads direction/magnitude from it (AI pads never
    // go through here at all, and pad 1+ keep their own untouched x/y).
    if (m_padIndex == 0 && PortModChallengeGetActive() == PORT_CHALLENGE_INVERTED_CONTROLS)
    {
        // PORT DEBUG: set STRIKERS_DEBUG_INVERT=1 to confirm this branch is actually
        // reached at runtime, and with what raw stick values. Remove once confirmed.
        if (getenv("STRIKERS_DEBUG_INVERT") != NULL)
        {
            static int s_count = 0;
            if (s_count < 120) // ~2s at 60fps, so it doesn't spam forever
            {
                s_count++;
                fprintf(stderr, "[invert] pad0 raw x=%.3f y=%.3f -> x=%.3f y=%.3f\n", x, y, -x, -y);
            }
        }
        x = -x;
        y = -y;
    }

    m_polarAnalogLeft.r = nlSqrt((x * x) + (y * y), 1);

    if ((0.f != x) || (0.f != y))
    {
        m_polarAnalogLeft.a = (u16)(10430.378f * nlATan2f(y, x));
    }

    x = this->AnalogRightX();
    y = this->AnalogRightY();
    m_polarAnalogRight.r = nlSqrt((x * x) + (y * y), 1);
    if ((0.f != x) || (0.f != y))
    {
        m_polarAnalogRight.a = (u16)(10430.378f * nlATan2f(y, x));
    }
}

/**
 * Offset/Address/Size: 0xB0 | 0x801F0194 | size: 0x2C
 */
bool cGlobalPad::JustPressed(int button, bool remap)
{
    return this->PlatJustPressed(button, remap);
}

/**
 * Offset/Address/Size: 0x84 | 0x801F0168 | size: 0x2C
 */
bool cGlobalPad::JustReleased(int button, bool remap)
{
    return this->PlatJustReleased(button, remap);
}

/**
 * Offset/Address/Size: 0x70 | 0x801F0154 | size: 0x14
 */
cGlobalPad* cPadManager::GetPad(int idx)
{
    return m_aPads[idx];
}

/**
 * Offset/Address/Size: 0x0 | 0x801F00E4 | size: 0x70
 */
void cPadManager::Update(float deltaTime)
{
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++)
    {
        m_aPads[i]->Update(deltaTime);
    }
    m_DeltaT = deltaTime;
}
