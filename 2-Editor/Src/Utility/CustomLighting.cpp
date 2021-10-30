#include "UnityPrefix.h"
#include "CustomLighting.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Camera/LightManager.h"

using namespace std;

static CustomLighting *s_CustomLighting = NULL;

CustomLighting::CustomLighting ()
{
	m_CustomLight = false;
}

void CustomLighting::Initialize ()
{
	s_CustomLighting = new CustomLighting ();
	RegisterAddComponentCallback (&AddComponentCallback);
}

CustomLighting &CustomLighting::Get ()
{
	AssertIf (s_CustomLighting == NULL);
	return *s_CustomLighting;
}

void CustomLighting::DisableLights (std::vector<PPtr<Light> > *backup)
{
	if (backup)
		backup->clear();

	LightManager::Lights& lights = GetLightManager().GetAllLights();

	if (!lights.empty ())
	{
		std::vector<PPtr<Light> > *tmp;
		if (backup) {
			backup->clear();
			tmp = backup;
		} else 
			tmp = new std::vector<PPtr<Light> > ();
		
		for (LightManager::Lights::iterator i = lights.begin (); i != lights.end(); i++)
		{
			PPtr<Light> l = &*i;
			tmp->push_back (l);
		}
		// actually remove them from the light manager
		for (std::vector <PPtr <Light> >::iterator i = tmp->begin(); i != tmp->end(); i++)
		{
			Light* cur = *i;
			if (cur)
				cur->RemoveFromManager ();
		}
		
		if (!backup)
			delete tmp;
	}
}
	
void CustomLighting::RemoveCustomLighting ()
{
	if (!m_CustomLight)
		return;

	DisableLights (NULL);
	GetRenderSettings ().SetAmbientLightNoDirty (m_OldAmbient);

	for (std::vector <PPtr < Light> >::iterator i = m_BackupLights.begin(); i != m_BackupLights.end(); i++)
	{
		Light* cur = *i;
		if (cur)
			cur->AddToManager ();
	}

	m_BackupLights.clear ();			
	m_CustomLight = false;
}

void CustomLighting::SetCustomLighting (MonoArray *lights, const ColorRGBAf &ambientColor) {
	if (m_CustomLight) 
		 RemoveCustomLighting ();
	else 
		m_OldAmbient = GetRenderSettings ().GetAmbientLight();

	DisableLights (&m_BackupLights);
	
	// Go over the new lights...			
	for (int i = 0; i < mono_array_length (lights); i++) {
		Light *light = ScriptingObjectToObject<Light> (GetMonoArrayElement<MonoObject*> (lights, i));
		light->AddToManager ();
	}
	GetRenderSettings ().SetAmbientLightNoDirty (ambientColor);
	m_CustomLight = true;
}

bool CustomLighting::CalculateShouldEnableLights ()
{
	std::vector<Light*> lights;
	Object::FindObjectsOfType (&lights);
	for (std::vector<Light*>::iterator i=lights.begin ();i != lights.end ();i++)
	{
		Light& light = (**i);
		if (!light.IsPersistent () && !light.TestHideFlag (Object::kDontSave))
			return true;
	}
	return false;
}

void CustomLighting::AddComponentCallback (Unity::Component& com)
{
	if (!com.TestHideFlag(Object::kDontSave) && !com.IsPersistent () && com.IsDerivedFrom (ClassID (Light)) && !IsWorldPlaying ())
	{
		CallStaticMonoMethod("SceneView", "OnForceEnableLights");
	}
}
