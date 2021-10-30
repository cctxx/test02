#ifndef NAVMESH_LAYERS_H
#define NAVMESH_LAYERS_H

#include "Runtime/BaseClasses/GameManager.h"




class NavMeshLayers : public GlobalGameManager
{
public:
	struct NavMeshLayerData
	{
		DECLARE_SERIALIZE (NavMeshLayerData)
		enum
		{
			kEditNone = 0,
			kEditName = 1,
			kEditCost = 2
		};

		UnityStr name;
		float cost;
		int editType;
	};

	enum BuiltinNavMeshLayers
	{
		kDefaultLayer = 0,
		kNotWalkable = 1,
		kJumpLayer = 2
	};

	NavMeshLayers (MemLabelId& label, ObjectCreationMode mode);
	// ~NavMeshLayers (); declared-by-macro

	REGISTER_DERIVED_CLASS (NavMeshLayers, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (NavMeshLayers)

	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void CheckConsistency ();

	void SetLayerCost (unsigned int index, float cost);
	float GetLayerCost (unsigned int index) const;
	int GetNavMeshLayerFromName (const UnityStr& layerName) const;
	std::vector<std::string> NavMeshLayerNames () const;

	enum
	{
		kBuiltinLayerCount = 3,
		kLayerCount = 32
	};

	static const char* s_WarningCostLessThanOne;
private:

	NavMeshLayerData m_Layers[kLayerCount];
};

NavMeshLayers& GetNavMeshLayers ();

#endif
