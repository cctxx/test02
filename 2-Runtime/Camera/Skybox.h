#ifndef SKYBOX_H
#define SKYBOX_H

#include "Runtime/GameCode/Behaviour.h"

namespace Unity { class Material; }
class Camera;

class Skybox : public Behaviour {
public:
	REGISTER_DERIVED_CLASS (Skybox, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Skybox) 

	Skybox (MemLabelId label, ObjectCreationMode mode);
	static void RenderSkybox (Material* material, const Camera& camera);

	void SetMaterial (Material* material);
	Material* GetMaterial ()const;
		
	virtual void AddToManager ();
	virtual void RemoveFromManager ();

private:
  	PPtr<Material> m_CustomSkybox;
};

#endif
