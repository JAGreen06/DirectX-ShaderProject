#ifndef _LIGHT_H_INCLUDED_
#define _LIGHT_H_INCLUDED_

#include "Math/CVector3.h"

class Model;

class Light
{
private:
	CVector3 mColour;
	float mStrength;
	bool mToggle;	
	Model* mModel;

	float mColourTime;
	float mOnOffTime;

	float mRed, mGreen, mBlue;

public:

	Light();

	CVector3 GetColour() { return mColour; };
	float GetStrength() { return mStrength; };
	Model* GetModel() { return mModel; };

	void SetColour(CVector3 colour) { mColour = colour; };
	void SetStrength(float strength) { mStrength = strength; };
	void SetModel(Model* model) { mModel = model; };


	void UpdateToggle(float& frametime);
	void UpdateColour(float& frametime);
};

#endif //_LIGHT_H_INCLUDED_