#include "UnityPrefix.h"
#include "Skybox.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Camera.h"
#include "CameraUtil.h"
#include "Runtime/Shaders/Material.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"

PROFILER_INFORMATION(gRenderSkyboxProfile, "Camera.RenderSkybox", kProfilerRender);


using namespace ShaderLab;

enum
{
	kSkyboxVertexChannels = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0)
};

struct SkyboxVertex {
	float x, y, z;
	float tu, tv;
};

const int kSkyboxVertexCount = 6*4;
static const SkyboxVertex kSkyboxVB[kSkyboxVertexCount] = {
	{ -1,  1,  1, 0,1 }, {  1,  1,  1, 1,1 }, {  1, -1,  1, 1,0 }, { -1, -1,  1, 0,0 },
	{  1,  1, -1, 0,1 }, { -1,  1, -1, 1,1 }, { -1, -1, -1, 1,0 }, {  1, -1, -1, 0,0 },
	{  1,  1,  1, 0,1 }, {  1,  1, -1, 1,1 }, {  1, -1, -1, 1,0 }, {  1, -1,  1, 0,0 },
	{ -1,  1, -1, 0,1 }, { -1,  1,  1, 1,1 }, { -1, -1,  1, 1,0 }, { -1, -1, -1, 0,0 },
	{ -1,  1, -1, 0,1 }, {  1,  1, -1, 1,1 }, {  1,  1,  1, 1,0 }, { -1,  1,  1, 0,0 },
	{ -1, -1,  1, 0,1 }, {  1, -1,  1, 1,1 }, {  1, -1, -1, 1,0 }, { -1, -1, -1, 0,0 },
};

const int kSkyplaneVertexCount = 4;
static const SkyboxVertex kSkyplaneVB[kSkyplaneVertexCount] = {
	{  1,  1, -1, 0,1 }, { -1,  1, -1, 1,1 }, { -1, -1, -1, 1,0 }, {  1, -1, -1, 0,0 },
};

Skybox::Skybox (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

Skybox::~Skybox ()
{
}

void Skybox::AddToManager ()
{

}

void Skybox::RemoveFromManager ()
{

}

void Skybox::RenderSkybox (Material* mat, const Camera& camera)
{
	if( !mat )
		return;

	PROFILER_AUTO_GFX(gRenderSkyboxProfile, &camera)

	Shader* shader = mat->GetShader();
	Assert (shader);

	GfxDevice& device = GetGfxDevice();

	DeviceMVPMatricesState preserveMVP;

	#if UNITY_WP8
	void RotateScreenIfNeeded(Matrix4x4f& mat);
	#endif

	if (camera.GetOrthographic())
	{
		const float epsilon = 1e-6f;
		const float nearPlane = camera.GetNear () * 0.01f;
		const float dist = camera.GetFar() * 10.0f;

		// Make Ortho matrix which passes W to Z
		Matrix4x4f projection = Matrix4x4f::identity;
		//camera.GetImplicitProjectionMatrix( nearPlane, projection );
		projection.Get(2, 2) = -1.0f + epsilon;
		projection.Get(2, 3) = (-2.0f + epsilon) * nearPlane;
		projection.Get(3, 2) = -1.0f;

		#if UNITY_WP8
		if (!RenderTexture::GetActive()) {
			RotateScreenIfNeeded(projection);
		}
		#endif

		Matrix4x4f matrix = Matrix4x4f::identity;
		//device.SetViewMatrix(matrix.GetPtr());
		matrix.SetScale(Vector3f(dist,dist,dist));
		matrix.SetPosition( camera.GetPosition() );
		device.SetWorldMatrix(matrix.GetPtr());
		device.SetProjectionMatrix(projection);
	}
	else
	{
		// Modify Projection matrix to make Infinite Projection Matrix (which passes W into Z)
		// perspective divide will lend ZBuffer value to always be 1.0 (all points on far plane)
		// NOTE: However we need to compensate on floating point precision errors
		// by bringing Z slightly closer to viewer by adding Epsilon
		// See "Projection Matrix Tricks" by Eric Lengyel GDC2007
		// http://www.terathon.com/gdc07_lengyel.ppt

		// In order to avoid clipping of skybox polys we increase skybox size and pull NearPlane as close as possible
		// Higher epsilon values that Z/W < 1.0, but drastically reduces zBuffer precision close to FarPlane
		// Epsilon 1e-6 gives good results as long as NearPlane >= 0.05
		const float epsilon = 1e-6f;
		const float nearPlane = camera.GetNear () * 0.01f;
		const float dist = camera.GetFar() * 10.0f;

		Matrix4x4f projection;
		camera.GetImplicitProjectionMatrix( nearPlane, projection );
		projection.Get(2, 2) = -1.0f + epsilon;
		projection.Get(2, 3) = (-2.0f + epsilon) * nearPlane;
		projection.Get(3, 2) = -1.0f;

		#if UNITY_WP8
		if (!RenderTexture::GetActive()) {
			RotateScreenIfNeeded(projection);
		}
		#endif

		Matrix4x4f matrix = Matrix4x4f::identity;
		matrix.SetScale( Vector3f(dist,dist,dist) );
		matrix.SetPosition( camera.GetPosition() );
		device.SetWorldMatrix( matrix.GetPtr() );
		device.SetProjectionMatrix(projection);
	}

	ShaderLab::IntShader* slshader = shader->GetShaderLabShader();
	const int passCount = slshader->GetActiveSubShader().GetValidPassCount();
	if( passCount == 6 )
	{
		// regular skybox with 6 separate textures per face
		DynamicVBO& vbo = device.GetDynamicVBO();
		for( int j = 0; j < 6; j++ )
		{
			SkyboxVertex* vbPtr = 0;
			if(vbo.GetChunk(kSkyboxVertexChannels, 4, 0, DynamicVBO::kDrawQuads, (void**)&vbPtr, NULL))
			{
				vbPtr[0] = kSkyboxVB[4*j+0];
				vbPtr[1] = kSkyboxVB[4*j+1];
				vbPtr[2] = kSkyboxVB[4*j+2];
				vbPtr[3] = kSkyboxVB[4*j+3];

				vbo.ReleaseChunk(4, 0);

				const ChannelAssigns* channels = mat->SetPassWithShader( j, shader, 0 );
				vbo.DrawChunk (*channels);
				GPU_TIMESTAMP();
			}
		}
	}
	else
	{
		// cube mapped skybox
		for (int pass = 0; pass < passCount; pass++)
		{
			mat->SetPassWithShader( pass, shader, 0 );
			device.ImmediateBegin( kPrimitiveQuads );
			const SkyboxVertex* verts = kSkyboxVB;
			for ( int i = 0; i < kSkyboxVertexCount; i++ ) {
				device.ImmediateTexCoordAll( verts->x, verts->y, verts->z );
				device.ImmediateVertex( verts->x, verts->y, verts->z );
				++verts;
			}
			device.ImmediateEnd();
			GPU_TIMESTAMP();
		}
	}
}

void Skybox::SetMaterial (Material* material) {
	m_CustomSkybox = material;
}

Material* Skybox::GetMaterial ()const {
	return m_CustomSkybox;
}

template<class TransferFunction>
void Skybox::Transfer (TransferFunction& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_CustomSkybox);
}

IMPLEMENT_CLASS (Skybox)
IMPLEMENT_OBJECT_SERIALIZE (Skybox)
