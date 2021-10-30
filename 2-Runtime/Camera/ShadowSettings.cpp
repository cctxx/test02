#include "UnityPrefix.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "ShadowSettings.h"
#include "Lighting.h"

void ShadowSettings::Reset()
{
	m_Type = kShadowNone;
	m_Resolution = -1; // auto
	m_Strength = 1.0f;
	m_Bias = 0.05f; // 5 cm
	m_Softness = 4.0f;
	m_SoftnessFade = 1.0f;
}
