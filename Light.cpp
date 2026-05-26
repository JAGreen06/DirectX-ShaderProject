#include "Light.h"
#include "Model.h"

Light::Light()
{
    mColour = { 0.0f, 0.0f, 0.0f };
    mStrength = 1.0f;
    mToggle = true;
}

void Light::UpdateToggle(float& frametime)
{
    mOnOffTime += frametime;
    if (mOnOffTime >= 3.0f)
    {
        mToggle = !mToggle;
        mOnOffTime = 0.0f;

    }
    if (mToggle)
    {
        mStrength = 10;
    }
    else
    {
        mStrength = 0;
    }   
}

void Light::UpdateColour(float& frametime)
{
    mColourTime += frametime;
    mRed = (sin(mColourTime) + 1.0f) / 2.0f;
    mGreen = (sin(mColourTime * 0.5f) + 1.0f) / 2.0f;
    mBlue = (sin(mColourTime * 1.5f) + 1.0f) / 2.0f;

    mColour = { mRed, mGreen, mBlue };
}
