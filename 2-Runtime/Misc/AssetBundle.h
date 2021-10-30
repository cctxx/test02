#pragma once

#include "Runtime/BaseClasses/NamedObject.h"

#include <map>
#include <utility>

class UnityWebStream;
class CachedUnityWebStream;

class AssetBundleScriptInfo
{
public:
	DECLARE_SERIALIZE(AssetBundleScriptInfo)

	AssetBundleScriptInfo () {}
	AssetBundleScriptInfo (const UnityStr& name, const UnityStr& ns, const UnityStr& assembly, UInt32 h) : className (name), nameSpace (ns), hash (h), assemblyName(assembly) {}
	UnityStr className;
	UnityStr nameSpace;
	UnityStr assemblyName;
	UInt32   hash;
};


class AssetBundle : public NamedObject
{
public:

	typedef std::vector<AssetBundleScriptInfo> ScriptCompatibility;
	typedef std::vector<std::pair<int, UInt32> > ClassCompatibility;

	enum
	{
		/// A simple integer version count to keep track of changes to the
		/// runtime that cause asset bundles to no longer work as intended.
		/// Increase this version whenever you need to break backwards-compatibility.
		/// An example of this is when we started to no longer package all default
		/// resources with every player and thus broke asset bundles that were
		/// referencing these resources.
		///
		/// @note Runtime compatibility checks are bypassed for the webplayer where
		///		breaking backwards-compatibility is not allowed at this point.
		///
		/// @see TestAssetBundleCompatiblity
		CURRENT_RUNTIME_COMPATIBILITY_VERSION = 1
	};

	struct AssetInfo
	{
		DECLARE_SERIALIZE(AssetInfo)

		int preloadIndex;
		int preloadSize;

		PPtr<Object> asset;

		AssetInfo () { preloadIndex = 0; preloadSize = 0; }
	};
	struct UncompressedFileInfo
	{
		std::string fileName;
		UInt32 offset, size;

		bool operator < (UncompressedFileInfo const& a) const { return fileName < a.fileName; }
	};
	typedef std::vector<UncompressedFileInfo> UncompressedFileInfoContainer;

	DECLARE_OBJECT_SERIALIZE (AssetBundle)
	REGISTER_DERIVED_CLASS (AssetBundle, NamedObject)

	AssetBundle (MemLabelId label, ObjectCreationMode mode);
	// ~AssetBundle (); declared-by-macro

	typedef  std::multimap<UnityStr, AssetInfo> AssetMap;
	typedef  AssetMap::iterator iterator; 
	typedef  std::pair<iterator, iterator> range;

	range GetAll ();
	range GetPathRange (const string& path);

	virtual bool ShouldIgnoreInGarbageDependencyTracking ();

	void DebugPrintContents ();

	// 	AssetBundle* file = GetEditorAssetBundle();
	// 	if (file) {
	//     // Path MUST omit the extension
	//     MonoBehaviour* be = file->Get<MonoBehaviour>(MySkin);
	// }
	template<class T>
	T* Get (const string& path)
	{
		Object* res = GetImpl (T::GetClassIDStatic (), path);
		return static_cast<T*> (res);
	} 

	Object* GetImpl (int classID, const string& path);

	void AddScriptCompatibilityInfo (std::string const& className, std::string const& nameSpace, std::string const& assembly, UInt32 hash);
	void FillHashTableForRuntimeClasses (std::vector<SInt32> const& classIds, TransferInstructionFlags transferFlags);

	UInt32 m_RuntimeCompatibility;
	ScriptCompatibility	m_ScriptCompatibility;
	ClassCompatibility m_ClassCompatibility;

	/// AssetInfo for the main asset.  Has no associated name.
	AssetInfo m_MainAsset;

	/// Table of objects that need to be pulled from the bundle by the preload
	/// manager when a specific asset is loaded from the bundle.  Each AssetInfo
	/// entry has an associated range of entries in the preload table.
	std::vector<PPtr<Object> > m_PreloadTable;

	/// Map of named assets contained in the bundle.  Multiple objects may
	/// have the same name.
	AssetMap m_Container;

#if ENABLE_WWW
	UnityWebStream* m_UnityWebStream;
#endif
#if ENABLE_CACHING
	CachedUnityWebStream* m_CachedUnityWebStream;
#endif
	UncompressedFileInfoContainer* m_UncompressedFileInfo;
};

AssetBundle* GetEditorAssetBundle ();
