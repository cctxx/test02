#include "UnityPrefix.h"
#include "SpriteFrameMetaData.h"

#if ENABLE_SPRITES

#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Graphics/Polygon2D.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "SpriteImporterUtility.h"

SpriteMetaData::SpriteMetaData()
: m_Alignment(kSA_Center)
#if ENABLE_SPRITECOLLIDER
, m_ColliderAlphaCutoff(kSpriteEditorDefaultAlphaTolerance)
, m_ColliderDetail(kSpriteEditorDefaultDetail)
#endif
{
}

void SpriteSheetMetaData::ProduceSprites(Texture2D& texture, TextureImporter& importer, const TSprites& sprites)
{	
	if (sprites.size() > 0)
	{
		// Calculate definition scale
		TextureImporter::SourceTextureInformation sti = importer.GetSourceTextureInformation();
		float definitionScale = float(texture.GetGLWidth()) / sti.width;		

		const float dstRatio = float(texture.GetGLWidth()) / texture.GetGLHeight();
		const float srcRatio = float(sti.width) / sti.height;
		Assert(CompareApproximately(dstRatio, srcRatio, 1.0f/32.0f)); // Stretching is not supported. NPOT option should be disabled in the TextureImporter.

		// Create Sprite objects
		for (SpriteSheetMetaData::TSprites::const_iterator it = sprites.begin(); it != sprites.end(); ++it)
		{
			const SpriteMetaData& spriteMD = *it;

			Rectf scaledRect = spriteMD.m_Rect;
			scaledRect.Scale(definitionScale, definitionScale);
			if(!ValidateSpriteRect(scaledRect, texture))
				continue;

			float scaledPTU = importer.GetSpritePixelsToUnits() * definitionScale;

			Sprite& sprite = importer.ProduceAssetObject<Sprite>(spriteMD.m_Name);
			sprite.Initialize(&texture, scaledRect, spriteMD.CalculatePivot(), scaledPTU, importer.GetSpriteExtrude(), importer.GetSpriteMeshType());
			sprite.SetPackingTag(importer.GetQualifiesForSpritePacking() ? importer.GetSpritePackingTag() : "");
#if ENABLE_SPRITECOLLIDER
			sprite.GetPoly().GenerateFrom(&sprite, Vector2f(0.0f, 0.0f), spriteMD.m_ColliderDetail, spriteMD.m_ColliderAlphaCutoff, true);
#endif
			sprite.AwakeFromLoad(kDefaultAwakeFromLoad);
		}
	}
}

void SpriteSheetMetaData::GenerateAssetData(Texture2D& texture, std::string textureName, TextureImporter& importer)
{
	switch (importer.GetSpriteMode())
	{
		case TextureImporter::kSpriteModeSingle:
		{
			TSprites sprites;
			sprites.resize(1);

			TextureImporter::SourceTextureInformation sti = importer.GetSourceTextureInformation();

			// Generate one Sprite covering the entire image
			SpriteMetaData& frame = sprites[0];
			frame.m_Name = DeletePathNameExtension (GetLastPathNameComponent (importer.GetAssetPathName ()));
			frame.m_Rect = Rectf(0, 0, sti.width, sti.height);
			frame.m_Alignment = (SpriteAlignment)importer.GetSettings().m_Alignment;
			frame.m_Pivot = importer.GetSettings().m_SpritePivot;
#if ENABLE_SPRITECOLLIDER
			frame.m_ColliderAlphaCutoff = clamp<int>(importer.GetSettings().m_SpriteColliderAlphaCutoff, 0, 254);
			frame.m_ColliderDetail = importer.GetSettings().m_SpriteColliderDetail;
#endif

			ProduceSprites (texture, importer, sprites);
		}
		break;

		case TextureImporter::kSpriteModeManual:
			ProduceSprites (texture, importer, m_Sprites);
			break;

		default:
			// This code should not be reached
			break;
	}
}

static float AlignmentToPivotX(SpriteAlignment alignment)
{
	switch (alignment)
	{
	case kSA_Center:
	case kSA_TopCenter:
	case kSA_BottomCenter:
		return 0.5f;
	case kSA_TopRight:
	case kSA_RightCenter:
	case kSA_BottomRight:
		return 1.0f;
	default:
		return 0.0f;
	};
}

static float AlignmentToPivotY(SpriteAlignment alignment)
{
	switch (alignment)
	{
	case kSA_Center:
	case kSA_LeftCenter:
	case kSA_RightCenter:
		return 0.5f;
	case kSA_BottomLeft:
	case kSA_BottomCenter:
	case kSA_BottomRight:
		return 0.0f;
	default:
		return 1.0f;
	};
}

Vector2f SpriteMetaData::CalculatePivot() const
{
	if (m_Alignment == kSA_Custom)
	{
		return m_Pivot;
	}
	else
	{
		Vector2f pivot(AlignmentToPivotX(m_Alignment), AlignmentToPivotY(m_Alignment));
		return pivot;
	}
}

SpriteSheetMetaData::SpriteSheetMetaData(){}

bool operator == (const SpriteSheetMetaData& lhs, const SpriteSheetMetaData& rhs)
{
	int result = memcmp(&lhs, &rhs, sizeof(SpriteSheetMetaData));
	return result == 0;
}

#endif //ENABLE_SPRITES
