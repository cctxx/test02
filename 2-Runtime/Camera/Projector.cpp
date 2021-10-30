#include "UnityPrefix.h"
#include "Projector.h"
#include "BaseRenderer.h"
#include "RenderManager.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/Camera.h"
#include "Renderqueue.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Camera/CullResults.h"
//#include "Runtime/Camera/RenderLoops/RenderLoopEnums.h"

void Projector::InitializeClass ()
{
	RegisterAllowNameConversion (Projector::GetClassStringStatic(), "m_IsOrthoGraphic", "m_Orthographic");
	RegisterAllowNameConversion (Projector::GetClassStringStatic(), "m_OrthoGraphicSize", "m_OrthographicSize");
}

Projector::Projector (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

Projector::~Projector ()
{
}

void Projector::Reset ()
{
	Super::Reset();
	m_NearClipPlane = .1F;
	m_FarClipPlane = 100.0F;
	m_FieldOfView = 60;
	m_AspectRatio = 1.0F;
	m_OrthographicSize = 10;
	m_Orthographic = false;
	m_IgnoreLayers.m_Bits = 0;
}

// chosen not to blow up when the user has set fov, aspect ratio and ortho size to 0 at the same time
static const float kSmallValue = 1.0e-8f;

// should probably be scaled with m_NearClipPlane, but it's fine for reasonable near clip values
static const float kSmallestNearClipMargin = 1.0e-2f;

void Projector::CheckConsistency ()
{
	Super::CheckConsistency ();

	if (m_Orthographic)
	{
		float clipPlaneDiff = m_FarClipPlane - m_NearClipPlane;
		if (Abs (clipPlaneDiff) < kSmallestNearClipMargin)
			m_FarClipPlane = m_NearClipPlane + Sign (clipPlaneDiff) * kSmallestNearClipMargin;
	}
	else
	{
		if (m_NearClipPlane < kSmallestNearClipMargin)
			m_NearClipPlane = kSmallestNearClipMargin;

		if (m_FarClipPlane < m_NearClipPlane + kSmallestNearClipMargin)
			m_FarClipPlane = m_NearClipPlane + kSmallestNearClipMargin;
	}


	if (Abs (m_FieldOfView) < kSmallValue)
		m_FieldOfView = kSmallValue * Sign (m_FieldOfView);

	if (Abs (m_AspectRatio) < kSmallValue)
		m_AspectRatio = kSmallValue * Sign (m_AspectRatio);

	if (Abs (m_OrthographicSize) < kSmallValue)
		m_OrthographicSize = kSmallValue * Sign (m_OrthographicSize);
}

using namespace Unity;

struct ProjectorRenderSettings
{
	Matrix4x4f projection;
	Matrix4x4f distance;
	Matrix4x4f clipping;
	Matrix4x4f frustumMatrix;
	
	Material*  material;
	Shader*    shader;
	int        subshaderIndex;
	int        passCount;
};

static void RenderProjectorForRenderer ( BaseRenderer* renderer, const ProjectorRenderSettings& settings)
{
	const TransformInfo& xformInfo = renderer->GetTransformInfo ();		
	const Matrix4x4f& transform = xformInfo.worldMatrix;
	
	// texture projection matrices
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();
	MultiplyMatrices4x4 (&settings.projection, &transform, &params.GetWritableMatrixParam(kShaderMatProjector));		
	MultiplyMatrices4x4 (&settings.distance, &transform, &params.GetWritableMatrixParam(kShaderMatProjectorDistance));
	MultiplyMatrices4x4 (&settings.clipping, &transform, &params.GetWritableMatrixParam(kShaderMatProjectorClip));
	
	SetupObjectMatrix (transform, xformInfo.transformType);
	
	for (int i=0;i<settings.passCount;i++)
	{
		const ChannelAssigns* channels = settings.material->SetPassWithShader( i, settings.shader, settings.subshaderIndex );
		if (!channels) // pass should not be rendered
			continue;
		
		int rendererMatCount = renderer->GetMaterialCount();
		for( int m = 0; m < rendererMatCount; ++m )
		{
			Material* rendererMat = renderer->GetMaterial(m);
			if( rendererMat )
			{
				Shader* rendererShader = rendererMat->GetShader();
				if( rendererShader && rendererShader->GetShaderLabShader()->GetNoProjector() )
					continue;
			}
			renderer->Render (renderer->GetSubsetIndex(m), *channels);
		}
	}
}

void Projector::AddToManager ()
{
	RenderManager& renderManager = GetRenderManager();

	// Remove first to make sure we don't have duplicates
	// @todo: Eventually Behaviour should be fixed to always close a AddToManager with a RemoveFromManager
	renderManager.RemoveCameraRenderable (this);

	// The projector should default to "after all geometry" render queue.
	// Unless the material defines a non-default render queue, in which case
	// use that.
	int renderQueue = kGeometryQueueIndexMax + 1;
	Material* projectorMaterial = m_Material;
	if (projectorMaterial)
	{
		int rq = projectorMaterial->GetActualRenderQueue();
		if (rq != kGeometryRenderQueue)
			renderQueue = rq;
	}

	renderManager.AddCameraRenderable (this, renderQueue);
}


void Projector::RemoveFromManager ()
{
	GetRenderManager ().RemoveCameraRenderable (this);
}

void Projector::SetupProjectorSettings (Material* projectorMaterial, ProjectorRenderSettings& projectorSettings)
{
	Matrix4x4f projectionMatrix = CalculateProjectionMatrix ();
	
	Matrix4x4f zscale;
	zscale.SetScale (Vector3f (1.0F, 1.0F, -1.0F));
	
	Matrix4x4f projectorToWorld;
	projectorToWorld = GetComponent (Transform).GetWorldToLocalMatrixNoScale ();
	
	Matrix4x4f temp1, temp2,temp3, temp4;
	
	// Setup the functor
	// projection matrix
	temp1.SetScale (Vector3f (.5f, .5f, 1.0f));
	temp2.SetTranslate (Vector3f (.5f, .5f, 0.0f));
	// functor.projection = temp2 * projectionMatrix * zscale * temp1 * projectorToWorld
	MultiplyMatrices4x4 (&temp2, &projectionMatrix, &temp3);
	MultiplyMatrices4x4 (&temp3, &zscale, &temp4);
	MultiplyMatrices4x4 (&temp4, &temp1, &temp2);
	MultiplyMatrices4x4 (&temp2, &projectorToWorld, &projectorSettings.projection);
	
	// X-axis fadeout matrix
	float scale = 1.0f / m_FarClipPlane;
	temp1.SetScale (Vector3f (scale, scale, scale));
	temp2.SetIdentity ();
	temp2.Get(0,0) = 0; temp2.Get(0,1) = 0; temp2.Get(0,2) = 1; temp2.Get(0,0) = 0; 
	// functor.distance = temp2 * temp1 * projectorToWorld
	MultiplyMatrices4x4 (&temp2, &temp1, &temp3);
	MultiplyMatrices4x4 (&temp3, &projectorToWorld, &projectorSettings.distance);
	
	// X-axis texture cull (use with an alpha map to do alpha-tested clip planes)
	scale = 1.0f / (m_FarClipPlane - m_NearClipPlane);
	temp1.SetScale (Vector3f (scale, scale, scale));
	temp2.SetIdentity ();
	temp3.SetTranslate (Vector3f (-m_NearClipPlane, -m_NearClipPlane, -m_NearClipPlane));
	temp2.Get(0,0) = 0; temp2.Get(0,1) = 0; temp2.Get(0,2) = 1; temp2.Get(0,0) = 0; 
	// functor.clipping = temp2 * temp1 * temp3 * projectorToWorld
	MultiplyMatrices4x4 (&temp2, &temp1, &temp4);
	MultiplyMatrices4x4 (&temp4, &temp3, &temp1);
	MultiplyMatrices4x4 (&temp1, &projectorToWorld, &projectorSettings.clipping);
	
	Shader* shader = projectorMaterial->GetShader();
	int subshaderIndex = 0;
	projectorSettings.material = projectorMaterial;
	projectorSettings.shader = shader;
	projectorSettings.subshaderIndex = subshaderIndex;
	const ShaderLab::SubShader& ss = shader->GetShaderLabShader()->GetSubShader(subshaderIndex);
	projectorSettings.passCount = ss.GetValidPassCount();
	
	/// Setup culling planes to be the projector area without any layer based distance culling.
	// finalProj = projectionMatrix * zscale * projectorToWorld
	MultiplyMatrices4x4 (&projectionMatrix, &zscale, &temp1);
	MultiplyMatrices4x4 (&temp1, &projectorToWorld, &projectorSettings.frustumMatrix);
}


void Projector::RenderRenderable (const CullResults& cullResults)
{
	// early out if we have no material
	Material* projectorMaterial = m_Material;
	if( !projectorMaterial )
		return;
	
	// We dont't support projectors when doing shader replacement
	if ( cullResults.shaderReplaceData.replacementShader != NULL)
		return;
	
	Camera& camera = GetCurrentCamera();
	if( !(camera.GetCullingMask() & GetGameObject().GetLayerMask()) )
		return; // current camera does not render our layer - exit

	// save current view/world matrices
	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	
	ProjectorRenderSettings projectorSettings;
	SetupProjectorSettings (projectorMaterial, projectorSettings);
	
	UInt32 projectorCullingMask = ~(m_IgnoreLayers.m_Bits);
	
	// From the objects that we are rendering from this camera.
	// Cull it further to the objects that overlap with the Projector frustum
	Plane    cullingPlanes[6];
	ExtractProjectionPlanes (projectorSettings.frustumMatrix, cullingPlanes);
	
	const VisibleNodes& nodes = cullResults.nodes;
	for (int i=0;i<nodes.size();i++)
	{
		if (IntersectAABBFrustumFull (nodes[i].worldAABB, cullingPlanes))
		{
			UInt32 mask = nodes[i].renderer->GetLayerMask();
			if (mask & projectorCullingMask)
				RenderProjectorForRenderer (nodes[i].renderer, projectorSettings);
		}
	}
	
	// restore view/world matrices
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

Matrix4x4f Projector::GetProjectorToPerspectiveMatrix() const
{
	Matrix4x4f zscale;
	zscale.SetScale (Vector3f (1.0F, 1.0F, -1.0F));
	
	Matrix4x4f projectorToWorld = GetComponent (Transform).GetWorldToLocalMatrixNoScale ();
	
	// CalculateProjectionMatrix() * zscale * projectorToWorld
	Matrix4x4f proj, temp, res;
	proj = CalculateProjectionMatrix();
	MultiplyMatrices4x4 (&proj, &zscale, &temp);
	MultiplyMatrices4x4 (&temp, &projectorToWorld, &res);
	return res;
}

Matrix4x4f Projector::CalculateProjectionMatrix() const
{
	Matrix4x4f projection;
	if( m_Orthographic )
		projection.SetOrtho(-m_OrthographicSize * m_AspectRatio, m_OrthographicSize * m_AspectRatio, -m_OrthographicSize, m_OrthographicSize, m_NearClipPlane, m_FarClipPlane);
	else
		projection.SetPerspective(m_FieldOfView, m_AspectRatio, m_NearClipPlane, m_FarClipPlane);
	return projection;
}

template<class TransferFunction>
void Projector::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	// Note: transfer code for version 1 was just removed. It was around Unity 1.2 times,
	// and now we're fine with losing project folder compatibility with that.
	transfer.SetVersion (2);
	
	TRANSFER_SIMPLE(m_NearClipPlane);
	TRANSFER_SIMPLE(m_FarClipPlane);
	TRANSFER_SIMPLE(m_FieldOfView);
	TRANSFER(m_AspectRatio);
	TRANSFER(m_Orthographic);
	transfer.Align();
	TRANSFER(m_OrthographicSize);
	TRANSFER_SIMPLE(m_Material);
	TRANSFER(m_IgnoreLayers);
}

IMPLEMENT_OBJECT_SERIALIZE (Projector)
IMPLEMENT_CLASS_HAS_INIT (Projector)
