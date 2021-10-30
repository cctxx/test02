#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/dynamic_bitset.h"


class EditorBuildSettings : public Object
{
public:	
	struct Scene
	{
		bool enabled;
		UnityStr path;
		DECLARE_SERIALIZE(Scene)

		friend bool operator == (const Scene& lhs, const Scene& rhs) { return lhs.enabled == rhs.enabled && lhs.path == rhs.path; }
	};
	
	REGISTER_DERIVED_CLASS (EditorBuildSettings, Object)
	DECLARE_OBJECT_SERIALIZE(EditorBuildSettings)
	
	EditorBuildSettings (MemLabelId label, ObjectCreationMode mode);
	// ~EditorBuildSettings (); declared-by-macro

	typedef std::vector<Scene>				Scenes;
	Scenes									m_Scenes;
	
	const Scenes& GetScenes () const { return m_Scenes; }
	void SetScenes (const Scenes& scenes);
		
	void ReadOldBuildSettings();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
private:
	bool m_ReadOldBuildSettings;
};

EditorBuildSettings& GetEditorBuildSettings();
void SetEditorBuildSettings(EditorBuildSettings* settings);
