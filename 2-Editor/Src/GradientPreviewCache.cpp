#include "UnityPrefix.h"
#include "GradientPreviewCache.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Math/Gradient.h"
#include "Runtime/Math/Color.h"
#include "Editor/Src/Utility/SerializedProperty.h"

using namespace std;


static GradientPreviewCache* gGradientPreviewCache = NULL;

void GradientPreviewCache::Cleanup ()
{
	delete gGradientPreviewCache;
	gGradientPreviewCache = NULL;
}

GradientPreviewCache& GradientPreviewCache::Get ()
{
	if (gGradientPreviewCache == NULL) 
		gGradientPreviewCache = new GradientPreviewCache();
	return *gGradientPreviewCache;
}

void GradientPreviewCache::ClearCache ()
{
	for (Entries::iterator i=m_Entries.begin();i != m_Entries.end();i++)
		DestroySingleObject(i->texture);
	
	m_Entries.clear();
}


GradientPreviewCache::GradientPreviewCache ()
{
	GetSceneTracker().AddSceneInspector(this);
}

GradientPreviewCache::~GradientPreviewCache ()
{
	GetSceneTracker().RemoveSceneInspector(this);
	ClearCache();
}


Texture2D* GradientPreviewCache::GetPreview (GradientNEW& gradient)
{
	return GetPreviewInternal(NULL, NULL, gradient);
}

Texture2D* GradientPreviewCache::GetPreview (SerializedProperty& property) 
{ 
	property.SyncSerializedObjectVersion();
	string propertyPath = property.GetPropertyPath();
	Object* target = property.GetSerializedObject()->GetTargetObject();

	Texture2D* tex = LookupCache(target, propertyPath.c_str());
	if (tex)
		return tex;

	GradientNEW* gradient = property.GetGradientValueCopy();
	if (gradient == NULL)
		return NULL;

	tex = GetPreviewInternal(target, propertyPath.c_str(), *gradient);
	delete gradient;

	return tex;
}

Texture2D* GradientPreviewCache::GetPreviewInternal (Object* obj, const char* propertyName, GradientNEW& gradient)
{
	// Find cached preview texture
	Texture2D* preview = NULL;
	if (obj)
		preview = LookupCache (obj, propertyName);
	else
		preview = LookupCache (gradient);
		
	if (preview != NULL)	
		return preview;
	
	// Clear cache whenever we have too many previews. It's very unlikely there are 50 preview textures on screen.
	if (m_Entries.size () > 50)
		ClearCache();
	
	// Generate preview			
	preview = GeneratePreviewTexture(gradient);	
	if (preview == NULL)
		return NULL;
	
	// Inject preview into cache
	if (obj)
	{
		CacheEntry entry (obj, propertyName);
		entry.texture = preview;
		bool didInsert = m_Entries.insert(entry).second;
		Assert(didInsert);
	}
	else
	{
		CacheEntry entry (&gradient);
		entry.texture = preview;
		bool didInsert = m_Entries.insert(entry).second;
		Assert(didInsert);
	}
	
	return preview;
}

Texture2D* GradientPreviewCache::LookupCache (GradientNEW& gradient)
{
	CacheEntry entry (&gradient);
	Entries::iterator result = m_Entries.find(entry);
	if (result != m_Entries.end())
		return result->texture;
	else
		return NULL;
}

Texture2D* GradientPreviewCache::LookupCache (Object* obj, const char* propertyName)
{
	CacheEntry entry (obj, propertyName);
	Entries::iterator result = m_Entries.find(entry);
	if (result != m_Entries.end())
		return result->texture;
	else
		return NULL;
}

Texture2D* GradientPreviewCache::GeneratePreviewTexture (GradientNEW& gradient)
{
	// Fixed size previews
	const int width = 256;
	const int height = 2;

	Texture2D* texture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad, kMemTextureCache);
	texture->SetHideFlags(Object::kHideAndDontSave);
	texture->InitTexture (width, height, kTexFormatRGBA32, Texture2D::kNoMipmap, 1);
	
	ImageReference image;
	texture->GetWriteImageReference(&image, 0, 0);
	ColorRGBA32* pixel;
	for (int x=0;x<width;x++)
	{
		ColorRGBA32 color = ColorRGBA32(gradient.Evaluate(x / (float)width));

		// Set both rows 
		pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(0)) + x;
		*pixel = color;
		
		pixel = reinterpret_cast<ColorRGBA32*> (image.GetRowPtr(1)) + x;
		*pixel = color;
	}
	texture->UpdateImageDataDontTouchMipmap();
	return texture;
}

void GradientPreviewCache::SelectionHasChanged (const std::set<int>& selection)
{
	GradientPreviewCache::Get().ClearCache();
}

void GradientPreviewCache::ForceReloadInspector (bool fullRebuild)
{
	GradientPreviewCache::Get().ClearCache();
}

GradientPreviewCache::CacheEntry::CacheEntry (void* obj, const char* propertyName)
:	object(obj)
{
	hash_cstring hashGen;
	hash = hashGen(propertyName);
}

GradientPreviewCache::CacheEntry::CacheEntry (GradientNEW* gradient)
:	object(gradient), hash()
{
}
