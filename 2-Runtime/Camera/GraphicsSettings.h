#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Shaders/Shader.h"
#include <vector>


class GraphicsSettings : public GlobalGameManager
{
public:
	typedef UNITY_VECTOR(kMemRenderer, PPtr<Shader>) ShaderArray;

	REGISTER_DERIVED_CLASS (GraphicsSettings, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (GraphicsSettings)

	GraphicsSettings (MemLabelId label, ObjectCreationMode mode);
	// ~GraphicsSettings (); declared-by-macro

	static void InitializeClass ();
	static void CleanupClass ();

	virtual void Reset ();

	#if UNITY_EDITOR
	bool DoesNeedToInitializeDefaultShaders() const { return m_NeedToInitializeDefaultShaders; }
	#endif
	void SetDefaultAlwaysIncludedShaders();

	/// Return true if the given shader is in the list of shaders
	/// that should always be included in builds.
	bool IsAlwaysIncludedShader (PPtr<Shader> shader) const;

#if UNITY_EDITOR

	const ShaderArray& GetAlwaysIncludedShaders () const { return m_AlwaysIncludedShaders; }

	/// Add a shader to the list of shaders that are aways included in builds.
	/// NOTE: Does not check whether the shader is already on the list.
	void AddAlwaysIncludedShader (PPtr<Shader> shader);

#endif

private:
	ShaderArray m_AlwaysIncludedShaders;
	bool m_NeedToInitializeDefaultShaders;
};

GraphicsSettings& GetGraphicsSettings();
