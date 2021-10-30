#pragma once

#include "Renderable.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Color.h"
#include "Runtime/GameCode/Behaviour.h"

class Halo : public Behaviour {
public:
	REGISTER_DERIVED_CLASS   (Halo, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Halo)
	Halo (MemLabelId label, ObjectCreationMode mode);
	static void InitializeClass ();
	static void CleanupClass ();

	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void Reset ();
	void TransformChanged ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

private:
	ColorRGBA32 m_Color;
	float m_Size;
	int m_Handle;
};


class HaloManager : public LevelGameManager, public Renderable {
public:
 	REGISTER_DERIVED_CLASS (HaloManager, LevelGameManager)
	
	int AddHalo ();
	void UpdateHalo (int h, Vector3f position,ColorRGBA32 color,float size, UInt32 layers);
	void DeleteHalo (int h);
	
	// Renderable
	virtual void RenderRenderable (const CullResults& CullResults);

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

private:
	HaloManager (MemLabelId label, ObjectCreationMode mode);
	// ~HaloManager (); declared-by-macro
	struct Halo {
		Vector3f position;
		ColorRGBA32 color;
		float size;
		int handle;
		UInt32 layers;
		Halo (int hdl);
		Halo (const Vector3f &pos, const ColorRGBA32 &col, float s, int h, UInt32 _layers);
	};
	
	typedef std::vector<Halo> HaloList;
	HaloList m_Halos;
};

HaloManager& GetHaloManager();


// DEPRECATED
class HaloLayer : public Behaviour {
public:
	REGISTER_DERIVED_CLASS (HaloLayer, Behaviour)
	HaloLayer (MemLabelId label, ObjectCreationMode mode);
	virtual void AddToManager () {};
	virtual void RemoveFromManager () {};
};
