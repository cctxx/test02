#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include <string>
#include <map>
#include "Runtime/Shaders/NameToObjectMap.h"
#include "Runtime/Modules/ExportModules.h"

using std::string;

class Shader;
typedef std::vector< PPtr<Shader> > ShaderPtrVector;


class EXPORT_COREMODULE ScriptMapper : public GlobalGameManager
{
public:
	typedef std::multimap<UnityStr, PPtr<Shader> > StringToShader;
	REGISTER_DERIVED_CLASS (ScriptMapper, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (ScriptMapper)
	
	typedef NameToObjectMap<Shader, std::map<PPtr<Shader>, UnityStr>, std::multimap<UnityStr, PPtr<Shader> > > Shaders;
	
	ScriptMapper (MemLabelId label, ObjectCreationMode mode);
	// virtual ~ScriptMapper (); declared-by-macro

	// Add a shader to the system.
	void AddShader (Shader& ptr);
	void AddBuiltinShader (const std::string& className, PPtr<Shader> ptr);

	// Get a shader by name.
	// If the shader could not be loaded (e.g. because it was deleted), it is removed from the master list.
	Shader* FindShader (const string &name);

	bool ShouldIgnoreInGarbageDependencyTracking ();
	
	#if UNITY_EDITOR
	
	Shader *GetDefaultShader() { return FindShader( "Diffuse" ); }
	
	const Shaders& GetShaders () { return m_Shaders; }
	void SetShaders (const Shaders& shaders) { m_Shaders = shaders; }
	
	#endif // UNITY_EDITOR
	
private:
	Shaders	m_Shaders;
};

ScriptMapper& GetScriptMapper ();
ScriptMapper* GetScriptMapperPtr ();


Shader* FindShaderLabShader (const std::string& name, ShaderPtrVector& shaderLookup, bool useLookup);
