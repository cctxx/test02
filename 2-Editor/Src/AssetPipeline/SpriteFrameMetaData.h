#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/SpriteFrame.h"

#if ENABLE_SPRITECOLLIDER
const float  kSpriteEditorDefaultDetail = 0.25f;
const UInt8  kSpriteEditorDefaultAlphaTolerance = 200;
#endif

class TextureImporter;
class Texture2D;

enum SpriteAlignment
{
	kSA_Center = 0,
	kSA_TopLeft,
	kSA_TopCenter,
	kSA_TopRight,
	kSA_LeftCenter,
	kSA_RightCenter,
	kSA_BottomLeft,
	kSA_BottomCenter,
	kSA_BottomRight,
	kSA_Custom
};

struct SpriteMetaData
{
	DECLARE_SERIALIZE (SpriteMetaData)

	SpriteMetaData();

	UnityStr        m_Name;
	Rectf           m_Rect;
	SpriteAlignment m_Alignment;
	Vector2f		m_Pivot; // [0;1] in sprite definition rectangle space
#if ENABLE_SPRITECOLLIDER
	int				m_ColliderAlphaCutoff; // [0;254]
	float			m_ColliderDetail; // [0;1]
#endif

	Vector2f CalculatePivot() const; // [0;1] in sprite definition rectangle space
};

struct SpriteSheetMetaData
{
	DECLARE_SERIALIZE (SpriteSheetMetaData)
	
	typedef std::vector<SpriteMetaData> TSprites;

	TSprites       m_Sprites;
	
	SpriteSheetMetaData();
	friend bool operator == (const SpriteSheetMetaData& lhs, const SpriteSheetMetaData& rhs);

	void GenerateAssetData(Texture2D& texture, std::string textureName, TextureImporter& importer);

private:
	static void ProduceSprites(Texture2D& texture, TextureImporter& importer, const TSprites& sprites);
};


template<class TransferFunction>
void SpriteMetaData::Transfer(TransferFunction& transfer)
{
	transfer.Transfer(m_Name, "m_Name");
	transfer.Transfer(m_Rect, "m_Rect");
	transfer.Transfer((int&)m_Alignment, "m_Alignment");
	transfer.Transfer(m_Pivot, "m_Pivot");
#if ENABLE_SPRITECOLLIDER
	transfer.Transfer(m_ColliderAlphaCutoff, "m_ColliderAlphaCutoff");
	transfer.Transfer(m_ColliderDetail, "m_ColliderDetail");
#endif
}

template<class TransferFunction>
void SpriteSheetMetaData::Transfer(TransferFunction& transfer)
{
	transfer.Transfer(m_Sprites, "m_Sprites");
}

#endif //ENABLE_SPRITES
