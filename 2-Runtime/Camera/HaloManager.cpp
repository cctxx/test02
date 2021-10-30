#include "UnityPrefix.h"
#include "HaloManager.h"
#include "Runtime/Shaders/Material.h"
#include "RenderManager.h"
#include "Camera.h"
#include "CullResults.h"
#include "RenderLoops/RenderLoop.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Profiler/Profiler.h"

IMPLEMENT_CLASS_HAS_INIT (Halo)
IMPLEMENT_OBJECT_SERIALIZE (Halo)

static Material *s_HaloMaterial = NULL;

Halo::Halo (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Handle = 0;	
}

Halo::~Halo ()
{
}

void Halo::Reset ()
{
	Super::Reset();
	m_Color = ColorRGBA32 (128, 128, 128, 255);
	m_Size = 5.0f;
}

void Halo::InitializeClass () {
	REGISTER_MESSAGE_VOID (Halo, kTransformChanged, TransformChanged);
}

void Halo::CleanupClass () {
//	s_HaloMaterial is clean up by UnloadAllObjects()
}

template<class TransferFunc>
void Halo::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_Color);
	TRANSFER_SIMPLE (m_Size);
}

static void LoadHaloMaterial()
{
	if (s_HaloMaterial)
		return;

	SET_ALLOC_OWNER(NULL);
	Shader* shader = GetScriptMapper ().FindShader ("Hidden/Internal-Halo");
	if (shader)
		s_HaloMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
}

void Halo::AwakeFromLoad (AwakeFromLoadMode awakeMode) {
	Super::AwakeFromLoad (awakeMode);
	if ((awakeMode & kDidLoadFromDisk) == 0 && m_Handle)
		GetHaloManager().UpdateHalo (m_Handle, GetComponent (Transform).GetPosition(), m_Color, m_Size, GetGameObject ().GetLayerMask());

	LoadHaloMaterial();
}

void Halo::TransformChanged () {
	if (m_Handle)
		GetHaloManager().UpdateHalo (m_Handle, GetComponent (Transform).GetPosition(), m_Color, m_Size, GetGameObject ().GetLayerMask());
}
void Halo::AddToManager () {
	m_Handle = GetHaloManager().AddHalo ();
	GetHaloManager().UpdateHalo (m_Handle, GetComponent (Transform).GetPosition(), m_Color, m_Size, GetGameObject ().GetLayerMask());
}
void Halo::RemoveFromManager () {
	GetHaloManager().DeleteHalo (m_Handle);
	m_Handle = 0;
}


HaloManager::HaloManager(MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{	
}

void HaloManager::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);

	GetRenderManager().AddCameraRenderable (this, kTransparentRenderQueue);
}

HaloManager::~HaloManager() {
	RenderManager* mgr = GetRenderManagerPtr();
	if (mgr) // render manager can be already gone
		mgr->RemoveCameraRenderable (this);
}

HaloManager::Halo::Halo (int hdl) 
	: position (Vector3f (0,0,0)), color (ColorRGBAf(0,0,0)), size(1), handle(hdl), layers (1) {	
}
HaloManager::Halo::Halo (const Vector3f &pos, const ColorRGBA32 &col, float s, int h, UInt32 _layers) 
	: position (pos), color (col), size(s), handle(h), layers (_layers) {	
}

IMPLEMENT_CLASS (HaloManager)
GET_MANAGER (HaloManager)

struct HaloVertex {
	Vector3f vert;
	ColorRGBA32 color;
	Vector2f uv;
};
PROFILER_INFORMATION(gHaloRenderProfile, "Halo.Render", kProfilerRender)
PROFILER_INFORMATION(gSubmitVBOProfileHalo, "Mesh.SubmitVBO", kProfilerRender)

void HaloManager::RenderRenderable (const CullResults& cullResults)
{	
	// Just bail if we have no visible halos or using shader replace
	if( m_Halos.empty() || !s_HaloMaterial || cullResults.shaderReplaceData.replacementShader != NULL )
		return;

	LoadHaloMaterial();
	if (!s_HaloMaterial)
		return;

	Shader* shader = s_HaloMaterial->GetShader();
	
	GfxDevice& device = GetGfxDevice();
	#if UNITY_EDITOR
	// don't draw when wireframe mode
	if( device.GetWireframe() )
		return;
	#endif
	
	PROFILER_AUTO(gHaloRenderProfile, this)
	
	
	const int kHaloVertices = 21;
	const int kMaxHalos = 65535 / kHaloVertices; // cap max halos to be rendered so we work on older DX hardware
	int haloCount = m_Halos.size();
	if( haloCount > kMaxHalos )
		haloCount = kMaxHalos;
	
	// Get VBO chunk
	DynamicVBO& vbo = device.GetDynamicVBO();
	HaloVertex* vbPtr;
	if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor),
		haloCount * kHaloVertices, 0,
		DynamicVBO::kDrawTriangleStrip,
		(void**)&vbPtr, NULL ) )
	{
		return;
	}
	

	// Write halos into VBO
	Camera &cam = GetCurrentCamera();
	UInt32 layers = cam.GetCullingMask();
	Matrix4x4f mat (cam.GetWorldToCameraMatrix());
	int halosToRender = 0;
	for( int i = 0; i < haloCount; ++i )
	{
		const Halo& halo = m_Halos[i];
		Vector3f v = mat.MultiplyPoint3( halo.position );
		const float s = halo.size;
		
		// Skip this halo if behind the camera or layers don't match
		if( v.z > -s || !(halo.layers & layers) )
			continue;
		
		// Dim this halo if near the camera (to avoid ugly intersection thingies).
		ColorRGBA32 c;
		if (v.z <= -s * 2.0f ) {
			c = halo.color;
		} else {
			int fac = RoundfToInt((-v.z * 255.0f / s) - 255.0f);
			c = halo.color * fac;
		}
		// Swizzle color of the renderer requires it
		c = device.ConvertToDeviceVertexColor(c);
		
		float z2 = v.z + s * 0.333f;
		
		// Output 21 vertices		
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x-s,v.y  ,v.z);	vbPtr->color = c; vbPtr->uv.Set( 0.0f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x-s,v.y-s,v.z);	vbPtr->color = c; vbPtr->uv.Set( 0, 0 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y-s,v.z);	vbPtr->color = c; vbPtr->uv.Set( .5f, 0 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x+s,v.y-s,v.z);	vbPtr->color = c; vbPtr->uv.Set( 1, 0 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x+s,v.y  ,v.z);	vbPtr->color = c; vbPtr->uv.Set( 1, .5f );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x+s,v.y+s,v.z);	vbPtr->color = c; vbPtr->uv.Set( 1, 1 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y+s,v.z);	vbPtr->color = c; vbPtr->uv.Set( .5f, 1 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x-s,v.y+s,v.z);	vbPtr->color = c; vbPtr->uv.Set( 0, 1 );		++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x-s,v.y  ,v.z);	vbPtr->color = c; vbPtr->uv.Set( 0.0f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		vbPtr->vert.Set( v.x  ,v.y  , z2);	vbPtr->color = c; vbPtr->uv.Set( .5f, .5f );	++vbPtr;
		
		++halosToRender;
	}
	
	vbo.ReleaseChunk( halosToRender * kHaloVertices, 0 );

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity

	// Output halos
	const ChannelAssigns* channels = s_HaloMaterial->SetPassWithShader( 0, shader, 0 );
	
	PROFILER_BEGIN(gSubmitVBOProfileHalo, this)
	vbo.DrawChunk (*channels);
	GPU_TIMESTAMP();
	PROFILER_END

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

int HaloManager::AddHalo () {
	int handle;
	if (!m_Halos.empty())
		handle = m_Halos.back().handle + 1;
	else
		handle = 1;
	m_Halos.push_back (Halo (handle));
	return handle;
}

void HaloManager::UpdateHalo (int h, Vector3f position,ColorRGBA32 color,float size, UInt32 layers)
{
	for (HaloList::iterator i = m_Halos.begin(); i != m_Halos.end(); i++) {
		if (i->handle == h) {
			i->position = position;
			i->color = color;
			i->size = size;
			i->layers = layers;
			return;
		}
	}
	AssertString ("Unable to find Halo to update");
}

void HaloManager::DeleteHalo (int h)
{
	for (HaloList::iterator i = m_Halos.begin(); i != m_Halos.end(); i++) {
		if (i->handle == h) {
			m_Halos.erase (i);
			return;
		}
	}
	AssertString ("Unable to find Halo to be deleted");
}

IMPLEMENT_CLASS (HaloLayer)

HaloLayer::HaloLayer (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

HaloLayer::~HaloLayer ()
{
}
