#include "UnityPrefix.h"
#include "GUITexture.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/Material.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Misc/ResourceManager.h"

extern bool IsNPOTTextureAllowed(bool hasMipMap);

static SHADERPROP (MainTex);
static Shader* gGUI2DShader = NULL;
static Material* gGUI2DMaterial = NULL;


// can't do in InitializeClass because graphics might not be created yet
static void InitializeGUIShaders()
{
	if( !gGUI2DMaterial )
	{
		Shader* shader = GetBuiltinResource<Shader> ("Internal-GUITexture.shader");
		gGUI2DMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
		gGUI2DShader = gGUI2DMaterial->GetShader ();
	}
}

void GUITexture::InitializeClass ()
{
}

void GUITexture::CleanupClass ()
{
	gGUI2DShader = NULL;
	gGUI2DMaterial = NULL;
}

GUITexture::GUITexture (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Sheet = NULL;
	m_PrevTextureWidth = 0;
	m_PrevTextureHeight = 0;
	m_PrevTextureBaseLevel = Texture::GetMasterTextureLimit();
}

GUITexture::~GUITexture ()
{
	SAFE_RELEASE_LABEL(m_Sheet, kMemShader);
}

void GUITexture::BuildSheet()
{
	InitializeGUIShaders();

	Texture* texture = m_Texture;
	if( !texture )
		return;
	
	SAFE_RELEASE_LABEL(m_Sheet, kMemShader);
	bool is2D = (texture->GetDimension () == kTexDim2D);
	m_Sheet = gGUI2DShader->MakeProperties();
	m_Sheet->SetTexture (kSLPropMainTex, texture);

	ShaderLab::PropertySheet::TextureProperty* prop = m_Sheet->GetTextureProperty (kSLPropMainTex);
	if (prop && prop->scaleOffsetValue)
	{
		// for NPOT textures (in case it is not supported) override the GL texture name and texture scale
		// so that we use unscaled texture and ignore the padded dummy portion
		bool isNPOT = !IsPowerOfTwo(m_PrevTextureWidth) || !IsPowerOfTwo(m_PrevTextureHeight);
		if( is2D && isNPOT && !IsNPOTTextureAllowed(texture->HasMipMap()) )
		{
			// Need to take master texture limit into account because that
			// changes scaling...
			int baseMipLevel = Texture::GetMasterTextureLimit();
			if( !texture->HasMipMap() )
				baseMipLevel = 0;
			int texWidth = texture->GetDataWidth() >> baseMipLevel;
			int texHeight = texture->GetDataHeight() >> baseMipLevel; 
			int actualWidth = texture->GetGLWidth() >> baseMipLevel;
			int actualHeight = texture->GetGLHeight() >> baseMipLevel;
			// ...and shifting above might produce zeros
			float scaleX = (actualWidth > 0) ? float(texWidth) / float(actualWidth) : 1.0f;
			float scaleY = (actualHeight > 0) ? float(texHeight) / float(actualHeight) : 1.0f;
			scaleX *= texture->GetUVScaleX();
			scaleY *= texture->GetUVScaleY();
			prop->texEnv->OverrideTextureInfo( texture->GetUnscaledTextureID(), scaleX, scaleY );
			prop->scaleOffsetValue->Set (scaleX, scaleY, 0,0);
		}
		else
		{
			prop->scaleOffsetValue->Set (1,1,0,0);
		}
	}
}

void GUITexture::AwakeFromLoad (AwakeFromLoadMode awakeMode) {
	Super::AwakeFromLoad (awakeMode);
	BuildSheet ();
}

void GUITexture::SetTexture (Texture* tex)
{
	m_Texture = tex;
	if( tex )
	{
		m_PrevTextureWidth = tex->GetDataWidth();
		m_PrevTextureHeight = tex->GetDataHeight();
	}
	m_PrevTextureBaseLevel = Texture::GetMasterTextureLimit();
	if( tex && !tex->HasMipMap() )
		m_PrevTextureBaseLevel = 0;
	BuildSheet ();
}

Texture* GUITexture::GetTexture ()
{
	return m_Texture;
}

void GUITexture::Reset ()
{
	Super::Reset ();
	m_Color = ColorRGBAf (.5, .5, .5, .5);
	m_LeftBorder = m_RightBorder = m_TopBorder = m_BottomBorder = 0;
	m_PixelInset = Rectf (0,0,0,0);
}

void GUITexture::SetColor (const ColorRGBAf& color) {
	m_Color = color;
	BuildSheet ();
	SetDirty ();
}

struct GUITextureVertex {
	Vector3f vert;
	ColorRGBA32 color;
	Vector2f uv;
};

static bool FillGUITextureVBOChunk( const Rectf &screenRect, Texture* tex, const Rectf &sourceRect, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color )
{
	DebugAssertIf( !tex );
	Vector2f texScale (1.0f / (float)tex->GetDataWidth(), 1.0f / (float)tex->GetDataHeight());
	
	float x0 = RoundfToInt(screenRect.x);
	float x3 = RoundfToInt(screenRect.GetRight());
	
	int y0 = RoundfToInt(screenRect.y);
	int y3 = RoundfToInt(screenRect.GetBottom());

	float x1 = x0 + (float)leftBorder;
	float x2 = x3 - (float)rightBorder;
	int y1 = int(y0 + (float)bottomBorder);
	int y2 = int(y3 - (float)topBorder);
	
	float tx0 = sourceRect.x;
	float tx1 = tx0 + texScale.x * (float)leftBorder;
	float tx3 = sourceRect.GetRight();
	float tx2 = tx3 -  texScale.x * (float)rightBorder;
	
	float ty0 = sourceRect.y;
	float ty1 = ty0 + texScale.y * (float)bottomBorder;
	float ty3 = sourceRect.GetBottom();
	float ty2 = ty3 -  texScale.y * (float)topBorder;
	
	GUITextureVertex* vbPtr;
	unsigned short* ibPtr;
	DynamicVBO& vbo = GetGfxDevice().GetDynamicVBO();
	if( !vbo.GetChunk(
		(1<<kShaderChannelVertex) | (1<<kShaderChannelColor) | (1<<kShaderChannelTexCoord0),
		16,		// 16 vertices
		9*6,	// 9 quads
		DynamicVBO::kDrawIndexedTriangles,
		(void**)&vbPtr, (void**)&ibPtr ) )
	{
		return false;
	}

	vbPtr[ 0].vert.Set( x0, y0, 0.0f ); vbPtr[ 0].color = color; vbPtr[ 0].uv.Set( tx0, ty0 );
	vbPtr[ 1].vert.Set( x1, y0, 0.0f ); vbPtr[ 1].color = color; vbPtr[ 1].uv.Set( tx1, ty0 );
	vbPtr[ 2].vert.Set( x2, y0, 0.0f ); vbPtr[ 2].color = color; vbPtr[ 2].uv.Set( tx2, ty0 );
	vbPtr[ 3].vert.Set( x3, y0, 0.0f ); vbPtr[ 3].color = color; vbPtr[ 3].uv.Set( tx3, ty0 );
	vbPtr[ 4].vert.Set( x0, y1, 0.0f ); vbPtr[ 4].color = color; vbPtr[ 4].uv.Set( tx0, ty1 );
	vbPtr[ 5].vert.Set( x1, y1, 0.0f ); vbPtr[ 5].color = color; vbPtr[ 5].uv.Set( tx1, ty1 );
	vbPtr[ 6].vert.Set( x2, y1, 0.0f ); vbPtr[ 6].color = color; vbPtr[ 6].uv.Set( tx2, ty1 );
	vbPtr[ 7].vert.Set( x3, y1, 0.0f ); vbPtr[ 7].color = color; vbPtr[ 7].uv.Set( tx3, ty1 );
	vbPtr[ 8].vert.Set( x0, y2, 0.0f ); vbPtr[ 8].color = color; vbPtr[ 8].uv.Set( tx0, ty2 );
	vbPtr[ 9].vert.Set( x1, y2, 0.0f ); vbPtr[ 9].color = color; vbPtr[ 9].uv.Set( tx1, ty2 );
	vbPtr[10].vert.Set( x2, y2, 0.0f ); vbPtr[10].color = color; vbPtr[10].uv.Set( tx2, ty2 );
	vbPtr[11].vert.Set( x3, y2, 0.0f ); vbPtr[11].color = color; vbPtr[11].uv.Set( tx3, ty2 );
	vbPtr[12].vert.Set( x0, y3, 0.0f ); vbPtr[12].color = color; vbPtr[12].uv.Set( tx0, ty3 );
	vbPtr[13].vert.Set( x1, y3, 0.0f ); vbPtr[13].color = color; vbPtr[13].uv.Set( tx1, ty3 );
	vbPtr[14].vert.Set( x2, y3, 0.0f ); vbPtr[14].color = color; vbPtr[14].uv.Set( tx2, ty3 );
	vbPtr[15].vert.Set( x3, y3, 0.0f ); vbPtr[15].color = color; vbPtr[15].uv.Set( tx3, ty3 );

	static UInt16 ib[9*6] = {
		0, 4, 1, 1, 4, 5,
		1, 5, 2, 2, 5, 6,
		2, 6, 3, 3, 6, 7,
		4, 8, 5, 5, 8, 9,
		5, 9, 6, 6, 9, 10,
		6, 10, 7, 7, 10, 11,
		8, 12, 9, 9, 12, 13,
		9, 13, 10, 10, 13, 14,
		10, 14, 11, 11, 14, 15,
	};
	memcpy( ibPtr, ib, sizeof(ib) );
	vbo.ReleaseChunk( 16, 9*6 );

	return true;
}

PROFILER_INFORMATION(gRenderGUITexture, "GUITexture.Render", kProfilerGUI)
PROFILER_INFORMATION(gSubmitVBOProfileGUITexture, "Mesh.SubmitVBO", kProfilerRender)

void GUITexture::DrawGUITexture (const Rectf &bounds)
{
	PROFILER_AUTO(gRenderGUITexture, NULL)
	
	InitializeGUIShaders();

	Shader* shader = gGUI2DShader;
	
	GfxDevice& device = GetGfxDevice();
	DynamicVBO& vbo = device.GetDynamicVBO();
	
	ColorRGBA32 color = m_Color;
	color = device.ConvertToDeviceVertexColor(color);
	
	if( !FillGUITextureVBOChunk( bounds, m_Texture, Rectf(0,0,1,1), m_LeftBorder, m_RightBorder, m_TopBorder, m_BottomBorder, color ) )
		return;
	const ShaderLab::SubShader& ss = shader->GetShaderLabShader()->GetActiveSubShader();
	const int passCount = ss.GetValidPassCount();
	for( int i = 0; i != passCount; ++i )
	{
		const ChannelAssigns* channels = shader->SetPass (0, i, 0, m_Sheet);

		PROFILER_BEGIN(gSubmitVBOProfileGUITexture, this)
		vbo.DrawChunk (*channels);
		GPU_TIMESTAMP();
		PROFILER_END
	}
}

void GUITexture::RenderGUIElement (const Rectf& cameraRect)
{
	Texture* tex = m_Texture;
	if( !tex )
		return;
	
	// Before rendering check whether something serious has changed:
	// * texture could have changed from POT to NPOT, or the size may have changed (at least in the editor)
	// * global texture limit might have changed
	int texWidth = tex->GetDataWidth();
	int texHeight = tex->GetDataHeight();
	int masterTexLimit = Texture::GetMasterTextureLimit();
	if( !tex->HasMipMap() )
		masterTexLimit = 0;
	if( texWidth != m_PrevTextureWidth || texHeight != m_PrevTextureHeight || m_PrevTextureBaseLevel != masterTexLimit )
	{
		m_PrevTextureWidth = texWidth;
		m_PrevTextureHeight = texHeight;
		m_PrevTextureBaseLevel = masterTexLimit;
		BuildSheet();
	}

	GfxDevice& device = GetGfxDevice();
	DeviceMVPMatricesState preserveMVP;
	
	Rectf rectNoOffset = cameraRect;
	rectNoOffset.Move( -rectNoOffset.x, -rectNoOffset.y );
	LoadPixelMatrix( rectNoOffset, device, true, false );

	Rectf drawBox = CalculateDrawBox (cameraRect);
	DrawGUITexture (drawBox);
}

Rectf GUITexture::CalculateDrawBox (const Rectf& screenViewportRect)
{
	Transform& transform = GetComponent (Transform);
	Rectf drawBox;
	
	Vector3f position = transform.GetPosition ();
	Vector3f scale = transform.GetWorldScaleLossy ();
	
	float xmin = position.x - scale.x * 0.5F;
	float xmax = position.x + scale.x * 0.5F;

	float ymin = position.y - scale.y * 0.5F;
	float ymax = position.y + scale.y * 0.5F;

	drawBox.x = screenViewportRect.Width() * xmin + m_PixelInset.x;
	drawBox.SetRight( screenViewportRect.Width() * xmax + m_PixelInset.GetRight() );
	drawBox.y = screenViewportRect.Height() * ymin + m_PixelInset.y;
	drawBox.SetBottom( screenViewportRect.Height() * ymax + m_PixelInset.GetBottom() );
	
	return drawBox;
}

Rectf GUITexture::GetScreenRect (const Rectf& cameraRect)
{
	return CalculateDrawBox (cameraRect);
}

template<class TransferFunction>
void GUITexture::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_Texture);
	TRANSFER_SIMPLE (m_Color);

	TRANSFER (m_PixelInset);

	TRANSFER (m_LeftBorder);
	TRANSFER (m_RightBorder);
	TRANSFER (m_TopBorder);
	TRANSFER (m_BottomBorder);
}

IMPLEMENT_CLASS_HAS_INIT (GUITexture)
IMPLEMENT_OBJECT_SERIALIZE (GUITexture)

void DrawGUITexture (const Rectf &screenRect, Texture* texture, ColorRGBA32 color, Material* material) {
	DrawGUITexture (screenRect, texture, 0,0,0,0, color, material);
}
void DrawGUITexture (const Rectf &screenRect, Texture* texture, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color, Material* material) {
	DrawGUITexture (screenRect, texture, Rectf (0,0,1,1), leftBorder, rightBorder, topBorder, bottomBorder, color, material);
}

void HandleGUITextureProps( ShaderLab::PropertySheet *sheet, Texture *texture )
{
	sheet->SetTexture (kSLPropMainTex, texture);

	int texWidth = texture->GetDataWidth();
	int texHeight = texture->GetDataHeight();

	ShaderLab::PropertySheet::TextureProperty* prop = sheet->GetTextureProperty (kSLPropMainTex);
	if (prop && prop->scaleOffsetValue)
	{
		float uvScaleX = texture->GetUVScaleX();
		float uvScaleY = texture->GetUVScaleY();

		// for NPOT textures (in case it is not supported) override the GL texture name and texture scale
		// so that we use unscaled texture and ignore the padded dummy portion
		bool isNPOT = !IsPowerOfTwo(texWidth) || !IsPowerOfTwo(texHeight);
		if( texture->GetDimension() == kTexDim2D && isNPOT && !IsNPOTTextureAllowed(texture->HasMipMap()) )
		{
			// Need to take master texture limit into account because that
			// changes scaling...
			int baseMipLevel = Texture::GetMasterTextureLimit();
			if (!texture->HasMipMap())
				baseMipLevel = 0;
			texWidth = texWidth >> baseMipLevel;
			texHeight = texHeight >> baseMipLevel; 
			int actualWidth = texture->GetGLWidth() >> baseMipLevel;
			int actualHeight = texture->GetGLHeight() >> baseMipLevel;
			// ...and shifting above might produce zeros
			float scaleX = (actualWidth > 0) ? float(texWidth) / float(actualWidth) : 1.0f;
			float scaleY = (actualHeight > 0) ? float(texHeight) / float(actualHeight) : 1.0f;
			scaleX *= uvScaleX;
			scaleY *= uvScaleY;
			prop->texEnv->OverrideTextureInfo( texture->GetUnscaledTextureID(), scaleX, scaleY );
			prop->scaleOffsetValue->Set (scaleX, scaleY, 0, 0);
		}
		else
		{
			prop->scaleOffsetValue->Set (uvScaleX,uvScaleY,0,0);
		}
	}
}

// Fills out the VBO with the new inverted-coordinate GUI rectangle

static bool FillGUITextureVBOChunkInverted( const Rectf &screenRect, Texture* tex, const Rectf &sourceRect, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color, UInt32* outTriangles )
{
	DebugAssertIf( !tex );
	Vector2f texScale (1.0f / (float)tex->GetDataWidth(), 1.0f / (float)tex->GetDataHeight());

	float x0 = RoundfToInt(screenRect.x);
	float x3 = RoundfToInt(screenRect.GetRight());
	float y0 = RoundfToInt(screenRect.GetBottom());
	float y3 = RoundfToInt(screenRect.y);

	float tx0 = sourceRect.x;
	float tx3 = sourceRect.GetRight();
	float ty0 = sourceRect.y;
	float ty3 = sourceRect.GetBottom();

	GUITextureVertex* vbPtr;
	unsigned short* ibPtr;
	DynamicVBO& vbo = GetGfxDevice().GetDynamicVBO();

	if ((leftBorder|rightBorder|topBorder|bottomBorder) == 0)
	{
		// no borders, texture is just a quad
		if( !vbo.GetChunk(
			(1<<kShaderChannelVertex) | (1<<kShaderChannelColor) | (1<<kShaderChannelTexCoord0),
			4,		// 4 vertices
			6,		// 1 quad
			DynamicVBO::kDrawIndexedTriangles,
			(void**)&vbPtr, (void**)&ibPtr ) )
		{
			return false;
		}
		
		vbPtr[ 0].vert.Set( x0, y0, 0.0f ); vbPtr[ 0].color = color; vbPtr[ 0].uv.Set( tx0, ty0 );
		vbPtr[ 1].vert.Set( x3, y0, 0.0f ); vbPtr[ 1].color = color; vbPtr[ 1].uv.Set( tx3, ty0 );
		vbPtr[ 2].vert.Set( x0, y3, 0.0f ); vbPtr[ 2].color = color; vbPtr[ 2].uv.Set( tx0, ty3 );
		vbPtr[ 3].vert.Set( x3, y3, 0.0f ); vbPtr[ 3].color = color; vbPtr[ 3].uv.Set( tx3, ty3 );
		static UInt16 ib[1*6] = {
			0, 2, 1, 1, 2, 3,
		};
		memcpy( ibPtr, ib, sizeof(ib) );
		vbo.ReleaseChunk( 4, 6 );
		*outTriangles = 2;
	}
	else
	{
		// with borders, texture is a 3x3 quad grid
		float x1 = x0 + (float)leftBorder;
		float x2 = x3 - (float)rightBorder;
		float y1 = y0 - (float)topBorder;
		float y2 = y3 + (float)bottomBorder;

		float tx1 = tx0 + texScale.x * (float)leftBorder;
		float tx2 = tx3 - texScale.x * (float)rightBorder;
		float ty1 = ty0 + texScale.y * (float)topBorder;
		float ty2 = ty3 - texScale.y * (float)bottomBorder;
	
		if( !vbo.GetChunk(
			(1<<kShaderChannelVertex) | (1<<kShaderChannelColor) | (1<<kShaderChannelTexCoord0),
			16,		// 16 vertices
			9*6,	// 9 quads
			DynamicVBO::kDrawIndexedTriangles,
			(void**)&vbPtr, (void**)&ibPtr ) )
		{
			return false;
		}
		
		vbPtr[ 0].vert.Set( x0, y0, 0.0f ); vbPtr[ 0].color = color; vbPtr[ 0].uv.Set( tx0, ty0 );
		vbPtr[ 1].vert.Set( x1, y0, 0.0f ); vbPtr[ 1].color = color; vbPtr[ 1].uv.Set( tx1, ty0 );
		vbPtr[ 2].vert.Set( x2, y0, 0.0f ); vbPtr[ 2].color = color; vbPtr[ 2].uv.Set( tx2, ty0 );
		vbPtr[ 3].vert.Set( x3, y0, 0.0f ); vbPtr[ 3].color = color; vbPtr[ 3].uv.Set( tx3, ty0 );
		vbPtr[ 4].vert.Set( x0, y1, 0.0f ); vbPtr[ 4].color = color; vbPtr[ 4].uv.Set( tx0, ty1 );
		vbPtr[ 5].vert.Set( x1, y1, 0.0f ); vbPtr[ 5].color = color; vbPtr[ 5].uv.Set( tx1, ty1 );
		vbPtr[ 6].vert.Set( x2, y1, 0.0f ); vbPtr[ 6].color = color; vbPtr[ 6].uv.Set( tx2, ty1 );
		vbPtr[ 7].vert.Set( x3, y1, 0.0f ); vbPtr[ 7].color = color; vbPtr[ 7].uv.Set( tx3, ty1 );
		vbPtr[ 8].vert.Set( x0, y2, 0.0f ); vbPtr[ 8].color = color; vbPtr[ 8].uv.Set( tx0, ty2 );
		vbPtr[ 9].vert.Set( x1, y2, 0.0f ); vbPtr[ 9].color = color; vbPtr[ 9].uv.Set( tx1, ty2 );
		vbPtr[10].vert.Set( x2, y2, 0.0f ); vbPtr[10].color = color; vbPtr[10].uv.Set( tx2, ty2 );
		vbPtr[11].vert.Set( x3, y2, 0.0f ); vbPtr[11].color = color; vbPtr[11].uv.Set( tx3, ty2 );
		vbPtr[12].vert.Set( x0, y3, 0.0f ); vbPtr[12].color = color; vbPtr[12].uv.Set( tx0, ty3 );
		vbPtr[13].vert.Set( x1, y3, 0.0f ); vbPtr[13].color = color; vbPtr[13].uv.Set( tx1, ty3 );
		vbPtr[14].vert.Set( x2, y3, 0.0f ); vbPtr[14].color = color; vbPtr[14].uv.Set( tx2, ty3 );
		vbPtr[15].vert.Set( x3, y3, 0.0f ); vbPtr[15].color = color; vbPtr[15].uv.Set( tx3, ty3 );
		static UInt16 ib[9*6] = {
			0, 4, 1, 1, 4, 5,			// Top-left
			1, 5, 2, 2, 5, 6,			// Top-mid
			2, 6, 3, 3, 6, 7,			// Top-right
			4, 8, 5, 5, 8, 9,			// mid-left
			5, 9, 6, 6, 9, 10,			// mid-center
			6, 10, 7, 7, 10, 11,		// mid-right
			8, 12, 9, 9, 12, 13,		// bottom-left
			9, 13, 10, 10, 13, 14,		// bottom-mid
			10, 14, 11, 11, 14, 15,	// bottom-right
		};
		memcpy( ibPtr, ib, sizeof(ib) );
		vbo.ReleaseChunk( 16, 9*6 );
		*outTriangles = 9 * 2;
	}

	return true;
}

void DrawGUITexture (const Rectf &screenRect, Texture* tex, const Rectf &sourceRect, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color, Material* material)
{
	InitializeGUIShaders();

	if( !tex )
	{
		ErrorString ("DrawGUITexture: texture is null");
		return;
	}

	GfxDevice& device = GetGfxDevice();
	color = device.ConvertToDeviceVertexColor( color );
	UInt32 triCount;
		
	// temp workaround to flip Y-axis. should probably be done in FillGUITextureVBOChunk - but for now we do it here
	if( !FillGUITextureVBOChunkInverted( screenRect, tex, sourceRect, leftBorder, rightBorder, bottomBorder, topBorder, color, &triCount ) )
		return;

	if (material)
	{
		HandleGUITextureProps( &material->GetWritableProperties(), tex );
	}
	else
	{
		HandleGUITextureProps( &gGUI2DMaterial->GetWritableProperties(), tex );
		material = gGUI2DMaterial;
	}

	int passCount = material->GetPassCount();
	
	DynamicVBO& vbo = device.GetDynamicVBO();

	for( int i = 0; i < passCount; ++i)
	{
		const ChannelAssigns* channels = material->SetPass (i, 0, false);

		PROFILER_BEGIN(gSubmitVBOProfileGUITexture, NULL);
		vbo.DrawChunk (*channels);
		GPU_TIMESTAMP();
		PROFILER_END
	}
}
