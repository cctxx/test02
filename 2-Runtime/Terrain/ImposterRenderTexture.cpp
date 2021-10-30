#include "UnityPrefix.h"
#include "ImposterRenderTexture.h"

#if ENABLE_TERRAIN

#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/Renderqueue.h"


#if UNITY_PS3
#	include "Runtime/GfxDevice/ps3/GfxDevicePS3.h"
#endif

const float kMinimumAngleDelta = 0.1f * kDeg2Rad;
const float kImpostorPadding = 1.0f;

ImposterRenderTexture::ImposterRenderTexture (const TreeDatabase& treeDB) 
:	m_TreeDatabase(treeDB)
,	m_AngleX(std::numeric_limits<float>::infinity())
,	m_AngleY(std::numeric_limits<float>::infinity())
,	m_ImposterHeight(256)
,	m_MaxImposterSize(2048)
,	m_CameraOrientationMatrix(Matrix4x4f::identity)
{
	// if render textures or vertex shaders are not supported, there shall be
	// no impostors.
	RenderTexture::SetTemporarilyAllowIndieRenderTexture (true);
	if (!RenderTexture::IsEnabled())
	{
		RenderTexture::SetTemporarilyAllowIndieRenderTexture (false);
		m_Supported = false;
		m_Texture = NULL;
		m_Camera = NULL;
		return;
	}
	m_Supported = true;
	
	const std::vector<TreeDatabase::Prototype>& prototypes = m_TreeDatabase.GetPrototypes();
	
	m_Areas.resize(prototypes.size());
	// Calculate how many pixel width we need!		
	float totalPixelWidth = 0.0F;
	for (int i = 0; i < prototypes.size(); ++i)
	{
		totalPixelWidth += m_ImposterHeight * prototypes[i].getBillboardAspect() + kImpostorPadding;
	}
	
	int textureWidth = std::min<int>(m_MaxImposterSize, ClosestPowerOfTwo(RoundfToIntPos(totalPixelWidth)));
	
	float uOffset = kImpostorPadding / (float)textureWidth;

	// Calculate areas
	float runOffset = 0.0F;
	for (int i = 0; i < prototypes.size(); ++i)
	{
		float width = (m_ImposterHeight * prototypes[i].getBillboardAspect()) / totalPixelWidth;
		m_Areas[i].Set(runOffset + uOffset, 0.0F, width - uOffset - uOffset, 1.0F);
		runOffset += width;
	}

	// Setup Render texture
	m_Texture = NEW_OBJECT (RenderTexture);
	m_Texture->Reset();

	m_Texture->SetHideFlags(Object::kDontSave);
	m_Texture->SetWidth(textureWidth);
	m_Texture->SetHeight(m_ImposterHeight);
	m_Texture->SetColorFormat(kRTFormatARGB32);
	m_Texture->SetDepthFormat(kDepthFormat16);
	m_Texture->SetName("Tree Imposter Texture");
	m_Texture->SetMipMap(true);
	m_Texture->SetMipMapBias(-1);
	
	m_Texture->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad); 
	
	// Setup camera	
	GameObject* cameraGO = &CreateGameObjectWithHideFlags ("Imposter Camera", true, Object::kHideAndDontSave, "Camera", NULL);
	m_Camera = &cameraGO->GetComponent(Camera);
	m_Camera->SetTargetTexture(m_Texture);
	m_Camera->SetClearFlags(Camera::kSolidColor);
	m_Camera->SetBackgroundColor(ColorRGBAf(.2f,.2f,.2f,0));
	m_Camera->SetOrthographic(true);
	m_Camera->SetCullingMask(0);
	m_Camera->SetEnabled(false);

#if UNITY_PS3
	((GfxDevicePS3&)GetGfxDevice()).RegisterSurfaceLostCallback(&InvalidateRenderTexture, this);
#endif

	RenderTexture::SetTemporarilyAllowIndieRenderTexture (false);
}

ImposterRenderTexture::~ImposterRenderTexture()
{
#if UNITY_PS3
	((GfxDevicePS3&)GetGfxDevice()).UnregisterSurfaceLostCallback(&InvalidateRenderTexture, this);
#endif
	if (m_Camera != NULL)
		DestroyObjectHighLevel(&m_Camera->GetGameObject());
	if (m_Texture != NULL)
		DestroyObjectHighLevel(m_Texture);
}

void ImposterRenderTexture::UpdateImposters (const Camera& mainCamera)
{
	// Only update if we really have to because the viewing angle is very different
	const Transform& transform = mainCamera.GetComponent (Transform);
	const Vector3f& cameraEuler = QuaternionToEuler(transform.GetRotation());
	if (m_Texture->IsCreated())
	{
		if (m_AngleX != std::numeric_limits<float>::infinity() && std::abs(DeltaAngleRad (m_AngleX, cameraEuler.x)) < kMinimumAngleDelta &&
			m_AngleY != std::numeric_limits<float>::infinity() && std::abs(DeltaAngleRad (m_AngleY, cameraEuler.y)) < kMinimumAngleDelta)
			return;
	}

	m_AngleX = cameraEuler.x;
	m_AngleY = cameraEuler.y;

	m_Camera->GetComponent (Transform).SetLocalEulerAngles(Vector3f (cameraEuler.x, cameraEuler.y, 0) * Rad2Deg(1));
	m_CameraOrientationMatrix = m_Camera->GetCameraToWorldMatrix();
	
	Camera& oldCamera = GetCurrentCamera();

	// Clear the whole render texture
	m_Camera->SetNormalizedViewportRect(Rectf (0, 0, 1, 1));
	m_Camera->SetClearFlags(Camera::kSolidColor);

	m_Camera->StandaloneRender(Camera::kRenderFlagDontRestoreRenderState | Camera::kRenderFlagSetRenderTarget, NULL, "");

	const std::vector<TreeDatabase::Prototype>& prototypes = m_TreeDatabase.GetPrototypes();
	for (int i = 0; i < prototypes.size(); ++i)
	{
		const Rectf& rect = m_Areas[i];//new Rect (offset , 0.0F, width, 1.0F);
		UpdateImposter(rect, prototypes[i]);
	}

	oldCamera.StandaloneSetup();
}

// angleFactor - used for:
//		1) non-uniform scale compensation 
//		2) blending between vertical (viewing from front) and horizontal (viewing from above/below) billboard modes
// offsetFactor - used for offsetting billboard from the ground when billboard is in horizontal mode
// 
// See TerrainBillboardTree in TerrainEngine.cginc for more detailed explanation
void ImposterRenderTexture::GetBillboardParams(float& angleFactor, float& offsetFactor) const
{
	float angles = Rad2Deg(m_AngleX);
	angles = angles <= 90 ? angles : angles - 360;

	AssertMsg(angles >= -90 && angles <= 90, "Invalid angle: %f", angles);

	angleFactor = 1 - cos(Deg2Rad(angles));

	{
		// calculate modeFactor
		const float kLimitAngle = 40;

		float factor = fabsf(angles);
		factor = factor < kLimitAngle ? 0 : SmoothStep(0, 1, (factor - kLimitAngle) / (90 - kLimitAngle));

		// we never want to use bottom-center mode completely, because it can cause intersection 
		// of billboard with terrain, so we just raise it a bit all the time
		offsetFactor = std::max(0.1f, factor);
	}
}

void ImposterRenderTexture::UpdateImposter (const Rectf& rect, const TreeDatabase::Prototype& prototype)
{
	if (prototype.imposterMaterials.empty()
		|| !prototype.mesh.IsValid())
	{
		return;
	}

	{
		Transform& transform = m_Camera->GetComponent (Transform);
		// Setup camera location
		transform.SetPosition(Vector3f (0, prototype.getCenterOffset(), 0));
		// Just move far away enough to get the whole tree into the view. (How far doesnt matter since it is orthographic projection)
		transform.SetPosition(transform.GetPosition() + transform.TransformDirection (-Vector3f::zAxis * (prototype.treeHeight + prototype.treeWidth) * 2));
	}	

	// Setup camera rect
	m_Camera->SetClearFlags(Camera::kDontClear);
	m_Camera->SetNormalizedViewportRect(rect);
	
	m_Camera->SetAspect(prototype.getBillboardAspect());
	m_Camera->SetOrthographicSize(prototype.getBillboardHeight() * 0.5F);

	// Setup render target
	m_Camera->StandaloneRender(Camera::kRenderFlagDontRestoreRenderState | Camera::kRenderFlagSetRenderTarget, NULL, "");

	const ShaderLab::FastPropertyName colorProperty = ShaderLab::Property("_Color");
	const ShaderLab::FastPropertyName halfOverCutoffProperty = ShaderLab::Property("_HalfOverCutoff");
	const ShaderLab::FastPropertyName terrainEngineBendTreeProperty = ShaderLab::Property("_TerrainEngineBendTree");

	for (int m=0; m<prototype.imposterMaterials.size(); ++m)
	{
		const ColorRGBAf& color = prototype.originalMaterialColors[m];
		float cutoff = prototype.inverseAlphaCutoff[m];
		Material* mat = prototype.imposterMaterials[m];
		mat->SetColor(colorProperty, color);
		mat->SetFloat(halfOverCutoffProperty, cutoff);
		mat->SetMatrix(terrainEngineBendTreeProperty, Matrix4x4f::identity);
		for (int p=0; p<mat->GetPassCount(); ++p)
		{
			if (CheckShouldRenderPass (p, *mat))
			{
				const ChannelAssigns* channels = mat->SetPass(p);
				if (channels)
				{
					DrawUtil::DrawMesh (*channels, *prototype.mesh, Vector3f::zero, Quaternionf::identity(), m);
				}
			}
		}
	}
}

#endif // ENABLE_TERRAIN
