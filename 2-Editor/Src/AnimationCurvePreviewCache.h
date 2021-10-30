#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Math/Color.h"
#include "SceneInspector.h"

class Texture2D;
class Object;
class SerializedProperty;

///@TODO: This should be rewritten with a common baseclass for GradientPreviewCache and AnimationCurvePreviewCache.
///       This can easily be done by making CacheEntry simply be UInt8 keys[48]; and using memcmp for comparision on it.
///       Then you have two classes that simply deal with actually generating the textures, but don't deal with the invalidation logic and lookups.

class AnimationCurvePreviewCache : ISceneInspector
{
public: 
	struct PreviewSize
	{
		PreviewSize(int width_, int height_) 
		:	width(width_), height(height_) 
		{}

		int width;
		int height;
	};
		
	Texture2D* GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color);
	Texture2D* GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color, const Rectf& curveRange);
	Texture2D* GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve& curve2, const ColorRGBAf& color);
	Texture2D* GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve& curve2, const ColorRGBAf& color, const Rectf& curveRange);

	Texture2D* GetPreview (const PreviewSize& previewSize, SerializedProperty& property, const ColorRGBAf& color);
	Texture2D* GetPreview (const PreviewSize& previewSize, SerializedProperty& property, const ColorRGBAf& color, const Rectf& curveRange);
	Texture2D* GetPreview (const PreviewSize& previewSize, SerializedProperty& propertyMax, SerializedProperty& propertyMin, const ColorRGBAf& color);
	Texture2D* GetPreview (const PreviewSize& previewSize, SerializedProperty& propertyMax, SerializedProperty& propertyMin, const ColorRGBAf& color, const Rectf& curveRange);

	void ClearCache ();

	static AnimationCurvePreviewCache& Get ();
	static void Cleanup ();

	// Clear cache when selecting another object
	virtual void ForceReloadInspector (bool fullRebuild);
	virtual void SelectionHasChanged (const std::set<int>& selection);

private:
	Texture2D* LookupCache (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color, const Rectf& curveRange);
	Texture2D* LookupCache (const PreviewSize& previewSize, Object* obj, const char* propertyName, const ColorRGBAf& color, const Rectf& curveRange);
	
	Texture2D* GetPreviewImpl (const PreviewSize& previewSize, Object* obj, const char* propertyName, AnimationCurve& curve, AnimationCurve* curve2, const ColorRGBAf& color, const Rectf& curveRange);
	Texture2D* GeneratePreviewTexture (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve* curve2, const ColorRGBAf& color, const Rectf& curveRange);

	AnimationCurvePreviewCache ();
	~AnimationCurvePreviewCache ();

	struct CacheEntry 
	{
		// Key
		PreviewSize previewSize;
		ColorRGBAf color;
		void* object;
		int hash;
		Rectf curveRange;

		// value
		Texture2D* texture;

		CacheEntry (const PreviewSize& previewSize, void* obj, const char* propertyName, const ColorRGBAf& color, const Rectf& curveRange);
		CacheEntry (const PreviewSize& previewSize, AnimationCurve* curve, const ColorRGBAf& color, const Rectf& curveRange);

		friend bool operator < (const AnimationCurvePreviewCache::CacheEntry& lhs, const AnimationCurvePreviewCache::CacheEntry& rhs)
		{
			int size = reinterpret_cast<const UInt8*> (&lhs.texture) - reinterpret_cast<const UInt8*> (&lhs);
			return memcmp(&lhs, &rhs, size) < 0;
		}
		
	private:
	};
	typedef std::set<CacheEntry> Entries;
	Entries m_Entries;

};

