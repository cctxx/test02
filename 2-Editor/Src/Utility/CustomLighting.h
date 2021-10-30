#ifndef CUSTOMLIGHTING_H
#define CUSTOMLIGHTING_H

#include <vector>
#include "Runtime/Math/Color.h"
#include "Runtime/Camera/Light.h"
struct MonoArray;

class CustomLighting {
public:
	void RemoveCustomLighting ();
	void SetCustomLighting (MonoArray* lights, const ColorRGBAf &ambientColor);

	bool CalculateShouldEnableLights ();
	static void Initialize ();
	
	static CustomLighting &Get ();

private:
	static void AddComponentCallback (Unity::Component& com);
	
	CustomLighting ();

	std::vector <PPtr < Light> > m_BackupLights;
	bool m_CustomLight;
	ColorRGBAf m_OldAmbient;

	void DisableLights (std::vector<PPtr<Light> > *backup);
};

#endif
