#ifndef ASSETBUNDLEUTILITY_H
#define ASSETBUNDLEUTILITY_H

#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Misc/AssetBundle.h"

#if ENABLE_WWW
class WWW;

AssetBundle* ExtractAssetBundle(WWW &www);


class AssetBundleCreateRequest : public PreloadManagerOperation
{
public:
	AssetBundleCreateRequest( const UInt8* dataPtr, int dataSize );
	virtual ~AssetBundleCreateRequest();

	virtual void Perform ();
	virtual bool HasIntegrateMainThread () { return true; }
	virtual void IntegrateMainThread ();

	virtual float GetProgress ();

#if ENABLE_PROFILER
	virtual std::string GetDebugName ()
	{
		static std::string debugName("AssetBundleCreateRequest");
		return debugName;
	}
#endif

	AssetBundle* GetAssetBundle()
	{
		return m_AssetBundle;
	}

	/// Set whether to perform runtime compatibility checks or not.  Unfortunately, in the
	/// editor we get lots of old asset bundles handed to us from the asset store to deliver
	/// previews.  Given that in the editor we can actually still run that content (unlike in
	/// players), we allow disabling checks for those bundles.
	///
	/// This is an internal feature only.
	void SetEnableCompatibilityChecks (bool value)
	{
		m_EnableCompatibilityChecks = value;
	}

protected:
	UnityWebStream* m_UnityWebStream;
	PPtr<AssetBundle> m_AssetBundle;
	bool m_EnableCompatibilityChecks;
};
#endif

AssetBundle* ExtractAssetBundle(std::string const& assetBundlePathName);

/// Return true if the given asset bundle can be used with the current player.
/// If not, "error" will be set to a description of why the bundle cannot be used.
/// The given "bundleName" is only used for printing more informative error
/// messages.
bool TestAssetBundleCompatibility (AssetBundle& bundle, const std::string& bundleName, std::string& error);

Object* LoadNamedObjectFromAssetBundle (AssetBundle& bundle, const std::string& name, ScriptingObjectPtr type);
Object* LoadMainObjectFromAssetBundle (AssetBundle& bundle);
void LoadAllFromAssetBundle (AssetBundle& assetBundle, ScriptingObjectPtr type, std::vector<Object* >& output);

struct AssetBundleRequestMono
{
	AsyncOperation* m_Result;
	ScriptingObjectPtr m_AssetBundle;
	ScriptingStringPtr m_Path;
	ScriptingObjectPtr m_Type;
};

#endif	// ASSETBUNDLEUTILITY_H
