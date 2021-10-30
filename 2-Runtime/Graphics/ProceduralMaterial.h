#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Utilities/Hash128.h"
#include "ProceduralTexture.h"
#include "SubstanceInput.h"
#include "External/Allegorithmic/builds/Engines/include/substance/handle.h"
#include "External/Allegorithmic/builds/Engines/include/substance/linker/linker.h"

class SubstanceArchive;
class Texture2D;
class Image;

#if UNITY_EDITOR
bool IsSubstanceSupportedOnPlatform(BuildTargetPlatform platform);
#endif

bool IsSubstanceSupported();															// return true if Substance is supported on current platform
TextureFormat GetSubstanceTextureFormat(SubstanceOutputFormat outputFormat, bool requireCompressed=false);	// return the required Substance texture format
SubstanceEngineIDEnum GetSubstanceEngineID();																// return the required Substance engine ID

// Global Substance processor usage
enum ProceduralProcessorUsage
{
	ProceduralProcessorUsage_Unsupported = 0,	// Substance isn't supported
	ProceduralProcessorUsage_One,				// uses only one CPU
	ProceduralProcessorUsage_Half,				// uses half of the CPU count (and not the first one if applicable)
	ProceduralProcessorUsage_All				// uses all the CPU
};

// Memory budget
enum ProceduralCacheSize
{
	ProceduralCacheSize_Tiny = 0,	// 128 Mb
	ProceduralCacheSize_Medium,		// 256 Mb
	ProceduralCacheSize_Heavy,		// 512 Mb
	ProceduralCacheSize_NoLimit,	// unlimited
	ProceduralCacheSize_None		// 1 byte
};

size_t GetProceduralMemoryBudget(ProceduralCacheSize budget);

// Loading behavior
enum ProceduralLoadingBehavior
{
	ProceduralLoadingBehavior_None = 0,         // do nothing
	ProceduralLoadingBehavior_Generate,         // generate textures
	ProceduralLoadingBehavior_BakeAndKeep,      // use baked textures, the substance may be generated later on
	ProceduralLoadingBehavior_BakeAndDiscard,   // use baked textures, the substance data is removed from runtime
	ProceduralLoadingBehavior_Cache             // generate textures and cache it, then use it at runtime to speed-up the loading
};

/* A ProceduralMaterial is a dynamic material that use the Substance engine.
 */

class ProceduralMaterial : public Material
{
public:		// NESTED TYPES
	
	typedef std::vector<PPtr<ProceduralTexture> > Textures;	
	typedef std::vector<ProceduralTexture*> PingedTextures;	

public:		// METHODS

	REGISTER_DERIVED_CLASS( ProceduralMaterial, Material )
	DECLARE_OBJECT_SERIALIZE( ProceduralMaterial )
	
	ProceduralMaterial( MemLabelId label, ObjectCreationMode mode );

	void Clean();

	ProceduralMaterial* Clone();
private:
	void RebuildClone();

public:
#if UNITY_EDITOR
	// Creates the material (one time call only from the importer)
	void Init( SubstanceArchive& substancePackage, const UnityStr& prototypeName, const SubstanceInputs& inputs, const Textures& textures );
	SubstanceArchive* GetSubstancePackage() { return m_PingedPackage; }
	const char* GetSubstancePackageName();
#endif
	const Textures&	GetTextures() const { return m_Textures; }
	Textures& GetTextures() { return m_Textures; }
	PingedTextures& GetPingedTextures() { return m_PingedTextures; }
	SubstanceInputs& GetSubstanceInputs () { return m_Inputs; }
	const SubstanceInputs& GetSubstanceInputs () const { return m_Inputs; }
	const SubstanceArchive* GetSubstancePackage() const { return m_PingedPackage; }
	SubstanceHandle* GetSubstanceHandle();
    
	// Generation sizes accessors
	void SetSize(int width, int height);
	int GetWidth() const  { return m_Width; }
	int GetHeight() const  { return m_Height; }

	// Threaded loading, launch the generation if all data is available
	void AwakeFromLoadThreaded();
	
	// Used in the editor to generate at reimport time
	void AwakeFromLoad( AwakeFromLoadMode awakeMode );

	// Call the rebuild of all the textures and updates the SBS texture assets
	//	generationType, the type of requested texture generation
	//	forceTextureGeneration, a boolean to force textures generation even though they may be up to date from the engine's point of view
	void RebuildTextures();	

	// Rebuilds all texture immediately in a synchronous manner
	// When that function returns, all textures should have been generated
	void RebuildTexturesImmediately();

	// Call the rebuild of all the textures in all of the Procedural Materials
	static void ReloadAll (bool unload = true, bool load = true);
	
    // Awake dependent objects
	void AwakeDependencies(bool awakeThreaded);

	// Process rebuild of textures
#if ENABLE_SUBSTANCE
    static ProceduralMaterial* m_PackedSubstance;
	static void PackSubstances(std::vector<ProceduralMaterial*>& materials);
	bool ProcessTexturesThreaded(const std::map<ProceduralTexture*, SubstanceTexture>& textures);
#if !UNITY_EDITOR
	bool PreProcess(std::set<unsigned int>& cachedTextureIDs);
	void PostProcess(const std::map<ProceduralTexture*, SubstanceTexture>& textures, const std::set<unsigned int>& cachedTextureIDs);
#endif
#endif

	// Scriptable input accessors
	std::vector<std::string> GetSubstanceProperties() const;
	bool HasSubstanceProperty( const std::string& inputName ) const;
	bool GetSubstanceBoolean( const std::string& inputName ) const;
	void SetSubstanceBoolean( const std::string& inputName, bool value );
	float GetSubstanceFloat( const std::string& inputName ) const;
	void SetSubstanceFloat( const std::string& inputName, float value );
	Vector4f GetSubstanceVector( const std::string& inputName ) const;
	void SetSubstanceVector( const std::string& inputName, const Vector4f& value );
	ColorRGBAf GetSubstanceColor( const std::string& inputName ) const;
	void SetSubstanceColor( const std::string& inputName, const ColorRGBAf& value );
	int GetSubstanceEnum( const string& inputName );
	void SetSubstanceEnum( const string& inputName, int value );
	Texture2D* GetSubstanceTexture( const string& inputName ) const;
	void SetSubstanceTexture( const string& inputName, Texture2D* value );

#if ENABLE_SUBSTANCE
	// Called by the SubstanceSystem when he process a "SetInput" command
	void Callback_SetSubstanceInput( const string& inputName, SubstanceValue& inputValue );
#endif

	const SubstanceInput* FindSubstanceInput( const string& inputName ) const;
	SubstanceInput* FindSubstanceInput( const string& inputName );

	// Property caching
	bool IsSubstancePropertyCached( const string& inputName ) const;
	void CacheSubstanceProperty( const string& inputName, bool value );
	void ClearCache();

	// Memory budget
	void SetProceduralMemoryBudget(ProceduralCacheSize budget);
	ProceduralCacheSize GetProceduralMemoryBudget() const;
	void SetProceduralMemoryWorkBudget(ProceduralCacheSize budget);
	ProceduralCacheSize GetProceduralMemoryWorkBudget() const;
	void SetProceduralMemorySleepBudget(ProceduralCacheSize budget);
	ProceduralCacheSize GetProceduralMemorySleepBudget() const;

protected:
	
#if ENABLE_SUBSTANCE
	void ApplyInputs (bool& it_has_changed, bool asHint, std::set<unsigned int>& modifiedOutputsUID);
	void ApplyOutputs (bool& it_has_changed, bool asHint, std::set<unsigned int>& modifiedOutputsUID, const std::set<unsigned int>& cachedTextureIDs);
#endif

private:
	
	// Shared substance data
	struct SubstanceData
	{
	    UInt8* substanceData;                       // Platform dependent linked binary content
	    SubstanceHandle* substanceHandle;           // Substance engine handle, shared by instances
	    ProceduralCacheSize memoryWorkBudget;       // 'Work' cache size, shared by instances 
	    ProceduralCacheSize memorySleepBudget;      // 'Sleep' cache size, shared by instances 
	    int instanceCount;                          // Count of substances using the same handle
	};

	PPtr<SubstanceArchive> m_SubstancePackage;      // The parent SBS package from which we get generated
	SubstanceArchive* m_PingedPackage;              // The pinged package
	SubstanceData* m_SubstanceData;                 // Shared substance data
	UnityStr m_PrototypeName;                       // The name of the original graph in the package
	int m_Width;                                    // Width
	int m_Height;                                   // Height
	Textures m_Textures;                            // The list of persistent output textures for that material
	PingedTextures m_PingedTextures;                // The list of pinged output textures
	SubstanceInputs m_Inputs;                       // Substance inputs
	static Mutex m_InputMutex;                      // Input accessors mutex
	Hash128 m_Hash;                                 // Hash used for cache status checking
	
public:
	// Texture inputs
	struct TextureInput
	{
		Texture2D* texture;
		Image* image;
		SubstanceTextureInput* inputParameters;
		void* buffer;
	};
	std::vector<TextureInput> m_TextureInputs;
#if UNITY_EDITOR
	std::vector<TextureInput>& GetTextureInputs() { return m_TextureInputs; }
#endif

	void ApplyTextureInput (int substanceInputIndex, const SubstanceTextureInput& requiredTextureInput);
	
	// Animated substances
private:
	int m_AnimationUpdateRate;
	float m_AnimationTime;
public:
	void SetAnimationUpdateRate(int rate) { m_AnimationUpdateRate = rate; }
	int GetAnimationUpdateRate() const { return m_AnimationUpdateRate; }
	void UpdateAnimation(float time);
	
	// Flags
public:
	enum Flag
	{
		Flag_DeprecatedGenerateAtLoad = 1<<0,  // deprecated
		Flag_Animated                 = 1<<2,  // the material has animated textures
		Flag_AwakeClone               = 1<<3,  // the material is a clone which require to be awaken
		Flag_GenerateAll              = 1<<4,  // we force the generation of all outputs
		Flag_ConstSize                = 1<<5,  // the size and seed don't change at runtime
		Flag_ForceGenerate            = 1<<6,  // force the generation
		Flag_Clone                    = 1<<7,  // the material is a clone
		Flag_Import                   = 1<<8,  // the material is being imported
		Flag_Awake                    = 1<<9,  // the material is awakening
		Flag_Uncompressed             = 1<<10, // the import is forced uncompressed
		Flag_Broken                   = 1<<11, // some dependencies are lacking, the substance can't be generated
		Flag_Readable                 = 1<<12  // generated textures are readable, provided it's in RAW format
	};
	void EnableFlag(const Flag& flag, bool enabled=true) { if (enabled) m_Flags |= (unsigned int)flag; else m_Flags &= ~(unsigned int)flag; }
	bool IsFlagEnabled(const Flag& flag) const { return m_Flags & (unsigned int)flag; }
private:
	unsigned int m_Flags;
	
	// Loading mode
public:
	void SetLoadingBehavior(ProceduralLoadingBehavior behavior) { m_LoadingBehavior = behavior; }
	ProceduralLoadingBehavior GetLoadingBehavior() const { return m_LoadingBehavior; }
private:
	ProceduralLoadingBehavior m_LoadingBehavior;

// Substance processing
public:
	bool IsProcessing() const;
	static void SetProceduralProcessorUsage(ProceduralProcessorUsage processorUsage);
	static ProceduralProcessorUsage GetProceduralProcessorUsage();
	static void StopProcessing();
    SubstanceData* GetSubstanceData() { return m_SubstanceData; }

// Presets handling
	bool SetPreset(const std::string& presetContent);
	std::string GetPreset() const;

// Textures accessors
	ProceduralTexture* GetGeneratedTexture(const std::string& textureName);
	
// Integration
	unsigned int integrationTimeStamp;

#if UNITY_EDITOR
	UInt8* GetHashPtr() { return m_Hash.hashData.bytes; }
	void InvalidateIfCachedOrInvalidTextures();
	
	friend struct TemporarilyStripSubstanceData;
#endif

// Enter/Leave PlayMode
#if UNITY_EDITOR
	bool m_isAlreadyLoadedInCurrentScene;
#endif

// Caching
#if ENABLE_SUBSTANCE && !UNITY_EDITOR
	std::string GetCacheFolder() const;
	std::string GetCacheFilename(const ProceduralTexture& texture) const;
	bool ReadCachedTexture(string& fileName, std::map<ProceduralTexture*, SubstanceTexture>& cachedTextures, const std::string& folder, const ProceduralTexture& texture);
	bool WriteCachedTexture(string& fileName, const std::string& folder, const ProceduralTexture& texture, const SubstanceTexture& data);
#endif
};
