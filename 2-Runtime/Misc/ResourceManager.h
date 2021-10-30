#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Misc/Allocator.h"
#include <list>
#include "Runtime/Utilities/vector_set.h"
#include <string>
#include "Configuration/UnityConfigure.h"

#define kDefaultFontName "Arial.ttf"

class UnityWebStream;
class CachedUnityWebStream;
class ScriptMapper;

/*
	The resource manager can lookup assets by name and classID.

	An resource file is generated with the editor. And is shipped with unity.
	It provides the most basic assets necessary to run unity.
	Eg. some shaders, some textures for lighting
	
	If you want to add a new resource to the ResourceManager:
	- Call RegisterResource inside InitializeResources (Use a unique id and never reuse an id)
	- Open a project with the resources you want to add
	  All resources have to be placed inside a "Assets/DefaultResources"
	- When you have assembled all the resources. Use BuildResources from the main menu.
	  This will Install them in the right folder and also 
	  invoke installframeworks so it is installed in all versions of the editor
*/

class BuiltinResourceManager
{
	public:
	
	static void StaticInitialize();
	static void StaticDestroy();

	static void InitializeAllResources ();
	static bool AreResourcesInitialized();
	void DestroyAllResources ();
	
	void InitializeOldWebResources ();
	
	#if UNITY_EDITOR
	static void SetAllowLoadingBuiltinResources (bool allowLoadingBuiltinResources);
	void RegisterBuiltinScriptsForBuilding ();
	static void LoadAllDefaultResourcesFromEditor ();
	static void GetBuiltinResourcesOfClass (int classID, std::vector< std::pair<std::string,int> >& outResources);
	#endif
	
	// Returns the first found resource with name that is derived from classID
	Object* GetResource (int classID, const std::string& name);

	std::vector<Object*> GetAllResources ();

	int GetRequiredHideFlags() const { return m_RequiredHideFlags; }

	#if ENABLE_UNIT_TESTS
	void RegisterShadersWithRegistry (ScriptMapper* registry);
	#endif
	
private:

	void RegisterResource (LocalIdentifierInFileType fileID, const char* name, const char* className, bool userVisible = true);
	int RegisterResourceInternal (LocalIdentifierInFileType fileID, const char* name, const char* className, const char* displayName, bool userVisible = true);

	void RegisterShader (LocalIdentifierInFileType fileID, const char* name, const char* shaderClassName, bool userVisible = true);
	
	void RegisterBuiltinScript (int instanceID, const char*  className, bool buildResourceFiles);
	
	void InitializeResources ();
	void InitializeExtraResources ();
	void LoadDefaultResourcesFromEditor ();

	struct Resource
	{
		const char* name;
		int classID;
		
		LocalIdentifierInFileType fileID;
		int cachedInstanceID;
		bool userVisible;
		#if UNITY_EDITOR
		std::string cachedDisplayName;
		#endif
		
		friend bool operator < (const Resource& lhs, const Resource& rhs)
		{
			int res = strcmp(lhs.name, rhs.name);
			if (res != 0)
				return res < 0;
			
			return lhs.classID < rhs.classID;
		}
	};
	
	typedef vector_set<Resource> Resources;
	Resources m_Resources;
	std::string m_ResourcePath;
	int         m_RequiredHideFlags;
	bool         m_AllowResourceManagerAccess;
	
	#if UNITY_EDITOR
	
	struct ShaderInitializationData
	{
		const char* shaderClassName;
		const char* resourceName;
		int         cachedInstanceID;
	};
	
	typedef std::vector<ShaderInitializationData> OrderedShaderStartup;
	OrderedShaderStartup m_OrderedShaderStartup;

	friend bool BuildBuiltinAssetBundle (BuiltinResourceManager& resourceManager, const std::string& targetFile, const std::string& resourceFolder, InstanceIDResolveCallback* resolveCallback, const std::set<std::string>& ignoreList, BuildTargetSelection platform, int options);
	friend bool GenerateBuiltinAssetPreviews ();
	void GetResourcesOfClass (int classID, std::vector< std::pair<std::string,int> >& outResources);
	#endif
	
	#if DEBUGMODE
	void RegisterDebugUsedFileID (LocalIdentifierInFileType fileID, const char* name);
	std::set<LocalIdentifierInFileType, std::less<LocalIdentifierInFileType>, STL_ALLOCATOR(kMemPermanent, LocalIdentifierInFileType) >  m_UsedFileIDs;
	#else
	void RegisterDebugUsedFileID (LocalIdentifierInFileType fileID, const char* name) {}
	#endif
};

BuiltinResourceManager& GetBuiltinResourceManager ();
BuiltinResourceManager& GetBuiltinExtraResourceManager ();
BuiltinResourceManager& GetBuiltinOldWebResourceManager ();

template<class T>
T* GetBuiltinResource (const std::string& name)
{
	Object* res = GetBuiltinResourceManager ().GetResource (T::GetClassIDStatic (), name);
	return static_cast<T*> (res);
}

#if WEBPLUG
template<class T>
T* GetBuiltinOldWebResource (const std::string& name)
{
	Object* res = GetBuiltinOldWebResourceManager ().GetResource (T::GetClassIDStatic (), name);
	return static_cast<T*> (res);
}
#endif // #if WEBPLUG

#if UNITY_EDITOR
template<class T>
T* GetBuiltinExtraResource (const std::string& name)
{
	Object* res = GetBuiltinExtraResourceManager ().GetResource (T::GetClassIDStatic (), name);
	return static_cast<T*> (res);
}
#endif // #if UNITY_EDITOR


class ResourceManager : public GlobalGameManager
{
	public:

	struct Dependency
	{
		typedef std::vector<PPtr<Object> > ChildCont;
		DECLARE_SERIALIZE (ResourceManager_Dependency)

		struct Sorter
		{
			bool operator() (Dependency const& a, Dependency const& b) const { return a.object.GetInstanceID () < b.object.GetInstanceID (); }
			bool operator() (Dependency const& a, SInt32 b) const { return a.object.GetInstanceID () < b; }
			bool operator() (SInt32 a, Dependency const& b) const { return a < b.object.GetInstanceID (); }
		};

		Dependency () {}
		explicit Dependency (SInt32 thisInstanceId) : object (thisInstanceId) {}

		PPtr<Object> object;
		ChildCont dependencies;
	};

	typedef std::vector<Dependency> DependencyContainer;
	
	DECLARE_OBJECT_SERIALIZE (ResourceManager)
	REGISTER_DERIVED_CLASS (ResourceManager, GlobalGameManager)

	ResourceManager (MemLabelId label, ObjectCreationMode mode);

	typedef  std::multimap<UnityStr, PPtr<Object> > container;
	typedef  std::multimap<UnityStr, PPtr<Object> >::iterator iterator; 
	typedef  std::pair<iterator, iterator> range;

	void RegisterResource (std::string& path, PPtr<Object> resource);
	std::string GetResourcePath (const std::string& path);
	
	range GetAll ();
	range GetPathRange (const string& path);
	
	void SetResourcesNeedRebuild() { m_NeedsReload = true; }
	bool ShouldIgnoreInGarbageDependencyTracking ();
	
	void RebuildResources ();

	void ClearDependencyInfo ();
	void PreloadDependencies (SInt32 instanceId);

	DependencyContainer m_DependentAssets;
	container m_Container;
	bool         m_NeedsReload;

private:
	void PreloadDependencies (SInt32 instanceId, std::set<SInt32>& visited);
};

ResourceManager& GetResourceManager ();

extern const char* kResourcePath;
extern const char* kOldWebResourcePath;
extern const char* kEditorResourcePath;
extern const char* kDefaultExtraResourcesPath;

#endif
