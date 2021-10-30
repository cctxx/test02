#include "UnityPrefix.h"
#include "GUIText.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Camera/CameraUtil.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Profiler/Profiler.h"

static Font* gDefaultFont = NULL;

GUIText::GUIText (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_FontSize = 0;
	m_FontStyle = 0;
	m_RichText = true;
	m_Color = 0xffffffff;
}

GUIText::~GUIText ()
{
}

Font * GUIText::GetFont () const {
	Font *f = m_Font;
	if (!f) {
		if (!gDefaultFont) 
			gDefaultFont = GetBuiltinResource<Font> (kDefaultFontName);
		return gDefaultFont;
	}
	else {
		return f;
	}

}

PROFILER_INFORMATION(gRenderGUIText, "GUIText.Render", kProfilerGUI)
PROFILER_INFORMATION(gSubmitVBOProfileGUIText, "Mesh.SubmitVBO", kProfilerRender)

void GUIText::RenderGUIElement (const Rectf& cameraRect)
{
	if (m_Text.empty ())
		return;
	
	PROFILER_AUTO(gRenderGUIText, this)

	pair<Font*, Material*> temp = GetFontAndMaterial ();
	Font* font = temp.first;
	Material* material = temp.second;
	if (font == NULL || material == NULL)
		return;
	
	TextMeshGenerator2 &tmgen = TextMeshGenerator2::Get (UTF16String(m_Text.c_str()), font, (TextAnchor)m_Anchor, (TextAlignment)m_Alignment, 0, m_TabSize, m_LineSpacing, m_RichText, m_PixelCorrect, m_Color, m_FontSize, m_FontStyle);
	
	GfxDevice& device = GetGfxDevice();
	DeviceMVPMatricesState preserveMVP; // save MVP matrices, restore when returning from this function!

	Vector2f size = tmgen.GetSize ();
	Vector2f offset = tmgen.GetTextOffset (Rectf (0, 0, -size.x, size.y * 2));
	switch (m_Alignment)
	{
		case kRight: offset.x += size.x; break;
		case kCenter:
			if (m_PixelCorrect)
				offset.x += Roundf(size.x * 0.5f);
			else
				offset.x += size.x * 0.5f;
			break;
	}
	Matrix4x4f textMatrix;
	if (!m_PixelCorrect)
	{
		Matrix4x4f ortho;
		ortho.SetOrtho( 0, 1, 0, 1, -1, 100 );
		device.SetProjectionMatrix (ortho);

		Transform& transform = GetComponent (Transform);
		Vector3f position = transform.GetPosition ();
		position.z = 0.0F;
		Vector3f scale = transform.GetWorldScaleLossy ();
		scale.x *= 0.05F * font->GetDeprecatedPixelScale ();
		scale.y *= -0.05F * font->GetDeprecatedPixelScale ();
		scale.z  = 1.0F;
		
		position.x += offset.x * scale.x;
		position.y -= offset.y * scale.y;

		textMatrix.SetTranslate( position );
		textMatrix.Scale( scale );
	}
	else {
		// Find out how large a rect the font should be rendered into, so that it is pixel correct. call the generator with this size.
		Rectf rectNoOffset = cameraRect;
		rectNoOffset.Move( -rectNoOffset.x, -rectNoOffset.y );
		LoadPixelMatrix( rectNoOffset, device, true, false );

		Transform& transform = GetComponent (Transform);
		Vector3f position = transform.GetPosition ();
		position.z = 0.0F;
		
		textMatrix.SetTranslate( Vector3f( Roundf (position.x * cameraRect.Width() + m_PixelOffset.x), Roundf(position.y * cameraRect.Height() + m_PixelOffset.y), 0.0f ) );
		textMatrix.Scale( Vector3f( 1.0F, -1.0F, 1.0f ) );
		
		textMatrix.Translate( Vector3f( offset.x, -offset.y, 0.0f ) );
	}
	device.SetViewMatrix( textMatrix.GetPtr() );
	
	int passCount = material->GetPassCount ();	
	for (int i=0;i < passCount ;i++)
	{
		const ChannelAssigns* channels = material->SetPass (i);
		tmgen.RenderRaw (*channels);
	}	
}


void DrawGUIText (const std::string& text, Font* font, Material* material)
{
	if (text.empty())
		return;
	if (font == NULL || material == NULL)
		return;

	PROFILER_AUTO(gRenderGUIText, NULL)

	TextMeshGenerator2 &tmgen = TextMeshGenerator2::Get (UTF16String(text.c_str()), font, kUpperLeft, kAuto, 0, 0, 1.0f, false, true, 0xffffffff, 0, 0);

	static SHADERPROP (MainTex);
	Texture* fontTexture = font->GetTexture();
	// In the case when font is a custum font, GetTexture() will return NULL, so in this case take the main texture from font's material
	if (fontTexture == NULL && font->GetMaterial())
	{
		fontTexture = font->GetMaterial()->GetTexture(kSLPropMainTex);
	}
	material->SetTexture(kSLPropMainTex, fontTexture);

	int passCount = material->GetPassCount ();	
	for (int i=0;i < passCount ;i++)
	{
		const ChannelAssigns* channels = material->SetPass (i);
		tmgen.RenderRaw (*channels);
	}	
}

Rectf GUIText::GetScreenRect (const Rectf& cameraRect)
{
	if (m_Text.empty ())
		return Rectf();

	Font* font = GetFontAndMaterial ().first;
	if (font == NULL)
		return Rectf();

	TextMeshGenerator2 &tmgen = TextMeshGenerator2::Get (UTF16String(m_Text.c_str()), font, (TextAnchor)m_Anchor, (TextAlignment)m_Alignment, 0, m_TabSize, m_LineSpacing, m_RichText, m_PixelCorrect, m_Color, m_FontSize, m_FontStyle);
	Vector2f size = tmgen.GetSize ();
	Vector2f offset = tmgen.GetTextOffset (Rectf (0, 0, -size.x, size.y * 2));
	Rectf rect (offset.x, -offset.y, size.x, size.y);

	Transform& transform = GetComponent (Transform);
	if (!m_PixelCorrect)
	{
		Vector3f position = transform.GetPosition ();
		position.z = 0.0F;
		Vector3f scale = transform.GetWorldScaleLossy ();
		scale.x *= 0.05F * font->GetDeprecatedPixelScale ();
		scale.y *= -0.05F * font->GetDeprecatedPixelScale ();
		scale.z = 1.0F;

		rect.Scale (scale.x, scale.y);
		rect.Move (position.x, position.y);
		Rectf windowRect = GetRenderManager ().GetWindowRect ();
		rect.Scale (windowRect.Width (), windowRect.Height ());
	}
	else
	{
		Rectf windowRect = GetRenderManager ().GetWindowRect ();

		Vector3f position = transform.GetPosition ();
		position.x = Roundf (position.x * windowRect.Width() + m_PixelOffset.x);
		position.y = Roundf (position.y * windowRect.Height() + m_PixelOffset.y);
		
		Vector3f scale = Vector3f (1.0F ,-1.0F,1);		

		rect.Scale (scale.x, scale.y);
		rect.Move (position.x, position.y);
	}
	if (rect.height < 0)
	{
		rect.height = -rect.height;
		rect.y -= rect.height;
	}
	return rect;
}

pair<Font*, Material*> GUIText::GetFontAndMaterial ()
{
	Font* font = m_Font;
	Material* material = m_Material;
	if (font != NULL && material == NULL)
		material = font->GetMaterial ();
	
	// Use default resource instead!
	if (font == NULL || material == NULL)
	{
		if (gDefaultFont == NULL)
		{
			gDefaultFont = GetBuiltinResource<Font> (kDefaultFontName);
			if (!gDefaultFont)
			{
				LogString ("Couldn't load default font!");
				return std::make_pair<Font*, Material*> (NULL, NULL);
			}
			if (!gDefaultFont->GetMaterial())
			{
				LogString ("Couldn't load default font material!");
				return std::make_pair<Font*, Material*> (NULL, NULL);
			}
		}
		
		if (font == NULL)
			font = gDefaultFont;
		if (material == NULL)
			material = gDefaultFont->GetMaterial();
	}
	
	return std::make_pair (font, material);
}

void GUIText::Reset ()
{
	Super::Reset ();
	m_PixelCorrect = true;
	m_Anchor = kUpperLeft;
	m_Alignment = kLeft;
	m_LineSpacing = 1.0F;
	m_TabSize = 4.0F;
	m_PixelOffset = Vector2f(0,0);
}

Material* GUIText::GetMaterial ()
{
	return GetFontAndMaterial ().second;
}

void GUIText::SetMaterial (Material* material)
{
	m_Material = material;
	SetDirty ();
}

void GUIText::SetFont (PPtr<Font> font)
{
	m_Font = font;
	SetDirty();
}


template<class TransferFunction> inline
void GUIText::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (3);
	TRANSFER_SIMPLE(m_Text);
	TRANSFER_SIMPLE(m_Anchor);
	TRANSFER_SIMPLE(m_Alignment);
	TRANSFER(m_PixelOffset);
	TRANSFER(m_LineSpacing);
	TRANSFER(m_TabSize);
	TRANSFER(m_Font);
	TRANSFER(m_Material);
	TRANSFER(m_FontSize);
	TRANSFER(m_FontStyle);
	TRANSFER(m_Color);

	TRANSFER(m_PixelCorrect);
	TRANSFER(m_RichText);

	#if UNITY_EDITOR
	// Explanation: in verson 1.2.2 we added pixel correct drawing. By default it is on.
	// for backwards compatbility we disable it
	if(transfer.IsOldVersion(1))
		m_PixelCorrect = false;

	// In version 1.5.0 line spacing is multiplicative instead of additive
	if(transfer.IsOldVersion(1) || transfer.IsOldVersion(2))
	{
		Font* font = GetFont();
		m_LineSpacing = (font->GetLineSpacing() + m_LineSpacing) / font->GetLineSpacing();
	}	
	#endif
}

IMPLEMENT_CLASS (GUIText)
IMPLEMENT_OBJECT_SERIALIZE (GUIText)
