#include "UnityPrefix.h"
#include "AssetBundle.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Misc/ResourceManager.h"

#if UNITY_EDITOR
#include "Editor/Src/Utility/RuntimeClassHashing.h"
#endif

#if ENABLE_WWW
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#endif

#if ENABLE_CACHING
#include "Runtime/Misc/CachingManager.h"
#endif

using namespace std;

IMPLEMENT_CLASS (AssetBundle)
IMPLEMENT_OBJECT_SERIALIZE (AssetBundle)

AssetBundle::AssetBundle (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
#if ENABLE_WWW
	m_UnityWebStream = NULL;
#endif
#if ENABLE_CACHING
	m_CachedUnityWebStream = NULL;
#endif

	m_UncompressedFileInfo = NULL;
	m_RuntimeCompatibility = CURRENT_RUNTIME_COMPATIBILITY_VERSION;

	// Mark as kDontSave, so that AssetBundles are not unloaded on level loads
	SetHideFlags(Object::kDontSave);
}

template<class TransferFunc>
void AssetBundle::AssetInfo::Transfer (TransferFunc& transfer)
{
	TRANSFER(preloadIndex);
	TRANSFER(preloadSize);
	TRANSFER(asset);
}

template<class TransferFunc>
void AssetBundleScriptInfo::Transfer (TransferFunc& transfer)
{
	TRANSFER (className);
	TRANSFER (nameSpace);
	TRANSFER (assemblyName);
	TRANSFER (hash);
}

template<class TransferFunc>
void AssetBundle::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (3);
	AssertIf (transfer.GetFlags() & kPerformUnloadDependencyTracking);

	if (transfer.IsReading ())
		m_RuntimeCompatibility = 0;

	if (transfer.IsOldVersion(1))
	{
		multimap<UnityStr, PPtr<Object> > oldContainer;
		transfer.Transfer(oldContainer, "m_Container");
		PPtr<Object> mainAsset;
		transfer.Transfer(mainAsset, "m_MainAsset");

		m_Container.clear();
		for (multimap<UnityStr, PPtr<Object> >::iterator i=oldContainer.begin();i != oldContainer.end();i++)
		{
			AssetInfo info;
			info.preloadIndex = 0;
			info.preloadSize = 0;
			info.asset = i->second;
			m_Container.insert(make_pair(i->first, info));
		}

		m_MainAsset.preloadIndex = 0;
		m_MainAsset.preloadSize = 0;
		m_MainAsset.asset = mainAsset;
	}
	else
	{
		transfer.Transfer(m_PreloadTable, "m_PreloadTable");
		transfer.Transfer(m_Container, "m_Container");
		transfer.Transfer(m_MainAsset, "m_MainAsset");
		transfer.Transfer(m_ScriptCompatibility, "m_ScriptCompatibility");
		transfer.Transfer(m_ClassCompatibility, "m_ClassCompatibility");

		if (!transfer.IsOldVersion (2))
			transfer.Transfer (m_RuntimeCompatibility, "m_RuntimeCompatibility");
	}
}

void AssetBundle::DebugPrintContents ()
{
	for (iterator i=m_Container.begin();i != m_Container.end();i++)
	{
		printf_console("- %s\n", i->first.c_str());
	}
}

AssetBundle::range AssetBundle::GetPathRange (const string& path)
{
//	if (m_Container.equal_range(ToLower(path)).first == m_Container.equal_range(ToLower(path)).second)
//	{
//		printf_console(("Failed loading " + path  + "\n***********\n").c_str());
//		DebugPrintContents();
//	}

	return m_Container.equal_range(ToLower(path));
}

AssetBundle::range AssetBundle::GetAll ()
{
	return make_pair (m_Container.begin(), m_Container.end());
}

Object* AssetBundle::GetImpl (int classID, const string& path)
{
	range r = GetPathRange(path);
	for (iterator i=r.first;i != r.second;i++)
	{
		Object* obj = i->second.asset;
		if (obj && obj->IsDerivedFrom(classID))
			return obj;
	}

	//printf_console(("Failed loading " + path  + "\n***********\n").c_str());
	//DebugPrintContents();

	return NULL;
}

AssetBundle::~AssetBundle ()
{
#if ENABLE_WWW
	if (m_UnityWebStream)
		m_UnityWebStream->Release();
#endif
#if ENABLE_CACHING
	UNITY_DELETE(m_CachedUnityWebStream, kMemFile);
#endif
	// Don't kill of m_UncompressedFileInfo here due to weird
	// logic in UnloadAssetBundle() that will access the UncompressedFileInfo
	// even after the AssetBundle has been deleted through DestroyAllAtPath().
}

bool AssetBundle::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

#if UNITY_EDITOR

void AssetBundle::AddScriptCompatibilityInfo (std::string const& className, std::string const& nameSpace, std::string const& assembly, UInt32 hash)
{
	m_ScriptCompatibility.push_back (AssetBundleScriptInfo (className, nameSpace, assembly, hash));
}

void AssetBundle::FillHashTableForRuntimeClasses (std::vector<SInt32> const& classIds, TransferInstructionFlags transferFlags)
{
	vector_map<int, UInt32> hashes;
	CalculateHashForClasses (hashes, classIds, transferFlags);
	m_ClassCompatibility.resize (hashes.size ());
	std::copy (hashes.begin (), hashes.end (), m_ClassCompatibility.begin ());
}

AssetBundle* GetEditorAssetBundle ()
{
	int instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(kEditorResourcePath, 1);
	PPtr<AssetBundle> res (instanceID);
	return res;
}

#endif
