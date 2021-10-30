#include "UnityPrefix.h"
#include "OptimizedGUIBlock.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Shaders/Material.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/intshader.h"

static const char *kGUIBlockTextShaderString =
"Shader \"__GUI_SHADER_TEXTURECLIP_TEXT\" {\n"
"Properties { _MainTex (\"Texture\", 2D) = \"white\" {} }\n"
"SubShader { Tags { \"ForceSupported\" = \"True\" } Lighting Off Blend SrcAlpha OneMinusSrcAlpha Cull Off ZWrite Off Fog { Mode Off } ZTest Always\n"
" BindChannels { Bind \"vertex\", vertex Bind \"color\", color Bind \"TexCoord\", texcoord0 Bind \"TexCoord1\", texcoord1 }\n"
" Pass { SetTexture [_MainTex] { combine primary, primary * texture}\n"
" SetTexture [_GUIClipTexture2] { combine primary, previous * texture alpha} \n"
"}}\n"
// Really ancient sub shader for single-texture cards
"SubShader { Tags { \"ForceSupported\" = \"True\" } Lighting Off Cull Off ZWrite Off Fog { Mode Off } ZTest Always\n"
" Pass { BindChannels { Bind \"vertex\", vertex Bind \"color\", color Bind \"TexCoord\", texcoord0 }    ColorMask A SetTexture [_MainTex] { combine primary * texture alpha, texture alpha } } \n"	// Get the base alpha in
" Pass { BindChannels { Bind \"vertex\", vertex Bind \"TexCoord1\", texcoord0 }  ColorMask A Blend DstAlpha Zero SetTexture [_GUIClipTexture2] { combine texture alpha } } \n"			// Multiply in the clip alpha
" Pass { BindChannels { Bind \"vertex\", vertex Bind \"color\", color Bind \"TexCoord\", texcoord0 }     ColorMask RGB Blend DstAlpha OneMinusDstAlpha } \n" // Get color
"}\n"
"}";

static const char *kGUIBlockShaderString = "Shader \"__GUI_SHADER_TEXTURECLIP\" {\n"
" Properties { _MainTex (\"Texture\", Any) = \"white\" {} }\n"
" SubShader { Tags { \"ForceSupported\" = \"True\" } Lighting Off Blend SrcAlpha OneMinusSrcAlpha Cull Off ZWrite Off Fog { Mode Off } ZTest Always\n"
" BindChannels { Bind \"vertex\", vertex Bind \"color\", color Bind \"TexCoord\", texcoord }\n"
" Pass { SetTexture [_MainTex] { combine primary * texture }\n"
" SetTexture [_GUIClipTexture] { combine previous, previous * texture alpha } \n"		// Clipping texture - Gets bound to the clipping matrix from code.
"} }\n"
// Really ancient subshader for single-texture cards...
"SubShader { Tags { \"ForceSupported\" = \"True\" } Lighting Off Cull Off ZWrite Off Fog { Mode Off } ZTest Always\n"
" BindChannels { Bind \"vertex\", vertex Bind \"color\", color Bind \"TexCoord\", texcoord }\n"
" Pass { ColorMask A SetTexture [_MainTex] { combine primary * texture } } \n"	// Get the base alpha in
" Pass { ColorMask A Blend DstAlpha Zero SetTexture [_GUIClipTexture] { combine texture, texture alpha } } \n"			// Multiply in the clip alpha
" Pass { ColorMask RGB Blend DstAlpha OneMinusDstAlpha SetTexture [_MainTex] { combine primary * texture } } \n"			// Multiply in the clip alpha
"}\n"
"}";


static Material *kGUIBlockMaterial = NULL;
static Material *kGUIBlockTextMaterial = NULL;

static void InitializeGUIClipTexture2 () 
{
	InitializeGUIClipTexture();
	ShaderLab::g_GlobalProperties->SetTexture (ShaderLab::Property ("_GUIClipTexture2"),GUIStyle::GetClipTexture ());
}

static Material* GetTextMaterial () 
{
	if (!kGUIBlockTextMaterial) 
	{
		kGUIBlockTextMaterial = Material::CreateMaterial(kGUIBlockTextShaderString, Object::kHideAndDontSave);
		InitializeGUIClipTexture2 ();
	}
	
	return kGUIBlockTextMaterial;
}

static Material* GetMaterial () 
{
	if (!kGUIBlockMaterial) 
	{
		kGUIBlockMaterial = Material::CreateMaterial(kGUIBlockShaderString, Object::kHideAndDontSave);
		InitializeGUIClipTexture ();
	}
	
	return kGUIBlockMaterial;
}

const float kGUIClipTextureSize = 16.0f;
void CalcClipMatrix (const Rectf &rect, Matrix4x4f &clipMatrix)
{
	clipMatrix.SetIdentity();
	clipMatrix.Get (0,0) = (kGUIClipTextureSize-2.0f) / kGUIClipTextureSize / rect.width;
	clipMatrix.Get (1,1) = (kGUIClipTextureSize-2.0f) / kGUIClipTextureSize / rect.height;
	clipMatrix.Get (0,3) = -rect.x * clipMatrix.Get (0,0) + 1.0f / kGUIClipTextureSize;
	clipMatrix.Get (1,3) = -rect.y * clipMatrix.Get (1,1) + 1.0f / kGUIClipTextureSize;
	clipMatrix.Get (2,2) = clipMatrix.Get (3,3) = 0;		// Kill all perspective/depth: Just put 1 in ZW texcoords
	clipMatrix.Get (2,3) = clipMatrix.Get (3,3) = 1;		// Kill all perspective/depth: Just put 1 in ZW texcoords	
}

OptimizedGUIBlock::~OptimizedGUIBlock ()
{
	Clear ();
}

void OptimizedGUIBlock::Abandon ()
{
	SetCaptureGUIBlock(NULL);
}

struct GuiVertex
{
	Vector3f pos;
	ColorRGBA32 color;
	Vector2f uv0, uv1;
};

// Add the text to this drawcall.
void OptimizedGUIBlock::TextDrawCall::AddText (const Rectf &guiRect, TextMeshGenerator2 *tmgen, const Rectf &clipRect) 
{
	Mesh *srcMesh = tmgen->GetMesh();
	Mesh *dstMesh = m_Mesh;
	Vector2f off = tmgen->GetTextOffset (guiRect);
	Vector3f offset (off.x, off.y, 0.0f);

	// Make room for new vertices
	int currentSize = dstMesh->GetVertexCount();
	int srcSize = srcMesh->GetVertexCount();
	
	if (currentSize + srcSize > std::numeric_limits<UInt16>::max())
	{
		ErrorString("Optimized GUI Block text buffer too large. Not appending further text.");
		return;
	}
	
	unsigned vertexFormat = VERTEX_FORMAT4(Vertex, Color, TexCoord0, TexCoord1);
	
	dstMesh->ResizeVertices (currentSize + srcSize, vertexFormat);
	
	// Copy vertex data 
	// TODO: Apply GUIMatrix & Calc cliprects into UV1
	GuiVertex *dstVertex = (GuiVertex*)dstMesh->GetVertexDataPointer () + currentSize;
	
	Matrix4x4f clipMatrix;
	CalcClipMatrix (clipRect, clipMatrix);

	StrideIterator<Vector2f> srcUV = srcMesh->GetUvBegin(0);
	StrideIterator<ColorRGBA32> srcColor = srcMesh->GetColorBegin();
	for (StrideIterator<Vector3f> srcVert = srcMesh->GetVertexBegin(), srcVertEnd = srcMesh->GetVertexEnd (); srcVert != srcVertEnd; ++dstVertex, ++srcVert, ++srcUV, ++srcColor)
	{
		Vector3f const& src = *srcVert;
		dstVertex->pos = src + offset;
		dstVertex->uv0 = *srcUV;
		dstVertex->color = *srcColor;
		Vector3f texUv = (clipMatrix).MultiplyPoint3(src + offset);
		dstVertex->uv1 = Vector2f (texUv.x, texUv.y);
	}

	// copy over faces, re-assigning indices as we go up.
	int srcIndexCount = srcMesh->GetTotalndexCount();
	int dstIndexStart = dstMesh->GetTotalndexCount();

	dstMesh->SetIndicesComplex (NULL, dstIndexStart + srcIndexCount, 0, kPrimitiveTriangles, Mesh::k16BitIndices | Mesh::kDontAssignIndices | Mesh::kDontSupportSubMeshVertexRanges);

	const UInt16* src = srcMesh->GetSubMeshBuffer16(0);
	UInt16* dst = dstMesh->GetSubMeshBuffer16(0) + dstIndexStart;
	for (int i=0; i < srcIndexCount; i++) 
		dst[i] = src[i] + currentSize;
	dstMesh->UpdateSubMeshVertexRange (0);
	
	dstMesh->SetChannelsDirty (0, true);
	 
	/*
	 Mesh::TemporaryIndexContainer newTriangles;
	 Mesh::TemporaryIndexContainer srcTriangles;
	 dstMesh->GetTriangles (newTriangles, 0);
	 srcMesh->GetTriangles (srcTriangles, 0);
	 int newTrianglesCount = newTriangles.size ();
	 int srcTrianglesCount = srcTriangles.size();
	 AssertIf(newTrianglesCount != dstIndexStart);
	 AssertIf(srcIndexCount != srcTrianglesCount);
	 newTriangles.resize (newTrianglesCount + srcTrianglesCount);
	for (int i=0; i < srcTrianglesCount; i++) 
	{
		newTriangles[newTrianglesCount + i] = srcTriangles[i] + currentSize;
	}
	dstMesh->SetIndicesComplex (&newTriangles[0], newTrianglesCount + srcTrianglesCount, 0, 0);	
	 */
}

OptimizedGUIBlock::ImageDrawCall::ImageDrawCall (Texture *image, Rectf screenRect, const RectOffset &border, const ColorRGBAf &color, const Rectf &clipRect) 
{
	m_Image = image;
	m_ScreenRect = screenRect;
	m_Border = border;
	m_Color = color;
	m_ClipRect = clipRect;
}

OptimizedGUIBlock::CursorRect::CursorRect (const Rectf cursorRect, GUIView::MouseCursor cursor)
{
	m_CursorRect = cursorRect;
	m_Cursor = cursor;
}

void OptimizedGUIBlock::QueueText (const Rectf &guiRect, TextMeshGenerator2 *tmgen, const Rectf& clipRect) 
{
	Font *font = tmgen->GetFont ();
	TextDrawCallContainer::iterator i = m_TextDrawCalls.find (font);
	if (i == m_TextDrawCalls.end ()) 
	{
		m_TextDrawCalls[font] = TextDrawCall();		
		i = m_TextDrawCalls.find (font);
		Mesh *m = CreateObjectFromCode<Mesh>(kInstantiateOrCreateFromCodeAwakeFromLoad);
		m->SetHideFlags (Object::kHideAndDontSave);
		i->second.m_Mesh = m;
	}
	
	i->second.AddText (guiRect, tmgen, clipRect);
}

void OptimizedGUIBlock::QueueBackground (const Rectf &guiRect, Texture2D *texture, const RectOffset &border, const ColorRGBAf &color, const Rectf& clipRect)
{
	m_Backgrounds.push_back (ImageDrawCall (texture, guiRect, border, color, clipRect));
}

void OptimizedGUIBlock::QueueTexture (const Rectf &guiRect, Texture *texture, const ColorRGBAf &color, const Rectf& clipRect) 
{
	m_Images.push_back (ImageDrawCall (texture, guiRect, RectOffset (), color, clipRect));
}

void OptimizedGUIBlock::QueueCursorRect (const Rectf &cursorRect, GUIView::MouseCursor cursor)
{
	m_CursorRects.push_back (CursorRect (cursorRect, cursor));
}

void OptimizedGUIBlock::Clear ()
{
	for (std::map<PPtr<Font>, TextDrawCall>::const_iterator i = m_TextDrawCalls.begin(); i != m_TextDrawCalls.end(); i++)
	{
		DestroySingleObject(i->second.m_Mesh);
	}
	m_TextDrawCalls.clear ();
	m_Backgrounds.clear ();
	m_Images.clear ();
	m_CursorRects.clear ();
}

void OptimizedGUIBlock::ClearForReuse ()
{
	// Do not clear m_TextDrawCalls map, let further requests reuse it. Unused fonts will be removed from inside
	// of Execute.
	for (std::map<PPtr<Font>, TextDrawCall>::const_iterator i = m_TextDrawCalls.begin(); i != m_TextDrawCalls.end(); i++)
	{
		i->second.m_Mesh->Clear(true);
	}
	m_Backgrounds.clear ();
	m_Images.clear ();
	m_CursorRects.clear ();
}
void OptimizedGUIBlock::Execute () 
{
	// We're executing, so we don't want to capture
	SetCaptureGUIBlock (NULL);

	Rectf oldClipRect = GUIStyle::GetGUIClipRect ();
	// Draw all background stuff
	DrawTextures (m_Backgrounds);
	// Draw all textures
	DrawTextures (m_Images);

	GUIStyle::SetGUIClipRect (oldClipRect);
	
	// Apply all cursor rects
	ApplyCursorRects ();

	GfxDevice &device = GetGfxDevice ();

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr());
	
	// Draw all text
	std::map<PPtr<Font>, TextDrawCall>::iterator i, next;
	for (next = i = m_TextDrawCalls.begin(); i != m_TextDrawCalls.end();i=next)
	{
		Mesh* mesh = i->second.m_Mesh;
		Font *font = i->first;
		
		// Clean up fonts that are no longer in use.
		next++;
		if (mesh->GetVertexCount() == 0)
		{
			DestroySingleObject(mesh);
			m_TextDrawCalls.erase(i);
			continue;
		}
			
		Material *material = GetTextMaterial ();
		
		static SHADERPROP (Color);
		static SHADERPROP (MainTex);
		ShaderLab::PropertySheet& properties = material->GetWritableProperties();
		
		properties.SetTexture(kSLPropMainTex, font->GetTexture());
			
		int passCount = material->GetPassCount ();
		for (int j=0;j < passCount ;j++)
		{
			const ChannelAssigns* channels = material->SetPass (j, 0, false);
			
			DrawUtil::DrawMeshRaw (*channels, *mesh, 0);	
		}
	}

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}


void OptimizedGUIBlock::DrawTextures (std::vector <ImageDrawCall> &texturesToDraw)
{
	for (std::vector<ImageDrawCall>::iterator i = texturesToDraw.begin(); i != texturesToDraw.end(); i++) 
	{
		GUIStyle::SetGUIClipRect (i->m_ClipRect);
		Texture* image = i->m_Image;
		if (image)
			DrawGUITexture (i->m_ScreenRect, image, i->m_Border.left, i->m_Border.right, i->m_Border.top, i->m_Border.bottom, i->m_Color, GetMaterial ());
	}
}

void OptimizedGUIBlock::ApplyCursorRects () 
{
	GUIView *current = GUIView::GetCurrent ();
	for (std::vector<CursorRect>::const_iterator i = m_CursorRects.begin(); i != m_CursorRects.end(); i++)
	{
		current->AddCursorRect (i->m_CursorRect, i->m_Cursor);
	}
}

static OptimizedGUIBlock *s_CaptureGUIBlock = NULL;
OptimizedGUIBlock *GetCaptureGUIBlock () 
{
	return s_CaptureGUIBlock;
}
void SetCaptureGUIBlock (OptimizedGUIBlock* block) 
{
	s_CaptureGUIBlock = block;
}
