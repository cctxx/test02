#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/Polygon2D.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/IntermediateUsers.h"

const float  kSpriteDefaultDetail = -1.0f; // if less than zero, automatic lod evaluation will performed
const UInt8  kSpriteDefaultAlphaTolerance = 0;
const UInt32 kSpriteDefaultExtrude = 1;
const UInt8  kSpriteMaxAlphaTolerance = 254;

class AABB;

struct SpriteVertex
{
	DECLARE_SERIALIZE_NO_PPTR (SpriteVertex)

	Vector3f    pos;
	Vector2f    uv;
};

enum SpritePackingMode
{
	kSPMTight = 0,
	kSPMRectangle
};

enum SpritePackingRotation
{
	kSPRNone = 0,
	// Reserved
	kSPRAny = 15
};

enum SpriteMeshType
{
	kSpriteMeshTypeFullRect = 0,
	kSpriteMeshTypeTight = 1
};

struct SpriteRenderData
{
	DECLARE_SERIALIZE (SpriteRenderData)

	SpriteRenderData();

	PPtr<Texture2D>           texture;

	// Mesh is scaled according to kTexturePixelsPerUnit
	std::vector<SpriteVertex> vertices;
	std::vector<UInt16>       indices;

	Rectf                     textureRect;       // In pixels (texture space). Invalid if packingMode is Tight.
	Vector2f                  textureRectOffset; // In pixels (texture space). Invalid if packingMode is Tight.
	
	union
	{
		struct 
		{
			UInt32 packed          :  1; // bool
			UInt32 packingMode     :  1; // SpritePackingMode
			UInt32 packingRotation :  4; // SpritePackingRotation
			UInt32 reserved        : 26;
		} settings;
		UInt32 settingsRaw;
	};

	void GenerateQuadMesh(const Rectf& rect, const Vector2f& rectOffset, float pixelsToUnits);
	void GenerateFullMesh(const Rectf& rect, const Vector2f& rectOffset, float pixelsToUnits, unsigned int extrude, Rectf* meshRect);
};

class Sprite : public NamedObject
{
public:
	REGISTER_DERIVED_CLASS(Sprite, NamedObject)
	DECLARE_OBJECT_SERIALIZE(Sprite)
	
	Sprite(MemLabelId label, ObjectCreationMode mode);
	// ~Sprite(); declared-by-macro
	
	void Initialize(Texture2D* texture, const Rectf& rect, const Vector2f& pivot, float pixelsToUnits, unsigned int extrude, SpriteMeshType meshType);

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	// detail [0; 1] where 0 is simplified and 1 is max quality.
	void GenerateOutline(float detail, unsigned char alphaTolerance, bool holeDetection, std::vector<dynamic_array<Vector2f> >& outVertices, int extrudeOverride = -1);

	// Common data
	Vector2f GetSize() const { return m_Rect.GetSize(); }
	Vector2f GetSizeInUnits() const { return m_Rect.GetSize()/m_PixelsToUnits; }

	const Rectf& GetRect() const { return m_Rect; }
	const Vector2f& GetOffset() const { return m_Offset; }
	float GetPixelsToUnits() const { return m_PixelsToUnits; }

#if ENABLE_SPRITECOLLIDER
	Polygon2D& GetPoly() { return m_Poly; }
#endif

	// Rendering data
	// Note: we want live preview of atlases when in editor, but only for rendering in play mode. 
	//       GetRenderData(false) will always return the source texture render data.
	//       GetRenderData(true) will return the atlas texture render data if it is available, otherwise fall back to source texture render data.
	//       GetRenderDataForPlayMode() is a convenience function which tries to get atlas texture render data when world is playing.
	const SpriteRenderData& GetRenderData(bool getEditorOnlyAtlasRenderDataIfPacked) const;
	const SpriteRenderData& GetRenderDataForPlayMode() const;
	
	bool GetIsPacked() const;
	AABB GetBounds(Vector2f extraOffset = Vector2f(0, 0)) const;

#if UNITY_EDITOR
	void SetPackingTag (const std::string& packingTag) { m_PackingTag = packingTag; }
	std::string GetPackingTag() const { return m_PackingTag; }
	
	std::string GetActiveAtlasName() const { return m_AtlasReady ? m_AtlasName : ""; }
	Texture2D* GetActiveAtlasTexture() const { return m_AtlasReady ? m_AtlasRD.texture : NULL; }

	// Changing texture import settings will rebuild Sprites and not use atlases until the next atlas rebuild, which will trigger RefreshAtlasRD or ClearAtlasRD.
	void RefreshAtlasRD ();
	void ClearAtlasRD ();

	static void OnEnterPlaymode ();
#endif

#if ENABLE_MULTITHREADED_CODE
	void SetCurrentCPUFence( UInt32 fence ) { m_CurrentCPUFence = fence; m_WaitOnCPUFence = true; }
#endif
	void WaitOnRenderThreadUse();

	static Vector2f PivotToOffset(const Rectf& rect, const Vector2f& pivot); // [0;1] to rect space

	void AddIntermediateUser( ListNode<IntermediateRenderer>& node ) { m_IntermediateUsers.AddUser(node); }

private:
	// Common data
	Rectf           m_Rect;   // In pixels (originating texture, full rect definition)
	Vector2f        m_Offset; // In pixels (from center of final rect, defines pivot point based on alignment)

	// Rendering data
	SpriteRenderData m_RD;
	float            m_PixelsToUnits;
	unsigned int     m_Extrude;

#if UNITY_EDITOR
	int              m_AtlasReady;
	SpriteRenderData m_AtlasRD;
	UnityStr         m_AtlasName;
	UnityStr         m_PackingTag;
#endif

#if ENABLE_SPRITECOLLIDER
	Polygon2D        m_Poly; // Collider
#endif

#if ENABLE_MULTITHREADED_CODE
	UInt32			m_CurrentCPUFence;
	bool			m_WaitOnCPUFence;
#endif

	IntermediateUsers     m_IntermediateUsers; // IntermediateRenderer users of this sprite
};

template<class TransferFunction>
void SpriteVertex::Transfer(TransferFunction& transfer)
{
	transfer.Transfer(pos, "pos");
	transfer.Transfer(uv, "uv");
}

template<class TransferFunction>
void SpriteRenderData::Transfer(TransferFunction& transfer)
{
	TRANSFER(texture);
	TRANSFER(vertices);
	TRANSFER(indices);

	transfer.Align();
	TRANSFER(textureRect);
	TRANSFER(textureRectOffset);
	TRANSFER(settingsRaw);
}

#endif //ENABLE_SPRITES
