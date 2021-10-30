#include "UnityPrefix.h"
#include "Flare.h"
#include "Runtime/Interfaces/IRaycast.h"
#include "Camera.h"
#include "RenderSettings.h"
#include "RenderManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/Transform.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Profiler/Profiler.h"

static Material *s_FlareMaterial;

static SHADERPROP (FlareTexture);


static void CalculateFlareGetUVCoords (int layout, int imageIndex, const Vector2f& pixOffset, Vector2f& outUV0, Vector2f& outUV1 );


Flare::Flare (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
	
}

Flare::~Flare ()
{
}

void Flare::Reset ()
{
	Super::Reset();
	resize_trimmed (m_Elements, 1);
	m_Elements[0].m_ImageIndex = 0;
	m_Elements[0].m_Rotate = false;
	m_Elements[0].m_Position = 0.0f;
	m_Elements[0].m_Size = 0.5f;
	m_Elements[0].m_Color = ColorRGBAf (1,1,1,0);
	m_Elements[0].m_Zoom = true;
	m_Elements[0].m_Fade = true;
	m_Elements[0].m_UseLightColor = true;
	m_UseFog = true;
	m_TextureLayout = 0;
}

template<class TransferFunc>
void Flare::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_FlareTexture);
	TRANSFER_SIMPLE (m_TextureLayout);
	TRANSFER_SIMPLE (m_Elements);
	TRANSFER (m_UseFog);
}

void Flare::AwakeFromLoad (AwakeFromLoadMode awakeMode) {
	Super::AwakeFromLoad(awakeMode);

	// mark pixel offset as "need to figure out"
	m_PixOffset.Set( -1.0f, -1.0f );
}

struct FlareVertex {
	Vector3f vert;
	ColorRGBA32 color;
	Vector2f uv;
};
PROFILER_INFORMATION(gSubmitVBOProfileFlare, "Mesh.SubmitVBO", kProfilerRender)

void Flare::Render (Vector3f &pos, float visibility, const ColorRGBAf &tintColor, const ChannelAssigns& channels)
{
	// Figure out pixel offset if we have to.
	// Have to do this just before rendering, because at load time texel sizes might not
	// be set up for some textures yet (render textures in particular).
	if( m_PixOffset.x == -1.0f )
	{
		Texture *tex = m_FlareTexture;
		// Get the UV offsets to address the texture on texel boundaries
		// Use this to work around OpenGL's feature that UVs are in the middle of a texel.
		if( tex )
		{
			m_PixOffset.x = tex->GetTexelSizeX() * 0.5f;
			m_PixOffset.y = tex->GetTexelSizeY() * 0.5f;
		}
		else
		{
			m_PixOffset.Set(0,0);
		}
	}

	Vector2f p (pos.x, pos.y);
	if (SqrMagnitude (p) > Vector2f::epsilon)
		p = Normalize (p);
	else
		p = Vector2f (1,0);
	
	if (m_UseFog)
		visibility *= 1.0F-GetRenderSettings().CalcFogFactor (pos.z);

	// Get VBO chunk
	const int elemCount = m_Elements.size();
	GfxDevice& device = GetGfxDevice();
	DynamicVBO& vbo = device.GetDynamicVBO();
	FlareVertex* vbPtr;
	if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor),
		elemCount * 4, 0,
		DynamicVBO::kDrawQuads,
		(void**)&vbPtr, NULL ) )
	{
		return;
	}
	
	std::vector<FlareElement>::const_iterator it, itEnd = m_Elements.end();
	for (it = m_Elements.begin(); it != itEnd; ++it)
	{
		Vector2f uv0, uv1;
		CalculateFlareGetUVCoords (m_TextureLayout, it->m_ImageIndex, m_PixOffset, uv0, uv1);

		// Had to invert uv's when we flipped all our textures to conform to opengl
		// the actual code using the uv's should flip uv's instead of this fix.
		uv0.y = 1.0F - uv0.y;
		uv1.y = 1.0F - uv1.y;
				
		float s = it->m_Size * pos.z   * (it->m_Zoom ? visibility * .01f :  .01f);
		Vector2f size;
		if (it->m_Rotate) {
			size = p * (s * 1.4f);
		} else {
			size.x = size.y  = s;
		}
		Vector3f v = Lerp (pos, Vector3f (0,0,pos.z), it->m_Position);
		ColorRGBA32 color;
		
		if (!it->m_UseLightColor) {
			color = (it->m_Color * visibility);
		} else {
			ColorRGBAf col = it->m_Color;
			col.r *= tintColor.r;
			col.g *= tintColor.g;
			col.b *= tintColor.b;
			col.a *= tintColor.a;
			if( it->m_Fade )
				col = col * (visibility);
			color = col;
		}
		// Swizzle color of the renderer requires it
		color = device.ConvertToDeviceVertexColor(color);

		// vertices
		vbPtr[0].vert.Set( v.x - size.x, v.y - size.y, v.z );
		vbPtr[0].color = color;
		vbPtr[0].uv.Set( uv1.x, uv0.y );
		
		vbPtr[1].vert.Set( v.x + size.y, v.y - size.x, v.z );
		vbPtr[1].color = color;
		vbPtr[1].uv.Set( uv0.x, uv0.y );
		
		vbPtr[2].vert.Set( v.x + size.x, v.y + size.y, v.z );
		vbPtr[2].color = color;
		vbPtr[2].uv.Set( uv0.x, uv1.y );
		
		vbPtr[3].vert.Set( v.x - size.y, v.y + size.x, v.z );
		vbPtr[3].color = color;
		vbPtr[3].uv.Set( uv1.x, uv1.y );
		
		vbPtr += 4;
	}
	
	vbo.ReleaseChunk( elemCount * 4, 0 );

	PROFILER_BEGIN(gSubmitVBOProfileFlare, this)

	vbo.DrawChunk (channels);
	GPU_TIMESTAMP();

	PROFILER_END
}

// Matches the modes used in the inspector for texture layout:
enum FlareLayout {
	kLayoutLargeRestSmall = 0,
	kLayoutMixed,
	kLayout1x1,
	kLayout2x2,
	kLayout3x3,
	kLayout4x4,
};


// Get the UV coords for a flare element.
// Returns: outUV0 is top-left UV, outUV1 is bottom-right UV.
static void CalculateFlareGetUVCoords (int layout, int imageIndex, const Vector2f& pixOffset, Vector2f& outUV0, Vector2f& outUV1 )
{
	switch (layout)
	{
	case kLayoutLargeRestSmall:
		if (imageIndex != 0) {
			imageIndex -= 1; 
			int xImg = (imageIndex & 1);
			int yImg = (imageIndex >> 1);
			float imgSize = .5f;
			outUV0 = Vector2f( (float)(xImg+0) * imgSize, (float)(yImg+0) * imgSize * 0.5f + 0.5f) + pixOffset;
			outUV1 = Vector2f( (float)(xImg+1) * imgSize, (float)(yImg+1) * imgSize * 0.5f + 0.5f) - pixOffset;
		} else {
			outUV0 = Vector2f( 0.0f, 0.0f ) + pixOffset;
			outUV1 = Vector2f( 1.0f, 0.5f ) - pixOffset;
		}
		break;
		
	case kLayoutMixed:
		switch (imageIndex) {
		case 0:
			outUV0 = Vector2f( 0.0f, 0.0f ); // + pixOffset;
			outUV1 = Vector2f( 1.0f, 0.5f ) - pixOffset;
			break;
		case 1:
			outUV0 = Vector2f( 0.0f, 0.50f ); // + pixOffset;
			outUV1 = Vector2f( 0.5f, 0.75f ) - pixOffset;
			break;
		case 2:
			outUV0 = Vector2f( 0.0f, 0.75f ); // + pixOffset;
			outUV1 = Vector2f( 0.5f, 1.00f ) - pixOffset;
			break;
		default:
			imageIndex -= 3;
			int xImg = (imageIndex & 1);
			int yImg = (imageIndex >> 1);
			const float imgSize = 0.25f;
			outUV0 = Vector2f( (float)(xImg+0) * imgSize + 0.5f, (float)(yImg+0) * imgSize * 0.5f + 0.5f ) + pixOffset;
			outUV1 = Vector2f( (float)(xImg+1) * imgSize + 0.5f, (float)(yImg+1) * imgSize * 0.5f + 0.5f ) - pixOffset;
			break;
		}
		break;
		
	default:
		{
			// the rest of layouts are regular grids
			const int grid = layout - 1;
			int xImg = imageIndex % grid;
			int yImg = imageIndex / grid;
			float imgSize = 1.0f / (float)grid;
			outUV0 = Vector2f( (float)(xImg+0) * imgSize, (float)(yImg+0) * imgSize ) + pixOffset;
			outUV1 = Vector2f( (float)(xImg+1) * imgSize, (float)(yImg+1) * imgSize ) - pixOffset;
		}
		break;
	}
}


FlareManager &FlareManager::Get ()
{
	static FlareManager* s_FlareManager = NULL;

	// Create the material used for the flares
	if (!s_FlareMaterial)
	{
		Shader* shader = GetScriptMapper ().FindShader ("Hidden/Internal-Flare");
		if(shader)
			s_FlareMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}

	if (!s_FlareManager)
		s_FlareManager = new FlareManager();	
	
	return *s_FlareManager;
}

int FlareManager::AddFlare ()
{
	// Find unused flare, and put it there if there is one
	FlareList::iterator it, itEnd = m_Flares.end();
	int index = 0;
	for (it = m_Flares.begin(); it != itEnd; ++it, ++index)
	{
		if (!it->used)
		{
			it->used = true;
			// set brightness of this to zero in all cameras
			for (RendererList::iterator rit = m_Renderers.begin(); rit != m_Renderers.end(); ++rit)
			{
				DebugAssert (rit->second.size() == m_Flares.size());
				rit->second[index] = 0.0f;
			}
			return index;
		}
	}

	// No unused flare found; add new one
	index = m_Flares.size();	
	m_Flares.push_back (FlareEntry());
	// add zero brightness to all cameras
	for (RendererList::iterator rit = m_Renderers.begin(); rit != m_Renderers.end(); ++rit)
	{
		rit->second.push_back (0.0f);
		DebugAssert (rit->second.size() == m_Flares.size());
	}
	return index;
}

void FlareManager::UpdateFlare(	int handle, Flare *flare, const Vector3f &position, 
								bool infinite, float brightness, const ColorRGBAf &color,
								float fadeSpeed, UInt32 layers, UInt32 ignoredLayers
							  )
{
	Assert (handle >= 0 && handle < m_Flares.size());
	FlareEntry& i = m_Flares[handle];
	i.position = position;
	i.flare = flare;
	i.infinite = infinite;
	i.brightness = brightness;
	i.color = color;
	i.fadeSpeed = fadeSpeed;
	i.layers = layers;
	i.ignoredLayers = ignoredLayers;
}

void FlareManager::DeleteFlare (int handle)
{
	Assert (handle >= 0 && handle < m_Flares.size());
	// mark it as unused
	FlareEntry& flare = m_Flares[handle];
	flare.used = false;
}

void FlareManager::AddCamera (Camera &camera) {
	Assert (m_Renderers.find (&camera) == m_Renderers.end());
	m_Renderers[&camera] = std::vector<float> ();
	std::vector<float> &vec = m_Renderers[&camera];
	vec.resize (m_Flares.size(), 0.0f);
	Assert (vec.size() == m_Flares.size());
}

void FlareManager::RemoveCamera (Camera &camera) {
	RendererList::iterator i = m_Renderers.find (&camera);
	AssertIf (i == m_Renderers.end());
	m_Renderers.erase (i);
}

void FlareManager::Update () {
	Camera &cam = GetCurrentCamera();
	RendererList::iterator it = m_Renderers.find (&cam);
	if (it == m_Renderers.end())
	{
		AssertString ("Flare renderer to update not found");
		return;
	}

	Assert (it->second.size() == m_Flares.size());
	float *brightness = (it->second.size() ? &it->second[0] : NULL);
	
	float camFar = cam.GetFar();
	
	for (FlareList::iterator i = m_Flares.begin(); i != m_Flares.end(); ++i, ++brightness)
	{
		if (!i->used)
			continue;

		int layers = ~i->ignoredLayers;
		if (!(i->layers & layers))
			continue;

		float fadeAmt = i->fadeSpeed * (IsWorldPlaying() ? GetDeltaTime() : 1.0f);
		// We want flares to fade out at half speed of which they fade in
		float fadeOutAmt = fadeAmt * .5f;

		float targetVisible;
		Vector3f pos;
		if (!i->infinite)
		{
			pos = cam.WorldToViewportPoint (i->position);
			if( pos.z < camFar && (pos.x > 0.0f && pos.x < 1.0f) && (pos.y > 0.0f && pos.y < 1.0f) )
				targetVisible = 1.0f;
			else 	
				targetVisible = 0.0f;
		}
		else
		{
			pos = cam.WorldToViewportPoint( GetCurrentCamera().GetPosition () + i->position );
			if( pos.x > 0.0F && pos.x < 1.0F && pos.y > 0.0F && pos.y < 1.0F )
				targetVisible = 1.0f;
			else
				targetVisible = 0.0f;
		}
		
		if (targetVisible)
		{
			float t = 10000;
			Ray r;
			r.SetOrigin (cam.GetPosition());
			if (!i->infinite) {
				t = Magnitude (cam.GetPosition() - i->position);
				r.SetDirection ((i->position - cam.GetPosition()) / t);
			} else 
				r.SetDirection (-i->position);

			IRaycast* raycast = GetRaycastInterface ();
			HitInfo hit;
			if (raycast && raycast->Raycast (r, t, layers, hit))
				targetVisible = 0.0f;
		}
		
		if (targetVisible > *brightness) {
			*brightness += fadeAmt;
			if (*brightness > 1.0f)
				*brightness = 1.0f;
		} else if (targetVisible < *brightness) {
			*brightness -= fadeOutAmt;
			if (*brightness < 0.0f)
				*brightness = 0.0f;
		}
	}	
}

void FlareManager::RenderFlares ()
{
	if(!s_FlareMaterial)
		return;
	Shader* shader = s_FlareMaterial->GetShader();
	if (!shader)
		return;
	if(!GetCurrentCameraPtr())
		return;

	Camera &cam = GetCurrentCamera();
	float doubleNearDistance = cam.GetNear() * 2.0F;

	Update ();

	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity

	RendererList::iterator it = m_Renderers.find (&cam);
	Assert (it->second.size() == m_Flares.size());
	float *brightness = (it->second.size() ? &it->second[0] : NULL);

	Texture *lastTex = NULL;
	const ChannelAssigns* channels = NULL;
	Matrix4x4f cameraMatrix = GetCurrentCamera().GetWorldToCameraMatrix ();
	for (FlareList::iterator i = m_Flares.begin(); i != m_Flares.end(); ++i, ++brightness)
	{
		if (!i->used)
			continue;

		if ((*brightness) <= 0.0f)
			continue;

		Flare *fl = i->flare;
		if (!fl)
			continue;

		Vector3f pos;
		if (!i->infinite)
			pos = cameraMatrix.MultiplyPoint3 (i->position);
		else
			pos = cameraMatrix.MultiplyVector3 (-i->position * doubleNearDistance);

		Texture* flareTex = fl->GetTexture();

		if (!flareTex)
			continue;

		if (lastTex != flareTex)
		{
			lastTex = flareTex;
			ShaderLab::g_GlobalProperties->SetTexture(kSLPropFlareTexture, lastTex);
			channels = s_FlareMaterial->SetPassWithShader( 0, shader, 0 );
		}

		fl->Render (pos, *brightness * i->brightness, i->color, *channels);
	}
	
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}


IMPLEMENT_CLASS_HAS_INIT (LensFlare)
IMPLEMENT_OBJECT_SERIALIZE (LensFlare)

LensFlare::LensFlare (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Handle = -1;
	m_FadeSpeed = 3.0f;
}

LensFlare::~LensFlare ()
{
}

void LensFlare::Reset () {
	Super::Reset();
	m_Brightness = 1.0f;
	m_Color = ColorRGBAf (1,1,1,0);
	m_Directional = false;
	m_FadeSpeed = 3.0f;
	m_IgnoreLayers.m_Bits = kNoFXLayerMask | kIgnoreRaycastMask;
}

void LensFlare::InitializeClass () {
	REGISTER_MESSAGE_VOID (LensFlare, kTransformChanged, TransformChanged);
}

template<class TransferFunc>
void LensFlare::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_Flare);
	TRANSFER_SIMPLE (m_Color);
	TRANSFER (m_Brightness);
	TRANSFER (m_FadeSpeed);
	TRANSFER (m_IgnoreLayers);
	TRANSFER (m_Directional);
}

inline void LensFlare::UpdateFlare () {
	Vector3f pos;
	if (!m_Directional)
		pos = GetComponent(Transform).GetPosition();
	else
		pos = GetComponent(Transform).TransformDirection (Vector3f (0,0,1));

	GetFlareManager().UpdateFlare (	m_Handle, m_Flare, pos, m_Directional, 
									m_Brightness, m_Color, m_FadeSpeed,
									GetGameObject().GetLayerMask(), m_IgnoreLayers.m_Bits
								  );
}

void LensFlare::AwakeFromLoad (AwakeFromLoadMode awakeMode) {
	Super::AwakeFromLoad (awakeMode);
	if ((awakeMode & kDidLoadFromDisk) == 0 && m_Handle != -1)
		UpdateFlare ();
}

/// Update the flare in the FlareManager when the GO moves.
void LensFlare::TransformChanged () {
	if (m_Handle != -1)
		UpdateFlare ();
}

void LensFlare::SetBrightness (float brightness) {
	m_Brightness = brightness;
	SetDirty ();
	if (m_Handle != -1)
		UpdateFlare ();
}

void LensFlare::SetFadeSpeed (float fadeSpeed) {
	m_FadeSpeed = fadeSpeed;
	SetDirty ();
	if (m_Handle != -1)
		UpdateFlare ();
}

void LensFlare::SetColor (const ColorRGBAf& color) {
	m_Color = color;
	SetDirty ();
	if (m_Handle != -1)
		UpdateFlare ();
}


void LensFlare::SetFlare (Flare *flare) {
	m_Flare = flare;
	SetDirty ();
	if (m_Handle != -1)
		UpdateFlare ();
}

void LensFlare::AddToManager () {
	m_Handle = GetFlareManager().AddFlare ();
	UpdateFlare ();
}

void LensFlare::RemoveFromManager () {
	GetFlareManager().DeleteFlare (m_Handle);
	m_Handle = -1;
}

IMPLEMENT_CLASS (FlareLayer)

FlareLayer::FlareLayer (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{}

FlareLayer::~FlareLayer ()
{
}

void FlareLayer::AddToManager ()
{
	Camera &cam = GetComponent (Camera);
	GetFlareManager().AddCamera (cam);
}

void FlareLayer::RemoveFromManager ()
{
	Camera &cam = GetComponent (Camera);
	GetFlareManager().RemoveCamera (cam);
}

IMPLEMENT_CLASS (Flare)
IMPLEMENT_OBJECT_SERIALIZE (Flare)
