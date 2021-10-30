#include "UnityPrefix.h"
#include "SpriteFrame.h"

#if ENABLE_SPRITES

#include "Runtime/Geometry/AABB.h"
#include "Runtime/Graphics/SpriteUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/BuildSettings.h"
#if UNITY_EDITOR
	#include "Editor/Src/EditorSettings.h"
	#include "Editor/Src/SpritePacker/SpritePacker.h"
	#include "Runtime/BaseClasses/IsPlaying.h"
#endif

static const int kMinSpriteDimensionForMesh = 32;
static const float kSpriteAABBDepth = 0.1f;
using namespace Unity;

IMPLEMENT_CLASS(Sprite)
IMPLEMENT_OBJECT_SERIALIZE(Sprite)

Sprite::Sprite(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
, m_PixelsToUnits(100.0f)
, m_Extrude(0)
#if UNITY_EDITOR
, m_AtlasReady(false)
#endif
{
#if ENABLE_MULTITHREADED_CODE
	m_CurrentCPUFence = 0;
	m_WaitOnCPUFence = false;
#endif
}

Sprite::~Sprite()
{
	WaitOnRenderThreadUse();
	m_IntermediateUsers.Notify( kImNotifyAssetDeleted );
}

void Sprite::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);

#if UNITY_EDITOR
	RefreshAtlasRD();
#endif
}

template<class TransferFunction>
void Sprite::Transfer(TransferFunction& transfer)
{
	Super::Transfer(transfer);

	TRANSFER(m_Rect);
	TRANSFER(m_Offset);
	TRANSFER(m_PixelsToUnits);
	TRANSFER(m_Extrude);

	TRANSFER_EDITOR_ONLY_HIDDEN(m_AtlasName);
	TRANSFER_EDITOR_ONLY_HIDDEN(m_PackingTag);

#if UNITY_EDITOR
	bool atlasPacked = !m_PackingTag.empty();
	if (!atlasPacked)
	{
		transfer.Transfer(m_RD, "m_RD");
		transfer.Align();
	}
	else
	{
		if (transfer.IsSerializingForGameRelease())
		{
			const bool packingEnabled = (GetEditorSettings().GetSpritePackerMode() != EditorSettings::kSPOff); // Make sure packing is not disabled
			const bool serializePacked = (packingEnabled && GetIsPacked());

			// Note: it is not expected that all sprites with atlas hints will be packed.
			transfer.Transfer(serializePacked ? m_AtlasRD : m_RD, "m_RD");
			transfer.Align();
		}
		else
		{
			transfer.Transfer(m_RD, "m_RD");
			transfer.Transfer(m_AtlasRD, "m_AtlasRD", kHideInEditorMask);
		}
	}
#else
	transfer.Transfer(m_RD, "m_RD");
	transfer.Align();
#endif

#if ENABLE_SPRITECOLLIDER
	TRANSFER(m_Poly);
#endif
}

bool Sprite::GetIsPacked() const
{
#if UNITY_EDITOR
	return (m_AtlasReady != 0);
#else
	return m_RD.settings.packed;
#endif
}

#if UNITY_EDITOR
static bool s_packingAllowedInPlayMode = false;
void Sprite::OnEnterPlaymode()
{
	s_packingAllowedInPlayMode = (GetEditorSettings().GetSpritePackerMode() == EditorSettings::kSPOn);
}
#endif

const SpriteRenderData& Sprite::GetRenderDataForPlayMode() const
{
#if UNITY_EDITOR
	return GetRenderData(s_packingAllowedInPlayMode && IsWorldPlaying());
#else
	return GetRenderData(true);
#endif
}

const SpriteRenderData& Sprite::GetRenderData(bool getEditorOnlyAtlasRenderDataIfPacked) const
{
#if UNITY_EDITOR
	if (getEditorOnlyAtlasRenderDataIfPacked && GetIsPacked())
		return m_AtlasRD;
#endif
	return m_RD;
}

void Sprite::Initialize(Texture2D* texture, const Rectf& rect, const Vector2f& pivot, float pixelsToUnits, unsigned int extrude, SpriteMeshType meshType)
{
	const bool hasAdvancedVersion = GetBuildSettings().hasAdvancedVersion;

	Assert(texture);
	bool generateRenderMesh = (meshType == kSpriteMeshTypeTight);
	generateRenderMesh &= (rect.width >= kMinSpriteDimensionForMesh);
	generateRenderMesh &= (rect.height >= kMinSpriteDimensionForMesh);
	generateRenderMesh &= (texture->GetRawImageData() != NULL); //BUGFIX:569636 - if we can't access pixel data, generate a quad.
	generateRenderMesh &= hasAdvancedVersion; //Note: tight mesh is Pro only.

	// Common data
	m_Rect = rect;
	m_Offset = PivotToOffset(rect, pivot);
	m_PixelsToUnits = pixelsToUnits;
	m_Extrude = hasAdvancedVersion ? extrude : 0; //Note: extrude is Pro only.

	// Render data
	m_RD.texture = texture;
	if (generateRenderMesh)
	{
		Rectf meshRect;
		m_RD.GenerateFullMesh(m_Rect, m_Offset, pixelsToUnits, m_Extrude, &meshRect);
		m_RD.textureRect = meshRect;
		m_RD.textureRect.x += m_Rect.GetPosition().x;
		m_RD.textureRect.y += m_Rect.GetPosition().y;
	}
	else
	{
		//Note: we could do alpha-trim here. But do we want to lose the texture space in this case?
		m_RD.GenerateQuadMesh(m_Rect, m_Offset, pixelsToUnits);
		m_RD.textureRect = m_Rect;
	}
	m_RD.textureRectOffset = m_RD.textureRect.GetPosition() - rect.GetPosition();
}

void Sprite::WaitOnRenderThreadUse()
{
#if ENABLE_MULTITHREADED_CODE
	if (m_WaitOnCPUFence)
	{
		GetGfxDevice().WaitOnCPUFence(m_CurrentCPUFence);
		m_WaitOnCPUFence = false;
	}
#endif
}

void Sprite::GenerateOutline(float detail, unsigned char alphaTolerance, bool holeDetection, std::vector<dynamic_array<Vector2f> >& outVertices, int extrudeOverride)
{
	unsigned int extrude = (extrudeOverride < 0) ? m_Extrude : extrudeOverride;
	GenerateSpriteOutline(m_RD.texture, m_PixelsToUnits, m_Rect, m_Offset, detail, alphaTolerance, holeDetection, extrude, kPathEmbed, &outVertices);
}

SpriteRenderData::SpriteRenderData()
: settingsRaw(0)
{
}

void SpriteRenderData::GenerateFullMesh(const Rectf& rect, const Vector2f& rectOffset, float pixelsToUnits, unsigned int extrude, Rectf* meshRect)
{
	GenerateSpriteOutline(texture, pixelsToUnits, rect, rectOffset, kSpriteDefaultDetail, kSpriteDefaultAlphaTolerance, true, extrude, kPathEmbed, NULL, &vertices, &indices, meshRect);
}

void SpriteRenderData::GenerateQuadMesh(const Rectf& rect, const Vector2f& rectOffset, float pixelsToUnits)
{
	const float scaler = 1.0f / pixelsToUnits;
	const int texGlWidth = texture->GetGLWidth();
	const int texGlHeight = texture->GetGLHeight();

	const float halfW = rect.width * 0.5f * scaler;
	const float halfH = rect.height * 0.5f * scaler;
	const Vector2f offset = rectOffset * scaler;
	const Vector4f uv = Vector4f(rect.x / texGlWidth, rect.y / texGlHeight, rect.GetXMax() / texGlWidth, rect.GetYMax() / texGlHeight);

	vertices.resize(4);
	vertices[0].pos = Vector3f(-halfW - offset.x,  halfH - offset.y, 0.0f);
	vertices[0].uv = Vector2f(uv[0], uv[3]);
	vertices[1].pos = Vector3f( halfW - offset.x,  halfH - offset.y, 0.0f);
	vertices[1].uv = Vector2f(uv[2], uv[3]);
	vertices[2].pos = Vector3f(-halfW - offset.x, -halfH - offset.y, 0.0f);
	vertices[2].uv = Vector2f(uv[0], uv[1]);
	vertices[3].pos = Vector3f( halfW - offset.x, -halfH - offset.y, 0.0f);
	vertices[3].uv = Vector2f(uv[2], uv[1]);

	indices.resize(6);
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 2;
	indices[4] = 1;
	indices[5] = 3;
}

AABB Sprite::GetBounds(Vector2f extraOffset) const
{
	Vector2f halfRect(m_Rect.width / m_PixelsToUnits * 0.5f, m_Rect.height / m_PixelsToUnits * 0.5f);
	Vector2f offset(m_Offset.x / m_PixelsToUnits, m_Offset.y / m_PixelsToUnits);

	MinMaxAABB minmax;
	Vector3f minV(-halfRect.x - offset.x + extraOffset.x,  halfRect.y - offset.y + extraOffset.y,  kSpriteAABBDepth);
	Vector3f maxV( halfRect.x - offset.x + extraOffset.x, -halfRect.y - offset.y + extraOffset.y, -kSpriteAABBDepth);
	minmax.Encapsulate(minV);
	minmax.Encapsulate(maxV);
	return minmax;
}

Vector2f Sprite::PivotToOffset(const Rectf& rect, const Vector2f& pivot)
{
	const Vector2f alignPos = rect.GetPosition() + rect.GetSize().Scale(pivot);
	Vector2f offset = alignPos - rect.GetCenterPos();
	return offset;
}

#if UNITY_EDITOR
void Sprite::RefreshAtlasRD ()
{
	UnityStr atlasName;
	const SpriteRenderData* packedRD = (m_PackingTag.empty() ? NULL : SpritePacker::GetPackedSpriteRD(*this, atlasName));
	if (packedRD)
	{
		WaitOnRenderThreadUse();
		m_AtlasReady = true;
		m_AtlasRD = *packedRD;
		m_AtlasName = atlasName;
	}
	else
		ClearAtlasRD ();
}

void Sprite::ClearAtlasRD ()
{
	WaitOnRenderThreadUse();
	m_AtlasReady = false;
	m_AtlasRD = SpriteRenderData ();
	m_AtlasName.clear();
}
#endif

#endif //ENABLE_SPRITES
