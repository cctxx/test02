#ifndef LIGHTING_H
#define LIGHTING_H

// Light type
enum LightType {
	kLightSpot,
	kLightDirectional,
	kLightPoint,
	kLightArea,
	kLightTypeCount // keep this last
};

// Pixel lighting mode (keyword to use)
enum LightKeywordMode {
	kLightKeywordSpot,
	kLightKeywordDirectional,
	kLightKeywordDirectionalCookie,
	kLightKeywordPoint,
	kLightKeywordPointCookie,
	kLightKeywordCount // keep this last
};

enum ShadowType {
	kShadowNone = 0,
	kShadowHard,
	kShadowSoft,
};

inline bool IsSoftShadow(ShadowType shadowType)
{
	return (shadowType == kShadowSoft);
};

#endif
