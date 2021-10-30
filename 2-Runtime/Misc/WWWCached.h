#ifndef WWWCACHED_H
#define WWWCACHED_H

#define kWWWCachedAccessError "WWWCached data can only be accessed using the assetBundle property!"

#if ENABLE_CACHING && ENABLE_WWW

#include "Runtime/Export/WWW.h"


class WWWCached : public WWW
{
	char* m_URL;
	bool m_DidDownload;
	bool m_Abort;
	WWW* m_WWW;
	string m_Error;
	AsyncCachedUnityWebStream* m_CacheRequest;
	bool m_AssetBundleRetrieved;

	void StartDownload(bool cached);
		
public:
	WWWCached (const char* url, int version, UInt32 crc);
	~WWWCached ();
	
	AssetBundle* GetAssetBundle ();
	
	virtual const UInt8* GetData();
	virtual const UInt8* GetPartialData() const;
	virtual size_t GetSize();
	virtual size_t GetPartialSize() const;
	
	virtual double GetETA() const;
	
	virtual void LockPartialData() {}
	virtual void UnlockPartialData() {}
		
	// Returns true when the download is complete or failed.
	virtual void Cancel();
	virtual bool IsDownloadingDone() const;
	virtual float GetProgress() const;
	virtual float GetUploadProgress() const { return 0.0f; }
	virtual const char* GetError();
	virtual const char* GetUrl() const { return m_URL; }
	
	virtual bool HasDownloadedOrMayBlock ();
	virtual void BlockUntilDone ();

	virtual void SetThreadPriority( ThreadPriority priority );

	virtual WWWType GetType () const { return kWWWTypeCached; }

private:
	bool IsDoneImpl();
};

#endif //ENABLE_CACHING
#endif
