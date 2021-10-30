#ifndef MATERIAL_H
#define MATERIAL_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Color.h"
#include "UnityPropertySheet.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Shaders/ShaderKeywords.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Modules/ExportModules.h"

namespace ShaderLab
{
	class PropertySheet;
	struct FastPropertyName;
	class ShaderState;
	class Pass;
}

class ChannelAssigns;
class Shader;
class Texture;
class Matrix4x4f;
class GfxDisplayList;



namespace Unity
{

class EXPORT_COREMODULE Material : public NamedObject 
{
  public:
	REGISTER_DERIVED_CLASS (Material, NamedObject)
	DECLARE_OBJECT_SERIALIZE (Material)
		
	Material (MemLabelId label, ObjectCreationMode mode);
	// ~Material (); declared-by-macro
	virtual void Reset();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	/// Get how many passes this material requires to render
	int GetPassCount ();

	// Set the pass to render next time.
	// Calling this function sets up the shader.
	// Returns vertex channels used; NULL if this pass should not be rendered.
	const ChannelAssigns* SetPass (int passNo, int subshaderIndex = 0, bool allowRecording = true);
	
	// Set the pass to render next time.
	// Can supply a different shader from the one in material, or just cached PPtr deref.
	// Also indicates which subshader to use.
	// Calling this function sets up the shader.
	// Returns vertex channels used; NULL if this pass should not be rendered.
	const ChannelAssigns* SetPassWithShader( int passNo, Shader* shader, int subshaderIndex );
	
	const ChannelAssigns* SetShadowCasterPass(int subshaderIndex);
	const ChannelAssigns* SetShadowCasterPassWithShader(Shader* shader, int subshaderIndex);
	const ChannelAssigns* SetShadowCollectorPass(int subshaderIndex);
	const ChannelAssigns* SetShadowCollectorPassWithShader(Shader* shader, int subshaderIndex);

	int GetActualRenderQueue() const;
	int GetCustomRenderQueue() const { return m_CustomRenderQueue; }
	void SetCustomRenderQueue (int q) { if (m_CustomRenderQueue != q) { m_CustomRenderQueue = q; SetDirty(); } }
	
	inline const ShaderLab::PropertySheet& GetProperties ()
	{
		EnsurePropertiesExist ();
		return *m_Properties;
	}

	inline void EnsurePropertiesExist ()
	{
//		#if UNITY_EDITOR || WEBPLUG
		#if 1
			// Don't preload in the editor because it can easily run out of memory and increase build times.
			// In the webplayer we also have some hacks where we call ClearProperties that must be fixed before we can remove the WEBPLUG condition
			if( !m_Properties )
				BuildProperties();
		#else
			// Enforce that everything is preloaded in the player, reduces hiccups at runtime.
			Assert(m_Properties != NULL);
		#endif
	}
	
	inline ShaderLab::PropertySheet& GetWritableProperties ()
	{
		EnsurePropertiesExist ();
		SetPropertiesDirty();
		return *m_Properties;
	}

	void SetPropertiesDirty ()
	{
		m_PropertiesDirty = true;
	}

	/// Clears the cached properties.
	/// This is used in the web player to avoid caching issues with builtin resources when reloading the player.
	void ClearProperties ();

	// Sets the materials properties
	void CopyPropertiesFromMaterial(Material& other);
	
	// Get/Set a color value in the material
	void SetColor (ShaderLab::FastPropertyName name, const ColorRGBAf &col);
	void SetColorIndexed (ShaderLab::FastPropertyName name, int indexed, float value);
	ColorRGBAf GetColor (ShaderLab::FastPropertyName name);
	
	// Get/Set a float value in the material
	void SetFloat (ShaderLab::FastPropertyName name, float val);
	float GetFloat (ShaderLab::FastPropertyName name);
	
	// Get/Set a matrix value in the material
	void SetMatrix (	ShaderLab::FastPropertyName name, const Matrix4x4f &val);
	const Matrix4x4f &GetMatrix (ShaderLab::FastPropertyName name);

	// Get/Set a texture value in the material
	void SetTexture (ShaderLab::FastPropertyName name, Texture *val);
	Texture *GetTexture (ShaderLab::FastPropertyName name);
	
	bool HasProperty (ShaderLab::FastPropertyName name);
	
	// Texture placement
	void SetTextureOffset( ShaderLab::FastPropertyName name, const Vector2f& offset );
	void SetTextureScale( ShaderLab::FastPropertyName name, const Vector2f& scale );

	Vector2f GetTextureOffset( ShaderLab::FastPropertyName name );
	Vector2f GetTextureScale( ShaderLab::FastPropertyName name );
	
	void SetTextureScaleAndOffsetIndexed (ShaderLab::FastPropertyName name, int indexed, float value);

	void SetComputeBuffer (ShaderLab::FastPropertyName name, ComputeBufferID val);

	
	// Get the default material
	static Material *GetDefault ();
	static Material *GetDefaultDiffuseMaterial ();
	
	static Material *CreateMaterial (const char *shaderStr, int hideFlags, bool scriptingObjectIsBeingCreated = false);
	static Material *CreateMaterial (Shader& shader, int hideFlags, bool scriptingObjectIsBeingCreated = false);
	static Material *CreateMaterial (const Material& material, int hideFlags, bool scriptingObjectIsBeingCreated = false);
	
	// Set the shader that drives the material
	void SetShader (Shader *s);

	const Shader *GetShader() const;
	Shader *GetShader();
	PPtr<Shader> GetShaderPPtr() const;
	
	std::string GetTag( const string& tag, bool currentSubShaderOnly, const string& defaultValue ) const;

	typedef std::vector<UnityStr> ShaderKeywordsT;
	const ShaderKeywordsT& GetShaderKeywords () const		{ return m_ShaderKeywords; }
	void SetShaderKeywords (const ShaderKeywordsT& keywords);
	ShaderKeywordSet GetShaderKeywordSet () const			{ return m_ShaderKeywordSet; }
	void EnableKeyword (const std::string& key);
	void DisableKeyword (const std::string& key);
	void ApplyMaterialPropertyDrawers();

	#if UNITY_EDITOR
	bool ActuallyHasTextureProperty (ShaderLab::FastPropertyName name) const;

	void ResetDefaultTextures (bool overrideSetTextures);
	#endif

	inline UnityPropertySheet& GetSavedProperties ()				{ return m_SavedProperties; }
	inline const UnityPropertySheet& GetSavedProperties () const	{ return m_SavedProperties; }	

	// If m_Owner equals renderer returns this
	// Otherwise creates a copy and sets m_Owner to renderer.
	// Used by Animation system and scripting to animate material properties of a single renderer.	
	static Material& GetInstantiatedMaterial (Material* material, Object& renderer, bool allowInEditMode);
	PPtr<Object> GetOwner ()								{ return m_Owner; }
	
	virtual int GetRuntimeMemorySize () const;

	void InvalidateDisplayLists ();
	
	inline UInt32	GetShadowCasterHash()					{ EnsurePropertiesExist (); return m_ShadowCasterHash; }
	inline UInt32	GetShadowCollectorHash()				{ EnsurePropertiesExist (); return m_ShadowCollectorHash; }

private:
	/// Build the ShaderLab property sheet (m_Properties) from the saved properties.
	/// This will correctly handle default properties supplied by the shader...
	void BuildProperties ();
	void BuildShaderKeywordSet ();

	void UpdateHashesOnPropertyChange (ShaderLab::FastPropertyName name);
	void UpdateHashes ();

	struct CachedShaderPass
	{
		CachedShaderPass() : displayList(NULL), channelAssigns(NULL), shaderKeywords(0), globalFogMode(kFogDisabled) {}

		GfxDisplayList* displayList;
		const ChannelAssigns* channelAssigns;
		UInt64 shaderKeywords;
		FogMode globalFogMode;
	};

	struct CachedSubShader
	{
		dynamic_array<CachedShaderPass> passes;
		CachedShaderPass shadowCasterPass;
		CachedShaderPass shadowCollectorPass;
	};

	PPtr<Shader>				m_Shader;
	ShaderLab::PropertySheet*	m_Properties;
	bool						m_PropertiesDirty;
	std::vector<CachedSubShader> m_CachedSubShaders;
	int							m_CustomRenderQueue; // -1 if should use shader's
	PPtr<Object>				m_Owner;
	UnityPropertySheet 			m_SavedProperties;

	ListNode<Material>			m_ShaderUserNode;
	ShaderKeywordsT				m_ShaderKeywords;
	ShaderKeywordSet			m_ShaderKeywordSet;
	UInt32						m_ShadowCollectorHash;
	UInt32						m_ShadowCasterHash;
	UInt32						m_StateKeyHash;
};

}

using namespace Unity;

#endif
