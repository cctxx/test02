#include "UnityPrefix.h"
#include "SpritePacker.h"

#if ENABLE_SPRITES

#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetDatabaseStructs.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/AssetPipeline/ImageOperations.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/SpritePacker/SpritePackerBlockMask.h"
#include "Editor/Platform/Interface/EditorUtility.h"

#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Math/Random/Rand.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Graphics/SpriteUtility.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Utilities/BitUtility.h"

// Debug features
#define SPRITEPACKER_DEBUG_LOG 1
#if (SPRITEPACKER_DEBUG_LOG)
	#define SPRITEPACKER_LOG(...) printf_console(__VA_ARGS__)
#else
	#define SPRITEPACKER_LOG(...)
#endif

static const int kSpritePackerCacheVersion = 2;

struct SpritePackingData
{
	SpritePackingMode     mode;
	SpritePackingRotation rotation;
};

struct SpriteAtlasData
{
	int               format;
	TextureUsageMode  usageMode;
	TextureColorSpace colorSpace;
	int               compressionQuality;
	TextureFilterMode filterMode;
	int               maxWidth;
	int               maxHeight;

	std::string ToString() const
	{
		return Format("Format: %s, Usage: %d, Color Space: %s, Compression: %d, Filter Mode: %d, Max Size: %dx%d", GetTextureFormatString(format), usageMode, GetTextureColorSpaceString(colorSpace), compressionQuality, filterMode, maxWidth, maxHeight);
	}
};

struct SpriteSetSorter
{
	bool operator() (const Sprite* a, const Sprite* b) const
	{
		const Vector2f aSize = a->GetSize();
		const Vector2f bSize = b->GetSize();
		const float aArea = aSize.x * aSize.y;
		const float bArea = bSize.x * bSize.y;
		if (aArea != bArea)
			return (aArea > bArea); // Largest to smallest

		return (a->GetInstanceID() < b->GetInstanceID());
	}
};

typedef std::set<Sprite*, SpriteSetSorter> TSpriteSet;
typedef std::map<Sprite*, SpritePackingData, SpriteSetSorter> TSpriteMap;

class PackingProgress;
struct Atlas
{
	std::string                    name;
	SpriteAtlasData                settings;
	TSpriteMap                     sprites;
	PPtr<CachedSpriteAtlas>        atlas;

	// Generated
	MdFour                         hash;
	MdFour                         hashForPacking;

	void CalculateHash(const char* policyVersionId);	
	bool Pack(PackingProgress& progress);
};

typedef std::map<std::string, Atlas> TAtlasList;

const char* const kRepackingSpriteAtlasTitle = "Repacking sprite atlases";
class PackingProgress
{
public:
	PackingProgress(bool display)
	: m_Display(display)
	, m_UserWantsToCancel(false)
	, m_TotalAtlases(0)
	, m_TotalSprites(0)
	, m_CurAtlas(0)
	, m_CurSprite(0)
	{
	}

	void IncludeAtlas(const Atlas& atlas)
	{
		++m_TotalAtlases;
		m_TotalSprites += atlas.sprites.size();
	}

	void NextAtlas() { ++m_CurAtlas; }
	void NextSprite() { ++m_CurSprite; }

	bool IsShown() const { return m_Display && (m_TotalSprites > 0); }
	bool UserWantsToCancel() const { return m_UserWantsToCancel; }

	ProgressBarState Show()
	{
		if (!IsShown())
			return kPBSNone;

		std::string message = Format("Fitting sprite %d out of %d to atlas %d out of %d.", m_CurSprite+1, m_TotalSprites, m_CurAtlas+1, m_TotalAtlases);
		const float spriteProgress = (float)m_CurSprite / m_TotalSprites;
		const float totalProgress =  (float)m_CurAtlas / m_TotalAtlases + (1.0f / m_TotalAtlases) * spriteProgress;
		ProgressBarState state = DisplayProgressbar(kRepackingSpriteAtlasTitle, message, totalProgress, true);
		if (state == kPBSWantsToCancel)
			m_UserWantsToCancel = true;
		return state;
	}

private:
	const bool m_Display;
	bool m_UserWantsToCancel;
	int m_TotalAtlases;
	int m_TotalSprites;
	int m_CurAtlas;
	int m_CurSprite;
};

bool PackSprites(Rand& rnd, const Atlas& group, TTextureList& outTextures, TCachedRenderDataMap& outRDs, PackingProgress& progress);
Texture2D* CreateAtlasTexture(const Atlas& group, int textureCacheIndex, int atlasSizeX, int atlasSizeY, const std::vector<BlockMask>& atlasMasks, const std::vector<BlockMask>& sourceMasks, const TSpriteMap& sprites, int border);

bool Atlas::Pack(PackingProgress& progress)
{
	Assert(atlas.IsNull());
	atlas = CreateObjectFromCode<CachedSpriteAtlas>();

	Rand rnd(hashForPacking.PackToUInt32());

	SPRITEPACKER_LOG("- Packing atlas (%s) of %d sprites. Settings: %s.\n", name.c_str(), sprites.size(), settings.ToString().c_str());

	TTextureList textures;
	TCachedRenderDataMap frames;
	PackSprites(rnd, *this, textures, frames, progress);
	if (progress.UserWantsToCancel())
		return false;

	for (TTextureList::iterator tit = textures.begin(); tit != textures.end(); ++tit)
		atlas->textures.push_back(*tit);
	
	for (TCachedRenderDataMap::iterator fit = frames.begin(); fit != frames.end(); ++fit)
		atlas->frames[fit->first] = fit->second;
	
	progress.NextAtlas();
	return true;
}

void Atlas::CalculateHash (const char* policyVersionId)
{
	MdFourGenerator md4;
	MdFourGenerator md4packing; // Only width/height of sprites

	md4.Feed(kSpritePackerCacheVersion);
	md4.Feed(policyVersionId);
	md4.Feed(name);
	md4.Feed((const char*)&settings, sizeof(SpriteAtlasData));
	for (TSpriteMap::const_iterator it = sprites.begin(); it != sprites.end(); ++it)
	{
		Sprite* sprite = (*it).first;
		AssetImporter* importer = FindAssetImporterAtAssetPath(GetAssetPathFromInstanceID(sprite->GetInstanceID()));
		const Asset* asset = AssetDatabase::Get().AssetPtrFromPath(importer->GetAssetPathName());
		const MdFour& hash = asset->hash;
		md4.Feed((const char*)&hash.bytes[0], sizeof(hash.bytes));

		const SpritePackingData& data = (*it).second;
		md4.Feed((const char*)&data, sizeof(SpritePackingData));

		md4packing.Feed(sprite->GetRect().width);
		md4packing.Feed(sprite->GetRect().height);
	}

	hash = md4.Finish();
	hashForPacking = md4packing.Finish();
}

typedef std::vector<Sprite*> TSpriteList;
typedef std::vector<TextureImporter*> TImporterList;


///------------------------------------------------
///------------[ Sprite grouping ]-----------------
///------------------------------------------------
static TAtlasList s_activeSpritePackerJob;
static BuildTargetPlatform s_ActiveSpritePackerPlatform;

bool GroupSprites(BuildTargetPlatform platform, const TImporterList& textureImporters, TAtlasList& atlases, const int qualifiedSpriteCount)
{
	s_activeSpritePackerJob.clear();
	s_ActiveSpritePackerPlatform = platform;

	SPRITEPACKER_LOG("- Grouping sprites using the selected SpritePackerPolicy.\n");

	// Execute packing policy
	void* params[] = { &platform, CreateScriptingArrayFromUnityObjects(textureImporters, ClassID(TextureImporter)) };
	MonoException* exception = NULL;
	CallStaticMonoMethod ("Packer", "UnityEditor.Sprites", "ExecuteSelectedPolicy", params, &exception);
	if (exception)
	{
		Scripting::LogException(exception, 0, "Failed executing selected SpritePackerPolicy.");
		s_activeSpritePackerJob.clear();
		return false;
	}

	atlases = s_activeSpritePackerJob;
	s_activeSpritePackerJob.clear();

	// Validate assignments.
	{
		// Each sprite must only be assigned to one atlas.
		TSpriteSet checkedSprites;
		TAtlasList::iterator it = atlases.begin();
		while (it != atlases.end())
		{
			Atlas& atlas = it->second;
			if (atlas.sprites.size() == 0)
			{
				SPRITEPACKER_LOG("- \tAtlas \"%s\" does not have any sprites assigned. Ignoring.\n", atlas.name.c_str());
				TAtlasList::iterator eraseIt = it++;
				atlases.erase(eraseIt);
				continue;
			}
			else ++it;

			SPRITEPACKER_LOG("- \tAtlas \"%s\" with %d sprites assigned.\n", atlas.name.c_str(), atlas.sprites.size());
			for (TSpriteMap::iterator sit = atlas.sprites.begin(); sit != atlas.sprites.end(); ++sit)
			{
				Sprite* sprite = (*sit).first;
				if (checkedSprites.find(sprite) != checkedSprites.end())
				{
					ErrorStringMsg("Error, Sprite \"%s\" assigned to multiple atlases.\n", sprite->GetName());
					return false;
				}
				else
					checkedSprites.insert(sprite);
			}
		}

		// Check if all sprites are assigned.
		#if SPRITEPACKER_DEBUG_LOG
		if (checkedSprites.size() < qualifiedSpriteCount)
			SPRITEPACKER_LOG("- Warning. Assigned %d sprites out of %d. Unassigned sprites will reference uncompressed textures.\n", checkedSprites.size(), qualifiedSpriteCount);
		#endif
	}

	return true;
}

void SpritePacker::ActiveJob_AddAtlas(std::string atlasName, const SpritePacker::SpriteAtlasSettings& settings)
{
	const int kMinSize = 64;

	if (settings.format == 0)
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" texture format is not set.", atlasName.c_str());
		return;
	}

	if (!TextureImporter::IsSupportedTextureFormat(settings.format, s_ActiveSpritePackerPlatform))
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" format \"%s\" is not valid for \"%s\".",
			atlasName.c_str(),
			GetTextureFormatString(settings.format),
			GetBuildTargetGroupName(s_ActiveSpritePackerPlatform, false).c_str());
		return;
	}

	if (settings.maxWidth < kMinSize)
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" width must be at least %d.", atlasName.c_str(), kMinSize);
		return;
	}

	if (settings.maxHeight < kMinSize)
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" height must be at least %d.", atlasName.c_str(), kMinSize);
		return;
	}

	if (s_activeSpritePackerJob.find(atlasName) != s_activeSpritePackerJob.end())
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" is already added.", atlasName.c_str());
		return;
	}

	Atlas& atlas = s_activeSpritePackerJob[atlasName];
	atlas.name = atlasName;
	atlas.settings.colorSpace = ColorSpaceToTextureColorSpace(s_ActiveSpritePackerPlatform, settings.colorSpace);
	atlas.settings.compressionQuality = settings.compressionQuality;
	atlas.settings.filterMode = settings.filterMode;
	atlas.settings.format = settings.format;
	atlas.settings.usageMode = settings.usageMode;
	atlas.settings.maxWidth = settings.maxWidth;
	atlas.settings.maxHeight = settings.maxHeight;
}

void SpritePacker::ActiveJob_AssignToAtlas(std::string atlasName, Sprite* sprite, SpritePackingMode packingMode, SpritePackingRotation packingRotation)
{
	if (sprite == NULL)
	{
		Scripting::RaiseNullException("Can not add a NULL sprite.");
		return;
	}
	TAtlasList::iterator atlasIt = s_activeSpritePackerJob.find(atlasName);
	if (atlasIt == s_activeSpritePackerJob.end())
	{
		Scripting::RaiseOutOfRangeException("Atlas \"%s\" does not exist. To create it call AddAtlas(\"%s\", settings).", atlasName.c_str(), atlasName.c_str());
		return;
	}

	Atlas& atlas = atlasIt->second;
	if (atlas.sprites.find(sprite) != atlas.sprites.end())
	{
		Scripting::RaiseArgumentException("Atlas \"%s\" already contains sprite \"%s\".", atlasName.c_str(), sprite->GetName());
		return;
	}

	SpritePackingData data;
	data.mode = packingMode;
	data.rotation = packingRotation;
	atlas.sprites.insert(std::make_pair(sprite, data));
}



///-------------------------------------------
///------------[ Main logic ]-----------------
///-------------------------------------------
void CollectOutputTextures (const Atlas& atlas, TTextureList& output)
{
	output.insert(output.end(), atlas.atlas->textures.begin(), atlas.atlas->textures.end());
}

static void RefreshSpriteRDForSprites(TAtlasList& atlases)
{
	for (TAtlasList::iterator ait = atlases.begin(); ait != atlases.end(); ++ait)
	{
		Atlas& atlas = ait->second;
		for (TSpriteMap::iterator it = atlas.sprites.begin(); it != atlas.sprites.end(); ++it)
		{
			Sprite* frame = (*it).first;
			frame->RefreshAtlasRD();
		}
	}
}

static int ClearSpriteRDForSprites(TextureImporter* ti)
{
	int count = 0;

	std::vector<Object*> objects = FindAllAssetsAtPath(ti->GetAssetPathName(), ClassID(Sprite));
	for (std::vector<Object*>::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		Sprite* sprite = dynamic_pptr_cast<Sprite*>(*it);
		if (sprite)
		{
			sprite->ClearAtlasRD();
			++count;
		}
	}
	
	return count;
}

static bool RebuildReferencedAtlasGroupsIfNeededGeneric (BuildTargetPlatform platform, TAtlasList& allAtlases, TTextureList& output, bool displayProgressBar = false)
{
	SPRITEPACKER_LOG("- Packing %d atlases.\n", allAtlases.size());
	PackingProgress progress(displayProgressBar);

	// Find dirty atlases. Load/map clean ones.
	std::map<std::string, Atlas*> dirtyAtlases;
	for (TAtlasList::iterator aIt = allAtlases.begin(); aIt != allAtlases.end(); ++aIt)
	{
		Atlas& atlas = aIt->second;
		SInt32 instanceID = SpriteAtlasCache::Get().LoadAndGetInstanceID(atlas.hash);
		if (instanceID == 0)
		{
			dirtyAtlases.insert(std::make_pair(aIt->first, &aIt->second));
			progress.IncludeAtlas(atlas);
			SPRITEPACKER_LOG("- Atlas (%s) file (%s) not found. Will repack.\n", atlas.name.c_str(), MdFourToString(atlas.hash).c_str());
		}
		else
		{
			atlas.atlas = PPtr<CachedSpriteAtlas>(instanceID);
			SpriteAtlasCache::Get().Map(atlas.name, atlas.hash);
			SPRITEPACKER_LOG("- Atlas (%s) file (%s) found. Mapping.\n", atlas.name.c_str(), MdFourToString(atlas.hash).c_str());
		}
	}

	// Pack dirty atlases
	for (std::map<std::string, Atlas*>::iterator aIt = dirtyAtlases.begin(); aIt != dirtyAtlases.end(); ++aIt)
	{
		Atlas& atlas = *aIt->second;
		if (!atlas.Pack(progress))
		{
			Assert(progress.UserWantsToCancel());
			SpriteAtlasCache::Get().UnMapAll(); // Unmap Sprite cache completely to prevent any atlasing
			ClearProgressbar();
			SPRITEPACKER_LOG("- Packing canceled by user.\n");
			return false;
		}
	}

	// Store dirty atlases in cache
	// Note: this will override existing hint-to-hash mappings
	for (std::map<std::string, Atlas*>::iterator aIt = dirtyAtlases.begin(); aIt != dirtyAtlases.end(); ++aIt)
	{
		Atlas& atlas = *aIt->second;
		SpriteAtlasCache::Get().Store(atlas.name, atlas.hash, atlas.atlas);
	}

	// Assign references
	if (progress.IsShown())
		DisplayProgressbar(kRepackingSpriteAtlasTitle, "Assigning cache references", 0.99f);
	RefreshSpriteRDForSprites(allAtlases);
	
	if (progress.IsShown())
		ClearProgressbar();

	// Collect output textures
	for (TAtlasList::iterator aIt = allAtlases.begin(); aIt != allAtlases.end(); ++aIt)
	{
		Atlas& atlas = aIt->second;
		CollectOutputTextures(atlas, output);
	}

	SPRITEPACKER_LOG("- Packing completed.\n");
	return true;
}

static bool GetSelectedPolicyId(std::string& id)
{
	MonoException* exception = NULL;
	MonoObject* result = CallStaticMonoMethod ("Packer", "UnityEditor.Sprites", "GetSelectedPolicyId", NULL, &exception);
	if (exception)
	{
		Scripting::LogException(exception, 0, "Failed getting selected SpritePackerPolicy version.");
		return false;
	}
	
	id = MonoStringToCppChecked(result);
	return true;
}

static MdFour s_LastSpritePackerJobHash;
bool SpritePacker::RebuildAtlasCacheIfNeeded (BuildTargetPlatform platform, bool displayProgressBar, SpritePackerExecution execution, bool enteringPlayMode)
{
	if (!GetBuildSettings().hasAdvancedVersion)
		return true;

	SPRITEPACKER_LOG("\nPacking sprites:\n");
	if (GetEditorSettings().GetSpritePackerMode() == EditorSettings::kSPOff)
	{
		SPRITEPACKER_LOG("- Skipping packing - Sprite Packer is off.\n");
		return true;
	}
	else if (GetEditorSettings().GetSpritePackerMode() == EditorSettings::kSPBuild && enteringPlayMode)
	{
		SPRITEPACKER_LOG("- Skipping packing for Play mode - Sprite Packer is in 'Enabled For Builds' mode.\n");
		return true;
	}

	ABSOLUTE_TIME prepareTime = START_TIME;

	MonoException* exception = NULL;
	CallStaticMonoMethod("Packer", "UnityEditor.Sprites", "SaveUnappliedTextureImporterSettings");
	if (exception)
	{
		Scripting::LogException(exception, 0, "Failed saving unapplied TextureImporter settings.");
		return false;
	}

	AssetDatabase& adb = AssetDatabase::Get();
	std::set<UnityGUID> allTextureAssets;
	adb.FindAssetsWithImporterClassID(ClassID(TextureImporter), allTextureAssets);

	// Grab policy version hash
	std::string policyVersionId;
	if (!GetSelectedPolicyId(policyVersionId))
		return false;

	// Check if texture assets or policy have changed
	MdFourGenerator jobHashGen;
	jobHashGen.Feed((UInt32)platform);
	jobHashGen.Feed(policyVersionId.c_str());
	for (std::set<UnityGUID>::iterator it = allTextureAssets.begin(); it != allTextureAssets.end(); ++it)
	{
		const Asset& asset = AssetDatabase::Get().AssetFromGUID(*it);
		jobHashGen.Feed((const char*)&asset.hash.bytes[0], 16);
	}
	const MdFour jobHash = jobHashGen.Finish();
	if (s_LastSpritePackerJobHash == jobHash)
	{
		prepareTime = ELAPSED_TIME(prepareTime);
		SPRITEPACKER_LOG("- Repacking is not required. Check took %f ms.\n", AbsoluteTimeToMilliseconds(prepareTime));
		if (execution == kSPE_Normal)
			return true;
		else
			SPRITEPACKER_LOG("-- However user forced a repack. Continuing.\n");
	}
	else
		s_LastSpritePackerJobHash = jobHash;
	
	

	// Group texture importers
	TImporterList textureImporters;
	int qualifiedSpriteCount = 0;
	for (std::set<UnityGUID>::iterator it = allTextureAssets.begin(); it != allTextureAssets.end(); ++it)
	{
		const UnityStr& path = GetAssetPathFromGUID(*it);
		TextureImporter* ti = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterAtAssetPath(path));
		if (!ti)
			continue;

		if (ti->GetQualifiesForSpritePacking())
		{
			textureImporters.push_back(ti);
			if (TextureImporter::DoesAssetNeedReimport(path, BuildTargetSelection(platform, 0), false))
			{
				SPRITEPACKER_LOG("- Importing %s.\n", path.c_str());
				AssetInterface::Get().ImportAtPath(path);
			}
			qualifiedSpriteCount += ClearSpriteRDForSprites(ti); // Clear before packing to end up in a perfect state
		}
	}
	
	TAtlasList allAtlases;
	if (!GroupSprites(platform, textureImporters, allAtlases, qualifiedSpriteCount))
		return false;

	
	
	// Calculate atlas hashes
	for (TAtlasList::iterator it = allAtlases.begin(); it != allAtlases.end(); ++it)
	{
		Atlas& atlas = it->second;
		atlas.CalculateHash(policyVersionId.c_str()); //Note: add platform to the hash if platform-specific atlases are needed in the future.
	}



	// Rebuild
	SpriteAtlasCache::Get().UnMapAll(); // Unmap Sprite cache completely - new cache will be created
	
	TTextureList output;
	return RebuildReferencedAtlasGroupsIfNeededGeneric(platform, allAtlases, output, displayProgressBar);
}

const SpriteRenderData* SpritePacker::GetPackedSpriteRD (const Sprite& sprite, UnityStr& outAtlasName)
{
	UnityGUID guid;
	GetGUIDPersistentManager().PathNameToGUID(GetAssetPathFromInstanceID(sprite.GetInstanceID()), &guid);
	LocalIdentifierInFileType localId = sprite.GetFileIDHint();
	const SpriteRenderData* rd = SpriteAtlasCache::Get().FindSpriteRenderDataMRU(guid, localId, outAtlasName);
	return rd;
}



///-----------------------------------------------------
///------------[ Actual packing logic ]-----------------
///-----------------------------------------------------

static bool FitBlockMaskInBlockMaskUsingPadding (Rand& rnd, const BlockMask& src, const BlockMask& dst, UInt32& outX, UInt32& outY, const bool square)
{
	const int kIterations = 50;
	bool success = false;
	//TODO: rotations (4.5).

	// Try N random positions without growing <dst> past a pow2 boundary unless the <src> is larger itself.
	int limitX = std::max( NextPowerOfTwo(dst.fillMaxX + 1), NextPowerOfTwo(src.GetRawWidth()) );
	int limitY = std::max( NextPowerOfTwo(dst.fillMaxY + 1), NextPowerOfTwo(src.GetRawHeight()) );
	int bestX, bestY;
	do 
	{
		if (square)
			limitX = limitY = std::max(limitX, limitY);
		
		bestX = limitX - src.GetRawWidth();
		bestY = limitY - src.GetRawHeight();
		for (int a = 0; a < kIterations; a++)
		{
			int x = RangedRandom(rnd, 0, bestX);
			int y = RangedRandom(rnd, 0, bestY);
			if (!src.OverlapsBlockMaskPadded(dst, x, y))
			{
				bestX = x;
				bestY = y;
				success = true;
			}
		}
		// Worst-case attempt
		if (!success)
		{
			if (!src.OverlapsBlockMaskPadded(dst, bestX, bestY))
			{
				success = true;
			}
		}

		// If couldn't fit, grow the boundary.
		if (!success)
		{
			const bool canGrowX = (limitX < dst.width);
			const bool canGrowY = (limitY < dst.height);
			if (!canGrowX && !canGrowY)
				break; // Won't fit

			bool growX = canGrowX;
			if (!square)
			{
				if (limitX == limitY)
					// Grow based on <src> size. If <src> w<h, expand horizontally.
					growX &= (src.GetRawWidth() <= src.GetRawHeight() || !canGrowY);
				else
					// Grow smaller side first
					growX &= (limitX < limitY || !canGrowY);
			}
			
			if (growX)
				limitX = std::min(NextPowerOfTwo(limitX + 1), dst.width);
			else
				limitY = std::min(NextPowerOfTwo(limitY + 1), dst.height);
		}
	}
	while (!success);

	// Narrow target rect more toward low-left corner on success.
	bool changed = success;
	while (changed)
	{
		changed = false;
		if (!src.OverlapsBlockMaskPadded(dst, bestX-1, bestY-1))
		{
			changed = true;
			bestX--;
			bestY--;
		} 
		else if (!src.OverlapsBlockMaskPadded(dst, bestX-1, bestY))
		{
			changed = true;
			bestX--;
		}
		else if (!src.OverlapsBlockMaskPadded(dst, bestX, bestY-1))
		{
			changed = true;
			bestY--;
		}
	}

	if (success)
	{
		outX = bestX;
		outY = bestY;
		return true;
	}

	return false;
}

static void TransferPixelDataForMask (const BlockMask& src, const SpriteRenderData& rd, ColorRGBA32* dest, int destWidth, int destHeight, int border)
{
	const UInt32 srcTexWidth = rd.texture->GetGLWidth();
	const UInt32 srcTexHeight = rd.texture->GetGLHeight();
	
	ColorRGBA32* srcColors;
	ALLOC_TEMP(srcColors, ColorRGBA32, srcTexWidth * srcTexHeight);
	rd.texture->GetPixels32(0, srcColors);

	for (UInt32 y = 0; y < src.GetRawHeight(); ++y)
	{
		for (UInt32 x = 0; x < src.GetRawWidth(); ++x)
		{
			const UInt32 srcPixelX = src.srcX + x;
			const UInt32 srcPixelY = src.srcY + y;
			const UInt32 dstPixelX = src.fitX + x;
			const UInt32 dstPixelY = src.fitY + y;

			if (src.bits.test(x + y * src.width))
				dest[dstPixelY * destWidth + dstPixelX] = srcColors[srcPixelY * srcTexWidth + srcPixelX];
		}
	}

	// Borders
	for (int b = 0; b < border; ++b)
	{
		// Vertical
		{
			const int minY = std::max<int>((int)src.fitY - (b), 0);
			const int maxY = std::min<int>((int)src.fitY + src.GetRawHeight() - 1 + (b), destHeight);
			// Left
			{
				const int srcX = (int)src.fitX;
				const int dstX = srcX - (b + 1);
				if (dstX >= 0)
					for (int y = minY; y <= maxY; ++y)
						dest[y * destWidth + dstX] = dest[y * destWidth + srcX];
			}
			// Right
			{
				const int srcX = (int)(src.fitX + src.GetRawWidth() - 1);
				const int dstX = srcX + (b + 1);
				if (dstX < destWidth)
					for (int y = minY; y <= maxY; ++y)
						dest[y * destWidth + dstX] = dest[y * destWidth + srcX];
			}
		}

		// Horizontal (1 pixel wider initially due to vertical borders being done at this point)
		{
			const int minX = std::max<int>((int)src.fitX - (b + 1), 0);
			const int maxX = std::min<int>((int)src.fitX + src.GetRawWidth() - 1 + (b + 1), destWidth);
			// Bottom
			{
				const int srcY = (int)src.fitY;
				const int dstY = srcY - (b + 1);
				if (dstY >= 0)
					for (int x = minX; x <= maxX; ++x)
						dest[dstY * destWidth + x] = dest[srcY * destWidth + x];
			}
			// Top
			{
				const int srcY = (int)(src.fitY + src.GetRawHeight() - 1);
				const int dstY = srcY + (b + 1);
				if (dstY < destHeight)
					for (int x = minX; x <= maxX; ++x)
						dest[dstY * destWidth + x] = dest[srcY * destWidth + x];
			}
		}

	}

	//TODO: rotation (4.5)
}

bool PackSprites(Rand& rnd, const Atlas& atlas, TTextureList& outTextures, TCachedRenderDataMap& outRDs, PackingProgress& progress)
{
	const TSpriteMap& sprites = atlas.sprites;

	std::vector<BlockMask> atlasMasks;
	std::vector<BlockMask> sourceMasks;
	sourceMasks.reserve(sprites.size());

	const int padding = 4; //TODO: decide on pad size based on what?
	const bool square = TextureImporter::DoesTextureFormatRequireSquareTexture(atlas.settings.format);

	for (TSpriteMap::const_iterator it = sprites.begin(); it != sprites.end(); ++it)
	{
		progress.Show();
		if (progress.UserWantsToCancel())
			return false;
		progress.NextSprite();

		const Sprite* frame = (*it).first;
		const SpriteRenderData& rd = frame->GetRenderData(false); // Use non-atlased RenderData as input.
		const SpritePackingData& data = (*it).second;
		
		// Adjust rect to exact texel boundaries on a grid (source texture space). That's what we're packing.
		int srcMinX, srcMinY, srcMaxX, srcMaxY;
		GetSpriteMeshRectPixelBounds(*rd.texture, rd.textureRect, srcMinX, srcMinY, srcMaxX, srcMaxY);
		
		// *. Build a block mask (source texture space).
		BlockMask srcMask(srcMaxX - srcMinX, srcMaxY - srcMinY, padding);
		srcMask.srcX = srcMinX;
		srcMask.srcY = srcMinY;
		srcMask.FillSprite(*frame, data.mode == kSPMRectangle);
		srcMask.GeneratePadding();

		// *. Iterate existing atlas masks and try to fit the sprite mask. If it won't fit, create a new atlas mask.
		//    Atlas mask is a block mask (destination texture space).
		int fitsIn = -1;
		UInt32 fitX;
		UInt32 fitY;
		for (int i = atlasMasks.size() - 1; i >= 0; --i) // Search for atlases backwards to minimize chances of failing the fit in an already full atlas.
		{
			if (FitBlockMaskInBlockMaskUsingPadding (rnd, srcMask, atlasMasks[i], fitX, fitY, square))
			{
				fitsIn = i;
				break;
			}
		}

		if (fitsIn == -1)
		{
			BlockMask newAtlasMask(atlas.settings.maxWidth, atlas.settings.maxHeight, 0);
			atlasMasks.push_back(newAtlasMask);

			bool fits = FitBlockMaskInBlockMaskUsingPadding (rnd, srcMask, newAtlasMask, fitX, fitY, square); //TODO:this should probably be a faster method with no randomization
			Assert(fits);
			fitsIn = atlasMasks.size() - 1;
		}

		// *. Mark blocks as used in destination atlas mask.
		atlasMasks[fitsIn].BurnInBlockMaskPadded (srcMask, fitX, fitY);

		// *. Register location
		srcMask.fitIn = fitsIn;
		srcMask.fitX = fitX;
		srcMask.fitY = fitY;
		sourceMasks.push_back(srcMask);
		// TODO: rotation (4.5)

		SPRITEPACKER_LOG("- \tPlaced sprite in page (%3d) at (%4d, %4d). Page is now (%4d, %4d). Sprite: \"%s\".\n", fitsIn, fitX, fitY, atlasMasks[fitsIn].fillMaxX+1, atlasMasks[fitsIn].fillMaxY+1, frame->GetName());
	}

	// *. Once all locations are registered, transfer pixels.
	outTextures.resize(atlasMasks.size());
	for (int i = 0; i < outTextures.size(); ++i)
	{
		// *. Find the minimum possible texture size that is pow2.
		int atlasSizeX = 32;
		int atlasSizeY = 32;
		for (std::vector<BlockMask>::const_iterator sm = sourceMasks.begin(); sm != sourceMasks.end(); ++sm)
		{
			const BlockMask& bm = *sm;
			if (bm.fitIn == i)
			{
				// "fit + fillMax + 1" will only go up to the last valid pixel.
				atlasSizeX = std::max<int>(atlasSizeX, bm.fitX + bm.fillMaxX + 1);
				atlasSizeY = std::max<int>(atlasSizeY, bm.fitY + bm.fillMaxY + 1);
			}
		}

		if (square)
			atlasSizeX = atlasSizeY = std::max(atlasSizeX, atlasSizeY);

		atlasSizeX = NextPowerOfTwo(atlasSizeX);
		atlasSizeY = NextPowerOfTwo(atlasSizeY);
		if (atlasSizeX != atlas.settings.maxWidth || atlasSizeY != atlas.settings.maxHeight)
		{
			SPRITEPACKER_LOG("- \tPage (%3d) downsized to : (%4d x %4d).\n", i, atlasSizeX, atlasSizeY);
		}

		outTextures[i] = CreateAtlasTexture(atlas, i, atlasSizeX, atlasSizeY, atlasMasks, sourceMasks, sprites, padding / 2);
	}

	// *. Setup new RenderDatas
	outRDs.clear();

	int i = 0;
	for (TSpriteMap::const_iterator it = sprites.begin(); it != sprites.end(); ++it, ++i)
	{
		const BlockMask& srcMask = sourceMasks[i];
		const Sprite* frame = (*it).first;
		const SpritePackingData& packingData = (*it).second;
		const Texture2D* texture = outTextures[srcMask.fitIn];
		const SpriteRenderData& rd = frame->GetRenderData(false); // Use non-atlased RenderData as input.

		// Register
		TCachedRenderDataKey key;
		GetGUIDPersistentManager().PathNameToGUID(GetAssetPathFromInstanceID(frame->GetInstanceID()), &key.first);
		key.second = frame->GetFileIDHint();
		SpriteRenderData& cache = outRDs.insert(make_pair(key, SpriteRenderData())).first->second;
		cache.texture = outTextures[srcMask.fitIn];
		cache.textureRect = rd.textureRect;
		cache.textureRectOffset = rd.textureRectOffset;

		// New render data
		// Adjust from expanded block space to proper pixel space.
		Vector2f meshPos = rd.textureRect.GetPosition();
		const float adjustmentX = meshPos.x - srcMask.srcX;
		const float adjustmentY = meshPos.y - srcMask.srcY;
		Rectf newRect(
			srcMask.fitX + adjustmentX,
			srcMask.fitY + adjustmentY,
			rd.textureRect.width,
			rd.textureRect.height
		);
		cache.textureRect.x = newRect.x;
		cache.textureRect.y = newRect.y;

		#if 0 //TODO: Figure out how to fix subpixel errors
		Assert((int)newRect.x == srcMask.fillMinX + srcMask.fitX);
		Assert((int)newRect.y == srcMask.fillMinY + srcMask.fitY);
		Assert((int)(rd.meshRect.width+kMaxError) == srcMask.fillMaxX - srcMask.fillMinX + 1);
		Assert((int)(rd.meshRect.height+kMaxError) == srcMask.fillMaxY - srcMask.fillMinY + 1);
		#endif

		Vector2f meshRectCenter = rd.textureRect.GetCenterPos();
		Vector2f meshCenterOffset = meshRectCenter - frame->GetRect().GetCenterPos();
		Vector2f newOffset = frame->GetOffset() + meshCenterOffset;

		if (!srcMask.rectFill)
		{
			cache.indices.assign(rd.indices.begin(), rd.indices.end());
			cache.vertices.reserve(rd.vertices.size());
			for (std::vector<SpriteVertex>::const_iterator it = rd.vertices.begin(); it != rd.vertices.end(); ++it)
			{
				const SpriteVertex& vertex = *it;
				SpriteVertex newVertex;
				newVertex.pos = vertex.pos;
			
				float u = (vertex.uv.x * rd.texture->GetGLWidth() + srcMask.fitX - srcMask.srcX) / texture->GetGLWidth();
				float v = (vertex.uv.y * rd.texture->GetGLHeight() + srcMask.fitY - srcMask.srcY) / texture->GetGLHeight();
				newVertex.uv = Vector2f(u, v);

				cache.vertices.push_back(newVertex);
			}
		}
		else 
		{
			cache.GenerateQuadMesh(newRect, newOffset, frame->GetPixelsToUnits());
		}

		cache.settingsRaw = 0;
		cache.settings.packed = true;
		cache.settings.packingMode = packingData.mode;
		cache.settings.packingRotation = packingData.rotation;
	}

	return true;
}

Texture2D* CreateAtlasTexture(const Atlas& atlas, int textureCacheIndex, int atlasSizeX, int atlasSizeY, const std::vector<BlockMask>& atlasMasks, const std::vector<BlockMask>& sourceMasks, const TSpriteMap& sprites, int border)
{
	int textureOptions = Texture2D::kNoMipmap; //TODO:mipmap support(4.5) atlas.settings.enableMipMap ? Texture2D::kMipmapMask : Texture2D::kNoMipmap;

    Texture2D* texture = NEW_OBJECT_MAIN_THREAD (Texture2D);
    texture->Reset();
	bool hasAlpha = HasAlphaTextureFormat(atlas.settings.format);   
    texture->InitTexture(atlasSizeX, atlasSizeY, hasAlpha ? kTexFormatARGB32 : kTexFormatRGB24, textureOptions);
    texture->SetWrapMode(kTexWrapClamp);
	if (atlas.settings.filterMode != -1)
		texture->SetFilterMode(atlas.settings.filterMode);
	texture->SetStoredColorSpace(atlas.settings.colorSpace);
	texture->SetUsageMode(atlas.settings.usageMode);

	// Clear texture
	ColorRGBA32* colors;
	ALLOC_TEMP(colors, ColorRGBA32, atlasSizeX*atlasSizeY);
	memset(colors, 0, atlasSizeX*atlasSizeY*sizeof(ColorRGBA32));
		
	// Transfer pixels
	int mi = 0;
	for (TSpriteMap::const_iterator it = sprites.begin(); it != sprites.end(); ++it, ++mi)
	{
		const BlockMask& srcMask = sourceMasks[mi];
		if (srcMask.fitIn == textureCacheIndex)
		{
			const SpriteRenderData& rd = (*it).first->GetRenderData(false); // Use non-atlased RenderData as input.
			TransferPixelDataForMask(srcMask, rd, colors, atlasSizeX, atlasSizeY, border);
		}
	}

	texture->SetPixels32(0, colors, atlasSizeX*atlasSizeY);
	//if (atlas.settings.enableMipMap)
		//texture->RebuildMipMap();//TODO:mipmap support(4.5)

#if 0 // Write image for debugging
	{
		ImageReference refImg;
		if (texture->GetWriteImageReference (&refImg, 0, 0))
		{
			std::string path = SPRITEPACKER_DEBUG_WRITE_PATH + std::string("Atlas_Hint_") + IntToString(s_currentlyPackingHint) + "_Group_" + IntToString(s_currentlyPackingIndex) + "_Tex_" + IntToString(textureCacheIndex) + ".png";
			ConvertImageToPNGFile(refImg, path);
		}
	}
#endif

	CompressTexture(*texture, atlas.settings.format, atlas.settings.compressionQuality);
	texture->Apply(false, false);
    texture->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

	texture->SetNameCpp(Format("SpriteAtlasTexture-%dx%d-fmt%d", texture->GetDataWidth(), texture->GetDataHeight(), texture->GetTextureFormat()));
	
	return texture;
}

#endif //ENABLE_SPRITES
