#include "UnityPrefix.h"
#include "TextMesh.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Font.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Filters/Mesh/LodMesh.h"

using namespace std;
namespace TextMesh_Static
{
static Font* gDefaultFont = NULL;
}


TextMesh::TextMesh (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Mesh = NULL;
	m_FontSize = 0;
	m_FontStyle = 0;
	m_RichText = true;
	m_Color = 0xffffffff;
}

TextMesh::~TextMesh ()
{
	DestroySingleObject(m_Mesh);
}

Mesh* TextMesh::GetMesh ()
{
	if (m_Mesh)
		return m_Mesh;
	else
	{
		m_Mesh = NEW_OBJECT (Mesh);
		m_Mesh->Reset();
		m_Mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

		m_Mesh->SetHideFlags(kHideAndDontSave);
		return m_Mesh;
	}
}

void TextMesh::Reset () {
	Super::Reset();

	m_OffsetZ = 0.0f;
	m_CharacterSize = 1.0f;
	m_Anchor = kUpperLeft;
	m_Alignment = kLeft;
	m_LineSpacing = 1.0F;
	m_TabSize = 4.0F;
}

Font * TextMesh::GetFont () const {
	using namespace TextMesh_Static;
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

void TextMesh::AwakeFromLoad(AwakeFromLoadMode awakeMode) {	
	Super::AwakeFromLoad (awakeMode);

	if (IsActive())
	{
		SetupMeshRenderer ();
		ApplyToMesh ();
	}
}

void TextMesh::SetText (const string& text) {
	if (m_Text != text)
	{
		m_Text = text;
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetFont (PPtr<Font> font) 
{
	if (m_Font != font)
	{
		m_Font = font;
		ApplyToMesh(); 
	}
	SetDirty();
}

void TextMesh::SetFontSize (int size) 
{ 
	if (m_FontSize != size)
	{
		m_FontSize = size; 
		ApplyToMesh();
	}
	SetDirty(); 
}

void TextMesh::SetFontStyle (int style) 
{ 
	if (m_FontStyle != style)
	{
		m_FontStyle = style; 
		ApplyToMesh();
	}
	SetDirty(); 
}

void TextMesh::SetAlignment(short alignment)
{ 
	if (m_Alignment != alignment)
	{
		m_Alignment = alignment; 
		ApplyToMesh();
	}
	SetDirty(); 
}

void TextMesh::SetOffsetZ(float offset)
{ 
	if (m_OffsetZ != offset)
	{
		m_OffsetZ = offset; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetAnchor(short anchor)
{ 
	if (m_Anchor != anchor)
	{
		m_Anchor = anchor; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetCharacterSize(float characterSize)
{ 
	if (m_CharacterSize != characterSize)
	{
		m_CharacterSize = characterSize; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetLineSpacing(float lineSpacing)
{ 
	if (m_LineSpacing != lineSpacing)
	{
		m_LineSpacing = lineSpacing; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetTabSize(float tabSize)
{ 
	if (m_TabSize != tabSize)
	{
		m_TabSize = tabSize; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetRichText(bool richText)
{ 
	if (m_RichText != richText)
	{
		m_RichText = richText; 
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetColor(const ColorRGBA32 color)
{
	if (m_Color != color)
	{
		m_Color = color;
		ApplyToMesh();
	}
	SetDirty();
}

void TextMesh::SetupMeshRenderer () {
	if (IsActive ())
	{
		MeshRenderer* renderer = QueryComponent(MeshRenderer);
		if (renderer)
			renderer->SetSharedMesh(GetMesh());
	}
}

void TextMesh::DidAddComponent () {
	if (IsActive ())
	{
		MeshRenderer* renderer = QueryComponent(MeshRenderer);
		if (renderer)
			renderer->SetSharedMesh(GetMesh());
	}
}

void TextMesh::ApplyToMesh ()
{
	Mesh * mesh = GetMesh();
	// Setup textmesh generator
	TextMeshGenerator2 &tmgen = TextMeshGenerator2::Get (UTF16String(m_Text.c_str()), GetFont (), (TextAnchor)m_Anchor, (TextAlignment)m_Alignment, 0, m_TabSize, m_LineSpacing, m_RichText, false, m_Color, m_FontSize, m_FontStyle);

	Vector2f size = tmgen.GetSize ();
	Vector2f offset = tmgen.GetTextOffset (Rectf (0, 0, -size.x, size.y * 2));
	switch (m_Alignment)
	{
		case kRight: offset.x += size.x; break;
		case kCenter: offset.x += size.x * 0.5f; break;
	}

	Mesh* srcMesh = tmgen.GetMesh ();
	Matrix4x4f m;
	Vector3f scale(m_CharacterSize, -m_CharacterSize, m_CharacterSize);
	scale *= GetFont()->GetDeprecatedPixelScale ();		
	m.SetTranslate (Vector3f(offset.x * scale.x, offset.y * -scale.y, m_OffsetZ));
	m.Scale(scale);
	mesh->CopyTransformed(*srcMesh, m);

	// Mesh CopyTransformed does not update local AABB! Kind of scared to change it there,
	// so instead manually transform the AABB here.
	const AABB& bounds = mesh->GetLocalAABB();
	AABB xformBounds;
	TransformAABB (bounds, m, xformBounds);
	mesh->SetLocalAABB (xformBounds);

	MeshRenderer* meshRenderer = QueryComponent(MeshRenderer);
	if (meshRenderer)
		meshRenderer->SetSharedMesh(mesh);
}

IMPLEMENT_CLASS_HAS_INIT (TextMesh)
IMPLEMENT_OBJECT_SERIALIZE (TextMesh)

template<class TransferFunction> inline
void TextMesh::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion (3);
	Super::Transfer (transfer);
	TRANSFER(m_Text);
	TRANSFER(m_OffsetZ);
	TRANSFER(m_CharacterSize);
	TRANSFER(m_LineSpacing);
	TRANSFER(m_Anchor);
	TRANSFER(m_Alignment);
	TRANSFER(m_TabSize);

	TRANSFER(m_FontSize);
	TRANSFER(m_FontStyle);

	TRANSFER(m_RichText);
	
	transfer.Align();
	
	TRANSFER (m_Font);
	TRANSFER (m_Color);

	
	#if UNITY_EDITOR
	// Renamed m_Settings to m_Font in version 1.2.2
	if (transfer.IsOldVersion(1))
	{
		transfer.Transfer(m_Font, "m_Settings");
	}
	
	// In version 1.5.0 line spacing is multiplicative instead of additive
	if (transfer.IsOldVersion(1) || transfer.IsOldVersion(2))
	{
		Font* font = GetFont();
		m_LineSpacing = (font->GetLineSpacing() + m_LineSpacing) / font->GetLineSpacing();
	}
	#endif
}

void TextMesh::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (TextMesh, kDidAddComponent, DidAddComponent);
}
