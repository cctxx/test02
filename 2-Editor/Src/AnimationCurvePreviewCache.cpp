#include "UnityPrefix.h"
#include "AnimationCurvePreviewCache.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Editor/Src/Utility/SerializedProperty.h"

using namespace std;


static AnimationCurvePreviewCache* gAnimationCurvePreviewCache = NULL;

void AnimationCurvePreviewCache::Cleanup ()
{
	delete gAnimationCurvePreviewCache;
	gAnimationCurvePreviewCache = NULL;
}

AnimationCurvePreviewCache& AnimationCurvePreviewCache::Get ()
{
	if (gAnimationCurvePreviewCache == NULL) 
		gAnimationCurvePreviewCache = new AnimationCurvePreviewCache();
	return *gAnimationCurvePreviewCache;
}

void AnimationCurvePreviewCache::ClearCache ()
{
	for (Entries::iterator i=m_Entries.begin();i != m_Entries.end();i++)
		DestroySingleObject(i->texture);
	
	m_Entries.clear();
}


AnimationCurvePreviewCache::AnimationCurvePreviewCache ()
{
	GetSceneTracker().AddSceneInspector(this);
}

AnimationCurvePreviewCache::~AnimationCurvePreviewCache ()
{
	GetSceneTracker().RemoveSceneInspector(this);
	ClearCache();
}

const Rectf kDefaultRange(0, 0, -1, -1);

Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color)
{
	return GetPreview(previewSize, curve, color, kDefaultRange);
}
Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color, const Rectf& curveRange) 
{
	return GetPreviewImpl(previewSize, NULL, NULL, curve, NULL, color, curveRange);
}

Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve& curve2, const ColorRGBAf& color) 
{
	return GetPreview(previewSize, curve, curve2, color, kDefaultRange);	
}
Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve& curve2, const ColorRGBAf& color, const Rectf& curveRange) 
{
	return GetPreviewImpl(previewSize, NULL, NULL, curve, &curve2, color, curveRange);
}



Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, SerializedProperty& property, const ColorRGBAf& color)
{
	return GetPreview(previewSize, property, color, kDefaultRange);
}
Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, SerializedProperty& property, const ColorRGBAf& color, const Rectf& curveRange) 
{ 
	property.SyncSerializedObjectVersion();
	string propertyPath = property.GetPropertyPath();
	Object* target = property.GetSerializedObject()->GetTargetObject();

	Texture2D* tex = LookupCache(previewSize, target, propertyPath.c_str(), color, curveRange);
	if (tex)
		return tex;

	AnimationCurve* curve = property.GetAnimationCurveValueCopy();
	if (curve == NULL)
		return NULL;
	
	tex = GetPreviewImpl (previewSize, target, propertyPath.c_str(), *curve, NULL, color, curveRange);
	delete curve;

	return tex;
}

Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, SerializedProperty& property, SerializedProperty& property2, const ColorRGBAf& color)
{
	return GetPreview (previewSize, property, property2, color, kDefaultRange);
}
Texture2D* AnimationCurvePreviewCache::GetPreview (const PreviewSize& previewSize, SerializedProperty& property, SerializedProperty& property2, const ColorRGBAf& color, const Rectf& curveRange)
{
	property.SyncSerializedObjectVersion();
	property.SyncSerializedObjectVersion();
	string propertyPath = property.GetPropertyPath();
	Object* target = property.GetSerializedObject()->GetTargetObject();

	Texture2D* tex = LookupCache(previewSize, target, propertyPath.c_str(), color, curveRange);
	if (tex)
		return tex;

	AnimationCurve* curve = property.GetAnimationCurveValueCopy();
	if (curve == NULL)
		return NULL;
	
	property2.SyncSerializedObjectVersion();
	AnimationCurve* curve2 = property2.GetAnimationCurveValueCopy();
	if (curve2 == NULL)
	{
		delete curve;
		return NULL;
	}

	tex = GetPreviewImpl (previewSize, target, propertyPath.c_str(), *curve, curve2, color, curveRange);
	delete curve;
	delete curve2;

	return tex;

}

Texture2D* AnimationCurvePreviewCache::GetPreviewImpl (const PreviewSize& previewSize, Object* obj, const char* propertyName, AnimationCurve& curve, AnimationCurve* curve2, const ColorRGBAf& color, const Rectf& curveRange)
{
	// Find cached preview texture
	Texture2D* preview = NULL;
	if (obj)
		preview = LookupCache (previewSize, obj, propertyName, color, curveRange);
	else
		preview = LookupCache (previewSize, curve, color, curveRange);
		
	if (preview != NULL)	
		return preview;
	
	// Clear cache whenever we have too many previews. It's very unlikely there are 50 preview textures on screen.
	if (m_Entries.size () > 50)
		ClearCache();
	
	// Generate preview			
	preview = GeneratePreviewTexture(previewSize, curve, curve2, color, curveRange);	
	if (preview == NULL)
		return NULL;
	
	// Inject preview into cache
	if (obj)
	{
		CacheEntry entry (previewSize, obj, propertyName, color, curveRange);
		entry.texture = preview;
		bool inserted = m_Entries.insert(entry).second;
		Assert(inserted);
	}
	else
	{
		CacheEntry entry (previewSize, &curve, color, curveRange);
		entry.texture = preview;
		bool inserted = m_Entries.insert(entry).second;
		Assert(inserted);
	}
	
	return preview;
}

Texture2D* AnimationCurvePreviewCache::LookupCache (const PreviewSize& previewSize, AnimationCurve& curve, const ColorRGBAf& color, const Rectf& curveRange)
{
	CacheEntry entry (previewSize, &curve, color, curveRange);
	Entries::iterator result = m_Entries.find(entry);
	if (result != m_Entries.end())
		return result->texture;
	else
		return NULL;
}

Texture2D* AnimationCurvePreviewCache::LookupCache (const PreviewSize& previewSize, Object* obj, const char* propertyName, const ColorRGBAf& color, const Rectf& curveRange)
{
	CacheEntry entry (previewSize, obj, propertyName, color, curveRange);
	Entries::iterator result = m_Entries.find(entry);
	if (result != m_Entries.end())
		return result->texture;
	else
		return NULL;
}

float EvaluateNormalized (AnimationCurve& curve, float x, int width, pair<float, float> range)
{
	float time = x / float(width - 1);
	time = (range.second - range.first) * time + range.first;
	return curve.Evaluate(time);
}

float EvaluateNormalized (AnimationCurve& curve, float x, int width, pair<float, float> xrange, pair<float, float> yrange)
{
	return (EvaluateNormalized(curve, x, width, xrange) - yrange.first) / (yrange.second - yrange.first);
}

static void DrawCurve ( ImageReference &image, 
						AnimationCurve& curve, 
						int width, int height, 
						int innerwidth, 
						int margin, int topmargin, int bottommargin, 
						const ColorRGBAf& color, 
						pair<float, float> range, 
						pair<float, float> verticalRange)
{
	ColorRGBA32 lineColor = ColorRGBA32((int)(color.r*255+0.5f), (int)(color.g*255+0.5f), (int)(color.b*255+0.5f), (int)(color.a*255+0.5f));
	ColorRGBA32 wrapColor = ColorRGBA32(255, 255, 255, (int)(color.a* 64+0.5f));
	

	if (curve.GetKeyCount () >= 2)
	{
		// Paint by sampling the curve at each step and drawing a dot
		int pixelyprev = RoundfToInt(EvaluateNormalized(curve, -margin-0.5f, innerwidth, range, verticalRange) * (height - 1-topmargin-bottommargin))+bottommargin;
		for (int x=0;x<width;x++)
		{
			float value01 = EvaluateNormalized(curve, x-margin+0.5f, innerwidth, range, verticalRange);

			// Draw curve between pixels 1 ... height-2
			int pixely = RoundfToInt(value01 * (height - 1-topmargin-bottommargin))+bottommargin;

			// Draw curve
			ColorRGBA32* pixel;
			int ymin = min(pixely,pixelyprev);
			int ymax = max(pixely,pixelyprev);
			ymax = max(ymin+1,ymax);

			ymin = std::max(0, ymin);
			ymax = std::min(height, ymax);
			for (int y=ymin;y<ymax;y++)
			{
				pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(y)) + x;
				if (x <= margin || x >= width-margin-1)
					*pixel = wrapColor;
				else
					*pixel = lineColor;
			}
			pixelyprev = pixely;
		}
	}
	else if (curve.GetKeyCount () == 1)
	{
		float value01 = EvaluateNormalized(curve, 0, innerwidth, range, verticalRange);
		int pixely = RoundfToInt(value01 * (height - 1-topmargin-bottommargin))+bottommargin;
		pixely = clamp(pixely, 0, height-1);

		for (int x=0;x<width;x++)
		{
			// Draw horizontal line
			ColorRGBA32* pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(pixely)) + x;
			*pixel = lineColor;
		}
	}
}

static void DrawRegion (ImageReference &image, 
						AnimationCurve& curve, 
						AnimationCurve& curve2, 
						int width, int height, int innerwidth, 
						int margin, int topmargin, int bottommargin, 
						const ColorRGBAf& color, 
						pair<float, float> range, 
						pair<float, float> verticalRange)
{
	ColorRGBA32 regionColor = ColorRGBA32((int)(color.r*255+0.5f), (int)(color.g*255+0.5f), (int)(color.b*255+0.5f), (int)(color.a*0.4f*255.f+0.5f));
	ColorRGBA32 wrapColor = ColorRGBA32(255, 255, 255, (int)(color.a* 64+0.5f));

	int keyCount1 = curve.GetKeyCount();
	int keyCount2 = curve2.GetKeyCount();
	if (keyCount1 >= 1 && keyCount2 >= 1)
	{
		// Paint by sampling the curve at each step and drawing a dot
		for (int x=0; x<width; x++)
		{
			float value1, value2;
			if (keyCount1 >= 2)
				value1 = EvaluateNormalized(curve, x-margin+0.5f, innerwidth, range, verticalRange);
			else
				value1 = EvaluateNormalized(curve, 0, innerwidth, range, verticalRange);

			if (keyCount2 >= 2)
				value2 = EvaluateNormalized(curve2, x-margin+0.5f, innerwidth, range, verticalRange);
			else
				value2 = EvaluateNormalized(curve2, 0, innerwidth, range, verticalRange);

			int pixely1 = RoundfToInt(value1 * (height - 1-topmargin-bottommargin))+bottommargin;
			int pixely2 = RoundfToInt(value2 * (height - 1-topmargin-bottommargin))+bottommargin;

			// Draw region
			int ymin = pixely1; 
			int ymax = pixely2; 
			if (ymin > ymax)
			{
				int tmp = ymin;
				ymin = ymax; 
				ymax = tmp;
			}
			ymax = max(ymin+1,ymax);

			ymin = std::max(0, ymin);
			ymax = std::min(height, ymax);
			
			ColorRGBA32* pixel;
			for (int y=ymin;y<ymax;y++)
			{
				pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(y)) + x;
				if (x <= margin || x >= width-margin-1)
					*pixel = wrapColor;
				else
					*pixel = regionColor;
			}
		}
	}
}

Texture2D* AnimationCurvePreviewCache::GeneratePreviewTexture (const PreviewSize& previewSize, AnimationCurve& curve, AnimationCurve* curve2, const ColorRGBAf& color, const Rectf& curveRange)
{
	ColorRGBA32 zeroColor = ColorRGBA32( 40,  40,  40, (int)(color.a*255+0.5f));

	int width = previewSize.width;
	int height = previewSize.height;
	int margin = 0; //previewSize.width / 8; // Discuss removing margin
	int innerwidth = width - margin*2;

	const int topmargin = 1;
	const int bottommargin = 1;
	height = max(height, topmargin + bottommargin);
	

	Texture2D* texture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad, kMemTextureCache);
	texture->SetHideFlags(Object::kHideAndDontSave);
	texture->InitTexture (width, height, kTexFormatRGBA32, Texture2D::kNoMipmap, 1);
	
	ImageReference image;
	texture->GetWriteImageReference(&image, 0, 0);
	image.ClearImage(ColorRGBA32(0, 0, 0, 0));
	
	// Display ranges
	pair<float, float> range (curveRange.x, curveRange.GetXMax());
	pair<float, float> verticalRange (curveRange.y, curveRange.GetYMax());

	if (curveRange == kDefaultRange && curve.GetKeyCount () >= 2)
	{
		range = curve.GetRange();

		// Figure out the vertical height of the curve
		verticalRange = make_pair(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()); 
		for (int x=0;x<=innerwidth;x++)
		{
			float value = EvaluateNormalized(curve, x-0.5f, innerwidth, range);

			verticalRange.first = min(verticalRange.first, value);
			verticalRange.second = max(verticalRange.second, value);
		}

		if (verticalRange.first == verticalRange.second)
		{
			verticalRange.first -= 1;
			verticalRange.second += 1;
		}
	}

	// Paint horizontal 0 line
	int zeroy = RoundfToInt((0 - verticalRange.first) / (verticalRange.second - verticalRange.first) * (height - 1-topmargin-bottommargin))+1;
	if (zeroy >= bottommargin && zeroy <= height-1-topmargin)
	{
		AssertIf(zeroy < 0 || zeroy >= height);
		for (int x=0;x<width;x++)
		{
			// Draw line
			ColorRGBA32* pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(zeroy)) + x;
			*pixel = zeroColor;
		}
	}

	// Draw
	if (curve2 == NULL)
	{
		DrawCurve (image, curve, width, height, innerwidth, margin, topmargin, bottommargin, color, range, verticalRange);
	}
	else
	{
		DrawRegion (image, curve, *curve2, width, height, innerwidth, margin, topmargin, bottommargin, color, range, verticalRange);
		DrawCurve  (image, curve, width, height, innerwidth, margin, topmargin, bottommargin, color, range, verticalRange);
		DrawCurve  (image, *curve2, width, height, innerwidth, margin, topmargin, bottommargin, color, range, verticalRange);
	}
	

	// Clear bottom row and top row so it fits better into the inspector
	for (int x=0;x<width;x++)
	{
		ColorRGBA32* pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(height-1)) + x;
		*pixel = ColorRGBA32 (86, 86, 86, 0);
	}
	
	texture->UpdateImageDataDontTouchMipmap();
	
	return texture;
}



void AnimationCurvePreviewCache::SelectionHasChanged (const std::set<int>& selection)
{
	AnimationCurvePreviewCache::Get().ClearCache();
}

void AnimationCurvePreviewCache::ForceReloadInspector (bool fullRebuild)
{
	AnimationCurvePreviewCache::Get().ClearCache();
}

AnimationCurvePreviewCache::CacheEntry::CacheEntry (const PreviewSize& previewSize_, void* obj, const char* propertyName, const ColorRGBAf& col, const Rectf& range)
: previewSize(previewSize_)
{
	// we are using memcmp in operator less; clear whole memory of the object (including padding)
	memset (this, 0, sizeof(*this));

	previewSize = previewSize_;
	curveRange = range;
	object = obj;
	color = col;
	hash_cstring hashGen;
	hash = hashGen(propertyName);
	texture = NULL;
}

AnimationCurvePreviewCache::CacheEntry::CacheEntry (const PreviewSize& previewSize_, AnimationCurve* curve, const ColorRGBAf& col, const Rectf& range)
: previewSize(previewSize_)
{
	// we are using memcmp in operator less; clear whole memory of the object (including padding)
	memset (this, 0, sizeof(*this));

	previewSize = previewSize_;
	curveRange = range;
	object = curve;
	color = col;
	hash = 0;
	texture = NULL;
}
