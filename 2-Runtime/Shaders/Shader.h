#ifndef SHADER_H
#define SHADER_H

#include "Runtime/Scripting/TextAsset.h"
#include <string>
#include "Runtime/Math/Color.h"
#include "Runtime/Utilities/LinkedList.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Runtime/Graphics/Texture.h"

using std::string;

namespace ShaderLab
{
	class IntShader;
	class PropertySheet;
	struct ParserProperty;
	class Pass;
	struct ParserShader;
}
namespace Unity { class Material; }
class Shader;
class ChannelAssigns;
typedef std::vector< PPtr<Shader> > ShaderPtrVector;


// A Wrapper for a ShaderLab shader.
// This class handles all the Unity-specific interfacing into the shader.
class Shader : public TextAsset {
	public:
	REGISTER_DERIVED_CLASS (Shader, TextAsset)
	DECLARE_OBJECT_SERIALIZE (Shader)

	Shader (MemLabelId label, ObjectCreationMode mode);
	// ~Shader (); declared-by-macro

	virtual bool MainThreadCleanup ();
	
	// Set the next pass to render
	const ChannelAssigns* SetPass (int subshaderIndex, int passNo, UInt32 stateKey, const ShaderLab::PropertySheet* props);

	bool CanPassBeRecorded(int subshaderIndex, int passNo) const;

	ShaderLab::Pass* GetShadowCasterPassToUse(int subshaderIndex);
	ShaderLab::Pass* GetShadowCollectorPassToUse(int subshaderIndex);

	bool AnySubshaderHasShadowCasterPass() const { return m_ShadowCollectorPass != NULL; }
	bool HasShadowCollectorPass() const { return m_ShadowCollectorPass != NULL; }
	int GetActiveSubShaderIndex () const;

	// Set the shader string
	virtual bool SetScript (const ScriptString& script);

	// Did this shader compile & work, or are we using the default
	bool IsSupported () const;

	// Get the default Shader
	static Shader* GetDefault ();
	static Shader* GetScreenClearShader ();
	static void LoadDefaultShaders ();

	static void UnloadDefaultShaderLabShader();
	static void LoadDefaultShaderLabShader();
	void ResetInternalPointersToNull()
	{
		m_Shader = NULL;
		m_ShadowCasterPass = NULL;
		m_ShadowCollectorPass = NULL;
	}

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void AwakeFromLoadThreaded ();

	// Make a new property sheet.
	ShaderLab::PropertySheet *MakeProperties () const;


	#if UNITY_EDITOR
	int GetPropertyCount () const;
	const ShaderLab::ParserProperty *GetPropertyInfo (int propertyNo) const;
	#endif

	// from TextAsset, returns shader name
	virtual const UnityStr& GetScriptClassName () const { return m_ShaderName; }
	// from Object, returns shader name
	virtual char const* GetName () const;
	// GetName() is overriden, use this to return name from named object
	// A bit of a hack, see case 364626
	char const* GetNamedObjectName() const { return m_Name.empty() ? GetName() : m_Name.c_str (); }

	const ShaderLab::IntShader *GetShaderLabShader() const { return m_Shader; }
	ShaderLab::IntShader *GetShaderLabShader() { return m_Shader; }

	// reparses and reloads all shaders
	static void ReloadAllShaders();
	// those two do the same as ReloadAllShaders. Use if there is something to be done in between.
	static void DeleteAllShaders( std::vector<SInt32>& outShaderObjects );
	static void RecreateAllShaders( const std::vector<SInt32>& shaderObjects );

	static void SetGLobalMaximumShaderLOD( int lod );
	static int GetGlobalMaximumShaderLOD();

	void SetMaximumShaderLOD( int lod );
	int GetMaximumShaderLOD() const;

	// Get the index of a subshader with a given type tag.
	int GetSubShaderWithTagValue (int tagNameID, int tagValueID) const;

	void AddMaterialUser( ListNode<Unity::Material>& node );

	bool GetNeedsParsing() const { return m_NeedsParsing; }

	void ParseAndPostLoad( const char* str, size_t strLength ) { Parse(str,strLength); PostLoad(); }

	bool IsDependentOn (PPtr<Shader> shader) const;
	Shader* GetDependency (const std::string& name);
	const ShaderPtrVector& GetDependencies () const { return m_Dependencies; }

	ShaderLab::Pass*	GetShadowCasterPass() { return	m_ShadowCasterPass; }
	ShaderLab::Pass*	GetShadowCollectorPass() { return	m_ShadowCollectorPass; }

	#if UNITY_EDITOR
	const ShaderLab::ParserShader* GetParsedForm() const { return m_ParsedForm; }
	const ShaderErrors& GetErrors() const { return m_Errors; }
	ShaderErrors& GetErrors() { return m_Errors; }
	UInt32 CalculateUsedVertexComponents (bool lightmapped);

	typedef std::map<UnityStr, PPtr<Texture> > DefaultTexturesMap;
	const DefaultTexturesMap& GetDefaultTextures () const { return m_DefaultTextures; }
	void SetDefaultTextures (const DefaultTexturesMap& textures) { m_DefaultTextures = textures; }

	bool HasClip() const;
	char const* GetCustomEditorName() const;
	#endif

private:
	// Function to extract renderqueue & channels from a shader.
	void PostLoad ();
	// Parse a string into the shader
	void Parse (const char *str, size_t strLength);

	ShaderLab::IntShader*	m_Shader;				// the actual shader
	ShaderLab::Pass*		m_ShadowCasterPass;		// shadow caster pass, if any
	ShaderLab::Pass*		m_ShadowCollectorPass;	// shadow collector pass, if any

	// List of Materials that use this shader
	typedef List< ListNode<Unity::Material> > MaterialList;
	MaterialList m_Users;


	// Shaders that are not supported get assigned a "default shader". However, we really
	// want to remember it's original name (so we can build players with proper ScriptMapper info,
	// and display the correct name in shader popups, etc.).
	//
	// So remember the actual name here, and not in ShaderLab
	// shader instance (since single "default" shaderlab instance is shared by all unsupported shaders).
	UnityStr m_ShaderName;

	ShaderPtrVector	m_Dependencies; // shader pointers used by Dependencies / UsePasses / Fallbacks from this shader
	bool		m_ShaderIsBaked;

	bool		m_NeedsParsing;

	#if UNITY_EDITOR
	ShaderErrors m_Errors;
	ShaderLab::ParserShader* m_ParsedForm;

	DefaultTexturesMap m_DefaultTextures;
	#endif
};

void CleanupShaders ();

#endif
