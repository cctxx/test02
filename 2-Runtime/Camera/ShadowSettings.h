#ifndef SHADOW_SETTINGS_H
#define SHADOW_SETTINGS_H

#include "Runtime/Serialize/SerializeUtility.h"

struct ShadowSettings
{
	DECLARE_SERIALIZE_NO_PPTR (ShadowSettings)
		
	int		m_Type;				///< enum { No Shadows, Hard Shadows, Soft Shadows } Shadow cast options
	// -1 is auto; the rest must match the values in Quality Settings!
	int		m_Resolution;		///< enum { Use Quality Settings = -1, Low Resolution = 0, Medium Resolution = 1, High Resolution = 2, Very High Resolution = 3 } Shadow resolution
	float	m_Strength;			///< Shadow intensity range {0.0, 1.0}
	float	m_Bias;				///< Bias for shadows range {0.0, 10.0}
	float	m_Softness;			///< Shadow softness range {1.0, 8.0}
	float	m_SoftnessFade;		///< Shadow softness fadeout range {0.1, 5.0}
	
	ShadowSettings () { Reset (); }
	void Reset();	
};

template<class TransferFunc>
void ShadowSettings::Transfer (TransferFunc& transfer)
{
	TRANSFER_SIMPLE(m_Type);
	TRANSFER (m_Resolution);
	TRANSFER (m_Strength);
	TRANSFER (m_Bias);
	TRANSFER (m_Softness);
	TRANSFER (m_SoftnessFade);
}

#endif
