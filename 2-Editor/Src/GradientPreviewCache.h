#include "SceneInspector.h"

class Texture2D;
class Object;
class SerializedProperty;
class GradientNEW;

///@TODO: This should be rewritten with a common baseclass for GradientPreviewCache and AnimationCurvePreviewCache.
///       This can easily be done by making CacheEntry simply be UInt8 keys[48]; and using memcmp for comparision on it.
///       THen you have two classes that simply deal with actually generating the textures, but don't deal with the invalidation logic and lookups.

class GradientPreviewCache : ISceneInspector
{
public:
		
	Texture2D* GetPreview (GradientNEW& gradient);
	Texture2D* GetPreview (SerializedProperty& property);
	void ClearCache ();

	static GradientPreviewCache& Get ();
	static void Cleanup ();

	// Clear cache when selecting another object
	virtual void ForceReloadInspector (bool fullRebuild);
	virtual void SelectionHasChanged (const std::set<int>& selection);

private:
	Texture2D* LookupCache (GradientNEW& gradient);
	Texture2D* LookupCache (Object* obj, const char* propertyName);
	
	Texture2D* GetPreviewInternal (Object* obj, const char* propertyName, GradientNEW& gradient);
	Texture2D* GeneratePreviewTexture (GradientNEW& gradient);

	GradientPreviewCache ();
	virtual ~GradientPreviewCache ();

	struct CacheEntry 
	{
		void* object;
		int hash;
		Texture2D* texture;

		CacheEntry (void* obj, const char* propertyName);
		CacheEntry (GradientNEW* gradient);

		friend bool operator < (const GradientPreviewCache::CacheEntry& lhs, const GradientPreviewCache::CacheEntry& rhs)
		{
			if (lhs.hash == rhs.hash)
				return lhs.object < rhs.object;
			else
				return lhs.hash < rhs.hash;
		}
	};

	typedef std::set<CacheEntry> Entries;
	Entries m_Entries;
};


