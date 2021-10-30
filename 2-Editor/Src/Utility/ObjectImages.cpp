#include "UnityPrefix.h"
#include "ObjectImages.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/CubemapTexture.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Editor/Src/EditorResources.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Graphics/ProceduralTexture.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/AssetPipeline/DefaultImporter.h"
#include "Editor/Src/Utility/MiniAssetIconCache.h"
#include "Runtime/Camera/Light.h"

// Defined in EditorGUIUtility.txt
int Internal_GetSkinIdx ();

using namespace std;

const char* kResources = "Resources";
const char* kDefaultIconName = "Default Icon";

typedef std::map<string, Image> ScriptImages;
static ScriptImages gScriptImages;

typedef map<string, PPtr<Texture2D> > Textures;
static Textures gNamedTextures;


Texture2D* TextureForScript (MonoScript* script)
{
	Texture2D* texture = dynamic_pptr_cast<Texture2D*> (script->GetIcon ());
	if (texture == NULL)
	{
		const std::string pathName = GetAssetPathFromObject(script);
		string ext = ToLower (GetPathNameExtension(pathName));
		texture = Texture2DNamed (ext + " Script Icon");
	}
	
	
	return texture;
}


Texture2D* FileTextureAtAbsolutePath (const string& path)
{
	std::auto_ptr<FreeImageWrapper> image;
	if( IsFileCreated(path) && LoadImageAtPath (path, image, false) )
	{
		Texture2D* texture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad, kMemTextureCache);
		texture->SetImage (image->GetImage(), Texture2D::kNoMipmap);
		texture->UpdateImageDataDontTouchMipmap ();
		return texture;
	}
	else
		return NULL;
}

string AddImageFileExtensionAutomagic (string path)
{
	if (IsFileCreated(path))
		return path;
	if (IsFileCreated(AppendPathNameExtension(path, "png")))
		return AppendPathNameExtension(path, "png");
	else if (IsFileCreated(AppendPathNameExtension(path, "tif")))
		return AppendPathNameExtension(path, "tif");
	else if (IsFileCreated(AppendPathNameExtension(path, "tiff")))
		return AppendPathNameExtension(path, "tiff");
	else
		return path;
}

// Loads a png icon from the Editor Asset Bundle or
// a texture from the Assets/Gizmos folder.
static Texture2D* Texture2DSkinNamed ( const string& name )
{
	Textures::iterator found = gNamedTextures.find (name);
	if (found == gNamedTextures.end ())
	{
		// Set MasterTextureLimit to 0 (full resolution) while loading GUI textures
		// so e.g scene view icons (with mipmaps) are not scaled down when texture quality settings are low. 
		int prevMasterLimit = Texture::GetMasterTextureLimit ();
		Texture::SetMasterTextureLimit(0, false);

		Texture2D *tex = NULL;
		
		if (IsDeveloperBuild())
		{
			GUIDPersistentManager& pm = GetGUIDPersistentManager ();
			UnityGUID guid;

			// For generated textures with MIPS
			string assetPath = AppendPathNameExtension (AppendPathName ("Assets/Editor Default Resources/Icons/Generated", name), "asset");
			if (pm.PathNameToGUID(assetPath, &guid))
			{	
				const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
				if (asset)
					tex = dynamic_pptr_cast<Texture2D*> (asset->mainRepresentation.object);
			}

			// Check normal icon path
			if (tex == NULL)
			{
				assetPath = AppendPathNameExtension (AppendPathName ("Assets/Editor Default Resources/Icons", name), "png");
				if (pm.PathNameToGUID(assetPath, &guid))
				{	
					const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
					if (asset)
						tex = dynamic_pptr_cast<Texture2D*> (asset->mainRepresentation.object);
				}
			}
		}

		if (tex == NULL)
		{
			// Icons with mip levels
			string iconName = AppendPathNameExtension (AppendPathName (EditorResources::kGeneratedIconsPath, name), "asset");
			tex = GetEditorAssetBundle()->Get<Texture2D> (iconName);
		}

		if (tex == NULL)
		{
			// Normal icons (no mip level)
			string iconName = AppendPathNameExtension (AppendPathName (EditorResources::kIconsPath, name), "png");
			tex = GetEditorAssetBundle()->Get<Texture2D> (iconName);
		}
		
		if (tex == NULL)
		{
			string absolutePath = PathToAbsolutePath (AppendPathName ("Assets/Gizmos", name));
			absolutePath = AddImageFileExtensionAutomagic (absolutePath);

			tex = FileTextureAtAbsolutePath (absolutePath);
		}
		
		if (tex)
		{
			tex->SetIgnoreMasterTextureLimit (true);
			tex->SetHideFlags (Object::kHideAndDontSave);
		}
		
		gNamedTextures[name] = tex;

		Texture::SetMasterTextureLimit(prevMasterLimit, false);

		return tex;
	}
	
	return found->second;
}

Texture2D* Texture2DNamed (const string& name)
{
	Texture2D* tex = NULL;

	if (GetEditorResources ().GetSkinIdx () == 1)
		tex = Texture2DSkinNamed ("d_" + name);

	if (tex == NULL)
		tex = Texture2DSkinNamed (name);

	return tex;
}

Image GetImageNamed (std::string name)
{
	Texture2D* tex = dynamic_pptr_cast<Texture2D*>(Texture2DNamed(name));
	if (tex == NULL)
		return Image();
	else
	{
		Image image (tex->GetDataWidth(), tex->GetDataHeight(), kTexFormatARGB32);
		if (tex->ExtractImage (&image, 0))
			return image;
		else
			return Image ();
	}
}


Image ImageForTexture (int instanceID, int widthHint, int heightHint, int& inoutFlags)
{
	Texture* texture = PPtr<Texture> (instanceID);
	if (!texture)
		return Image();
	
	if (texture->GetDataWidth () == 0 || texture->GetDataHeight () == 0)
		return Image();
	
	if (widthHint == -1 || heightHint == -1)
	{
		widthHint = texture->GetDataWidth ();
		heightHint = texture->GetDataHeight ();
	}
		
	if( inoutFlags == 0 )
	{
		Texture2D* texture2D = dynamic_pptr_cast<Texture2D*>(texture);
		if( texture2D )
		{
			if( IsAlphaOnlyTextureFormat(texture2D->GetTextureFormat()) )
				inoutFlags = kShowAlpha;
			else
				inoutFlags = kShowRGB;
		}
	}
	
	Image image (widthHint, heightHint, kTexFormatARGB32);
	if (texture->ExtractImage (&image))
		return image;
	else
		return Image();
}

Texture* TextureForObject (Object* o)
{
	if (o == NULL)
		return NULL;

	//Custom thumbnail for general textures, but not for cubemaps
	if (dynamic_pptr_cast<Texture2D*> (o) != NULL && dynamic_pptr_cast<Cubemap*> (o) == NULL)
	{
		return dynamic_pptr_cast<Texture2D*> (o);
	}
	if (dynamic_pptr_cast<ProceduralTexture*> (o) != NULL)
	{
		return dynamic_pptr_cast<ProceduralTexture*> (o);
	}

	Texture* icon = NULL;

	if (dynamic_pptr_cast<MonoBehaviour*> (o) != NULL)
	{
		// Custom MonoScript Icons
		MonoBehaviour* behaviour = static_cast<MonoBehaviour*> (o);
		if (MonoScript* monoScript = behaviour->GetScript())
		{
			if(behaviour->IsScriptableObject()) 
			{
				// Avoid TextureForScript's fallback to looking up the script name, as that will incorrectly give a ScriptableObjects the icon from the source file that defines its class (i.e. C#, j, etc)
				icon = dynamic_pptr_cast<Texture2D*> (monoScript->GetIcon ()); 
			}
			else 
			{
				icon = TextureForScript (monoScript);
			}
		}

		// Components (e.g inspector title headers)
		if (icon == NULL && !behaviour->GetScriptClassName().empty())
		{
			icon = Texture2DNamed(Format ("%s Icon", behaviour->GetScriptClassName().c_str()));
			if (icon == NULL)
				icon = Texture2DNamed(kDefaultIconName);
		}

		// Fallback for monobehaviors
		if (icon == NULL)
			icon = Texture2DNamed("ScriptableObject Icon");
	}
	else
	{
		// GameObject Icons (can be custom)
		if (GameObject* gameObject = dynamic_pptr_cast<GameObject*> (o))
			return gameObject->GetIcon ();

		// Custom MonoScript Icons
		if (MonoScript* monoScript = dynamic_pptr_cast<MonoScript*> (o))
			icon = dynamic_pptr_cast<Texture2D*> (monoScript->GetIcon ());
		
		// Light Icons
		if (dynamic_pptr_cast<Light*>(o) != NULL)
		{
			Light* light = static_cast<Light*> (o);
			LightType lightType = light->GetType();
			switch( lightType )
			{
				case kLightSpot:
					icon = Texture2DNamed("Spotlight Icon");
					break;
				case kLightArea:
					icon = Texture2DNamed("AreaLight Icon");
					break;
				case kLightDirectional:
					icon = Texture2DNamed("DirectionalLight Icon");
					break;
				case kLightPoint:
				default:
					// use the light icon
					icon = Texture2DNamed("Light Icon");
					break;
			}
			return icon;
		}
	}

	// Try asset database icons
	if (icon == NULL && IsMainAsset (o->GetInstanceID ()))
		icon = GetCachedAssetDatabaseIcon (GetAssetPathFromObject (o));

	// Fallback
	if (icon == NULL)
		icon = TextureForClass (o->GetClassID ());

	return icon;
}

static map<int, Image> gClassImages;
static map<int, Texture2D*> gClassTextures;
void FlushCachedObjectImages ()
{
	gClassImages.clear ();
	gClassTextures.clear ();
}
 
ImageReference ImageForClass (int classID) {
	map<int, Image>::iterator found = gClassImages.find (classID);
	if (found != gClassImages.end ())
		return found->second;

	string imageName = Object::ClassIDToString (classID) + " Icon";
	
	Image image = GetImageNamed(imageName);
	if (!image.IsValidImage() && classID != ClassID (Object) && Object::ClassIDToRTTI (classID) != NULL)
		image = ImageForClass (Object::GetSuperClassID (classID));

	if (!image.IsValidImage())
		image = GetImageNamed(kDefaultIconName);

	gClassImages[classID] = image;
	return gClassImages[classID];
}

Texture2D* TextureForClass (int classID)
{
	map<int, Texture2D*>::iterator found = gClassTextures.find (classID);
	if (found != gClassTextures.end ())
		return found->second;
	
	Texture2D* tex = NULL;
	if (tex == NULL)
		tex = Texture2DNamed(Format ("%s Icon", Object::ClassIDToString (classID).c_str()));
	
	if (tex == NULL && classID != ClassID (Object) && Object::ClassIDToRTTI (classID) != NULL)
		tex = TextureForClass (Object::GetSuperClassID (classID));

	if (tex == NULL)
		tex = Texture2DNamed(kDefaultIconName);

	gClassTextures[classID] = tex;
	return gClassTextures[classID];
}

void AssetImported (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, string>& moved)
{
	gNamedTextures.clear ();
}

void RegisterAssetImportCallback () 
{
	AssetDatabase::RegisterPostprocessCallback (AssetImported);
}

STARTUP (RegisterAssetImportCallback)	// Call this on startup.
