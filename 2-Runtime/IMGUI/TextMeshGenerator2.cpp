#include "UnityPrefix.h"
#include "TextMeshGenerator2.h"
#include "TextFormatting.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <limits>

#define TEXTDEBUG 0

#define kMaxMaterials 8

using std::pair;
typedef std::vector<TextMeshGenerator2*/*, main_thread_allocator<TextMeshGenerator2*> */> Generators;
static  Generators s_Generators;
static Font *gDefaultFont = NULL;
const int kKillFrames = 5;

static const TextAlignment kTextAnchorToAlignment[] = {
	kLeft, kCenter, kRight, kLeft, kCenter, kRight, kLeft, kCenter, kRight
};

// The global getter function
TextMeshGenerator2 &TextMeshGenerator2::Get (const UTF16String &text, Font *font, TextAnchor anchor, TextAlignment alignment, float wordWrapWidth, float tabSize, float lineSpacing, bool richText, bool pixelCorrect, ColorRGBA32 color, int fontSize, int fontStyle) {
	if (font == NULL) {
		if (!gDefaultFont) 
			gDefaultFont = GetBuiltinResource<Font> (kDefaultFontName);
		font = gDefaultFont;
	}

	// maybe not the best way around, but we want to report error if changing size/style for non-dynamic fonts
	// but in the same time don't want to spam console because we'll continue to work just fine
	bool reportNonDynamicFontError = false;

	bool fontDynamic = font->GetConvertCase() == Font::kDynamicFont;
	if( !fontDynamic )
	{
		if( fontSize != 0 || fontStyle != kStyleDefault )
			reportNonDynamicFontError = true;

		fontSize  = 0;
		fontStyle = kStyleDefault;
	}

	if (alignment == kAuto)
		alignment = kTextAnchorToAlignment[anchor];
	// TODO: Use some sort of hash to quickly find candidate meshes
	// in practice a lot of the parameters will be the same, with only the string really differing
	for (Generators::const_iterator i = s_Generators.begin(); i != s_Generators.end(); i++) {
		TextMeshGenerator2 *gen = *i;
		if (	gen->m_Font.GetInstanceID() == font->GetInstanceID() && 
			(anchor == kDontCare || (gen->m_Anchor == anchor && gen->m_Alignment == alignment)) && 
			gen->m_WordWrapWidth == wordWrapWidth && 
			gen->m_TabSize == tabSize && 
			gen->m_LineSpacing == lineSpacing && 
			gen->m_UTF16Text ==  text &&
			gen->m_FontSize == fontSize &&
			gen->m_FontStyle == fontStyle &&
			gen->m_RichText == richText &&
			gen->m_PixelCorrect == pixelCorrect &&
			gen->m_Color == color
			) {
				// we mark it as dirty the moment we look at it.
				// Doing it during rendering only makes us regenerate strings only for wordwrap & scrollview size negotiations.
				gen->m_LastUsedFrame = GetTimeManager().GetRenderFrameCount();
				return *gen;
		}
	}

	// report error only now - it will fire up the single time we create text mesh
	if(reportNonDynamicFontError)
	{
		// TODO: font name?
		WarningString("Font size and style overrides are only supported for dynamic fonts.");
	}

	// We don't have one already so we need to generate it.
	// Sanitize anchor if we don't care. We just pick one for the generation. 
	// If it gets rendered with another, this mesh will get garbage collected later and the real one used instead.
	if (anchor == kDontCare)	
		anchor = kUpperLeft;

	TextMeshGenerator2 *gen = new TextMeshGenerator2 (text, font, anchor, alignment, wordWrapWidth, tabSize, lineSpacing, richText, pixelCorrect, color, fontSize, fontStyle);
	gen->Generate ();
	s_Generators.push_back (gen);
	return *gen;
}

float TextMeshGenerator2::Roundf (float x)
{
	if (m_PixelCorrect)
		return ::Roundf(x);
	else
		return x;
}
/// Get the cursor position
Vector2f TextMeshGenerator2::GetCursorPosition (const Rectf &screenRect, int textPosition) {
	// clamp the text position to valid ranges
	if (textPosition < 0)
		textPosition = 0;
	else if (textPosition > m_UTF16Text.length) 
		textPosition = m_UTF16Text.length;

	// make sure we don't access out of bounds position when the string is too large to be processed completely.
	if(textPosition * 4 + 4 > std::numeric_limits<UInt16>::max())
		textPosition = (std::numeric_limits<UInt16>::max() / 4) - 1;

	return m_CursorPos[textPosition] + GetTextOffset (screenRect);
}


// Compare 2 vertex positions.
// this function correctly handles rounding so you can click on the first half of a letter and be taken to the front...
inline int ComparePositions2 (Vector2f *arr, int maxCount, int idx1, const Vector2f &p2, float lineHeight) {
	// Find out if we're line(s) above or below
	Vector2f p = arr[idx1];
	if (p.y <= p2.y - lineHeight)
		return -1;
	if (p.y > p2.y)
		return 1;
	// ok, we're on the same line here, so compare x positions...

	// We need to get the middle of letters
	Vector2f curr = arr[idx1];
	Vector2f nextPoint = arr[idx1 != maxCount ? idx1+ 1 : maxCount];
	// If the next letter is on the line after, we cheat by moving the point way to the right, so hit detection will work...
	float next;
	if (nextPoint.y == curr.y)
		next = nextPoint.x;
	else
		next = 10000;


	float rightBorder = (next + curr.x) * .5f;
	if (rightBorder < p2.x)
		return -1;

	Vector2f prevPoint = arr[idx1 != 0 ? idx1 - 1 : 0];
	float prev;
	// If the previous letter is on the line before, we cheat by moving the point way to the left, so hit detection will work...
	if (prevPoint.y == curr.y)
		prev = prevPoint.x;
	else 
		prev = -10000;
	float leftBorder = (prev + curr.x) * .5f;
	if (leftBorder > p2.x)
		return 1;
	return 0;
}

int TextMeshGenerator2::GetCursorIndexAtPosition (const Rectf &screenRect, const Vector2f &pos) {
	int start = 0;
	int maxCount = m_CursorPos.size () - 1;
	int end = maxCount;
	Vector2f localPos = pos - GetTextOffset (screenRect);
	Vector2f *arr = &m_CursorPos[0];

	Font *font = GetFont();
	float lineSize = Roundf (font->GetLineSpacing(m_FontSize));

	// Binary search to find out where we clicked
	while (start <= end) {
		int mid = ((start + end) >> 1);
		switch (ComparePositions2 (arr, maxCount, mid, localPos, lineSize)) {
		case 0:
			return mid;
		case -1:
			start = mid + 1;
			break;
		case 1:
			end = mid - 1;
			break;
		}
	}
	// We didn't find anything, this is the closest match
	if (end < 0)
		end = 0;	// Sanity.
	return end;
}

PROFILER_INFORMATION(gTextMeshGenerator, "TextRendering.Cleanup", kProfilerRender)

	// clear the cache for anything that wasn't rendered in the last frame.
	void TextMeshGenerator2::GarbageCollect ()
{
	int currentFrame = GetTimeManager().GetRenderFrameCount();

#if TEXTDEBUG
	printf_console ("TextMesh GarbageCollect\n");
#endif

	// We're removing from the vector, so we need to iterate backwards through it
	for (int i = s_Generators.size(); i--;)
	{
		PROFILER_AUTO(gTextMeshGenerator, NULL)
			TextMeshGenerator2 *gen = s_Generators[i];
		if (currentFrame - gen->m_LastUsedFrame > kKillFrames)
		{
#if TEXTDEBUG
			printf_console ("\tTextMesh killed - %d chars\n", gen->m_CursorPos.size() - 1);
#endif
			delete gen;
			s_Generators.erase(s_Generators.begin() + i);
		} 
	}
}

void TextMeshGenerator2::Flush ()
{
	for (int i = s_Generators.size(); i--;)
	{
		TextMeshGenerator2 *gen = s_Generators[i];
		delete gen;
	}
	s_Generators.clear ();
}


TextMeshGenerator2::TextMeshGenerator2 (const UTF16String &text, Font *font, TextAnchor anchor, TextAlignment alignment, float wordWrapWidth, float tabSize, float lineSpacing, bool richText, bool pixelCorrect, ColorRGBA32 color, int fontSize, int fontStyle) :
m_UTF16Text (text)
{
	m_Font      = font;
	m_FontSize  = fontSize;
	m_FontStyle = fontStyle;

	m_Anchor = anchor;
	m_Alignment = alignment;
	m_WordWrapWidth = wordWrapWidth;
	m_TabSize = tabSize;
	m_LastUsedFrame = 0;
	m_LineSpacing = lineSpacing;
	m_Mesh = NULL;
	m_RichText = richText;
	m_PixelCorrect = pixelCorrect;
	m_Color = color;

#if TEXTDEBUG
	printf_console ("Creating textMesh\n");
#endif
}


TextMeshGenerator2::~TextMeshGenerator2 ()
{
	if (m_Mesh)
		DestroySingleObject (&*m_Mesh);
}

Vector2f TextMeshGenerator2::GetTextOffset (const Rectf &rect)
{
	Vector2f offset;
	switch (m_Anchor) {
	case kUpperLeft:
		offset.x = Roundf (rect.x);
		offset.y = Roundf (rect.y);
		break;
	case kUpperCenter:
		offset.x = Roundf (rect.x + rect.width *.5f);
		offset.y = Roundf (rect.y);
		break;
	case kUpperRight:
		offset.x = Roundf (rect.GetRight());
		offset.y = Roundf (rect.y);
		break;
	case kMiddleLeft:
		offset.x = Roundf (rect.x);
		offset.y = Roundf ((rect.GetBottom() + rect.y - m_Rect.Height ()) * .5f);
		break;
	case kMiddleCenter:
		offset.x = Roundf (rect.x + rect.width *.5f);
		offset.y = Roundf ((rect.GetBottom() + rect.y - m_Rect.Height ()) * .5f);		
		break;
	case kMiddleRight:
		offset.x = Roundf (rect.GetRight());
		offset.y = Roundf ((rect.GetBottom() + rect.y - m_Rect.Height ()) * .5f);
		break;
	case kLowerLeft:
		offset.x = Roundf (rect.x);
		offset.y = Roundf (rect.GetBottom() - m_Rect.Height ());
		break;
	case kLowerCenter:
		offset.x = Roundf ((rect.x + rect.GetRight()) *.5f);
		offset.y = Roundf (rect.GetBottom() - m_Rect.Height ());
		break;
	case kLowerRight:
		offset.x = Roundf (rect.GetRight());
		offset.y = Roundf (rect.GetBottom() - m_Rect.Height ());
		break;
	default:
		offset.x = offset.y = 0.0F;
		break;	
	}
	return offset;
}

// Draw the text mesh 
// TODO: clip
void TextMeshGenerator2::Render (const Rectf &rect, const ChannelAssigns& channels)
{
	GfxDevice &device = GetGfxDevice();

	float matWorld[16], matView[16];

	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	Matrix4x4f textMatrix;

	Vector2f offset = GetTextOffset (rect);
	textMatrix.SetTranslate (Vector3f (offset.x, offset.y, 0.0f));

	device.SetViewMatrix (textMatrix.GetPtr());

	DrawUtil::DrawMeshRaw (channels, *m_Mesh, 0);

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);

	// YES, we have actually used this TextMesh
	m_LastUsedFrame = GetTimeManager().GetRenderFrameCount();	
}

void TextMeshGenerator2::RenderRaw (const ChannelAssigns& channels)
{
	DrawUtil::DrawMeshRaw (channels, *m_Mesh, 0);
	m_LastUsedFrame = GetTimeManager().GetRenderFrameCount();	
}

// Will offset count*4 vertices horizontally - used for doing center & rightaligned
void OffsetCharacters (Vector2f offset, TextMeshGenerator2::Vertex *firstIndex, Vector2f *cursor, int count) {
	int vertexCount = count * 4; 
	while (vertexCount--) {
		firstIndex->vert.x += offset.x;
		firstIndex->vert.y += offset.y;
		firstIndex++;
	}
	while (count--) {
		cursor->x += offset.x;
		cursor->y += offset.y;
		cursor++;
	}
}

void TextMeshGenerator2::FixLineOffset (float lineWidth, TextMeshGenerator2::Vertex *firstChar, Vector2f *firstCursor, int count) {
	// If we're centre or right-aligned, handle that.
	// Right now charOffset.x contains the width of the line.
	switch (m_Alignment) {
		// if we're right-aligned, move everything backl
	case kRight:
		OffsetCharacters (Vector2f (-lineWidth, 0), firstChar, firstCursor, count);
		break;
	case kCenter:
		OffsetCharacters (Vector2f (Roundf (-lineWidth * .5f), 0), firstChar, firstCursor, count);
		break;
	default:
		break;
	}
}

class TextMeshGenerationData 
{
	TextMeshGenerator2 &tmgen;
	std::vector<TextFormatChange> format;
	TextMeshGenerator2::Vertex *vertex, *vertexBegin;
	dynamic_array<UInt16> materialTriangles[kMaxMaterials];
	Font *font;
	int formatChange;
	int characterPos;
	int lastChar;
	int startOfWord, startOfLine;
	int stringlen;
	int materialCount;
	float lineSize;
	float lineLength, wordLength;
	float endOfLastWord, startOfWordPos;
	float spacesLength;
	float maxLineLength;
	bool wordWrap;
	Vector3f charOffset;
	float startY;
	FormatStack formatStack;

	inline float Roundf (float x)
	{
		return tmgen.Roundf(x);
	}

public:
	int GetStringLength () const { return stringlen; }
	dynamic_array<UInt16> *GetTriangles () { return materialTriangles; }

	TextMeshGenerationData(TextMeshGenerator2 &_tmgen)
		:	tmgen (_tmgen),
		formatStack(tmgen.m_Color, tmgen.m_FontSize, tmgen.m_FontStyle),
		formatChange (0),
		characterPos (0),
		startOfWord (0),			// Character index at start of current word (used for wordwrap)
		startOfLine (0),			// character index at start of current line (used for right/center-aligning text).
		lineLength (0),			// Length of current line without trailing spaces (in pixels)
		wordLength (0),		// Length of current word (in pixels)
		endOfLastWord (0),		// position of the last completed word's right bounds
		startOfWordPos (0),		// Pixel position of the current word start.
		spacesLength (0),		// Size of spaces (used to not make trailing spaces affect alignment)
		maxLineLength (0),		// The max line length - used for calculating the rect at the end.
		lineSize (0),
		startY (0),
		lastChar (-1)
	{
		font = tmgen.GetFont();
		wordWrap = tmgen.m_WordWrapWidth != 0.0f;
	}

	void ParseFormatAndCacheFont ()
	{
		if (tmgen.m_RichText && IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
			GetFormatString (tmgen.m_UTF16Text, format);
		int maxFontSize = tmgen.m_FontSize ? tmgen.m_FontSize : font->GetFontSize();

		materialCount = 1;
		for (std::vector<TextFormatChange>::iterator i = format.begin(); i != format.end(); i++)
		{
			if (i->flags & kFormatSize)
			{
				float size = i->format.size ? i->format.size : font->GetFontSize();
				if (size > maxFontSize)
					maxFontSize = (int)size;
			}
			if (i->flags & (kFormatMaterial | kFormatImage))
			{
				if (i->format.material >= kMaxMaterials || i->format.material < 0)
				{
					WarningStringMsg ("Only %d materials are allowed per TextMesh.", kMaxMaterials);
					i->format.material = 0;
				}
				if (i->format.material+1 > materialCount)
					materialCount = i->format.material+1;
			}
		}

		AssertIf (font == NULL);
		font->CacheFontForText (tmgen.m_UTF16Text.text, tmgen.m_UTF16Text.length, tmgen.m_FontSize, tmgen.m_FontStyle, format);

		if (font->GetFontSize() != 0 && IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1) || font->GetConvertCase() == Font::kDynamicFont)
			startY = Roundf(font->GetAscent() * ((float)maxFontSize/font->GetFontSize() - 1.0f));

		lineSize = Roundf (font->GetLineSpacing(maxFontSize) * tmgen.m_LineSpacing);

		charOffset = Vector3f (0, startY, 0);
	}

	void SetupBuffers ()
	{
		stringlen = tmgen.m_UTF16Text.length;

		if(stringlen * 4 + 4 > std::numeric_limits<UInt16>::max())
		{
			stringlen = (std::numeric_limits<UInt16>::max() / 4) - 1;
			Assert("String too long for TextMeshGenerator. Cutting off characters.");
		}

		Mesh *mesh = tmgen.m_Mesh;
		if (!mesh)
		{
			tmgen.m_Mesh = NEW_OBJECT_MAIN_THREAD (Mesh);
			mesh = tmgen.m_Mesh;
			mesh->Reset();
			mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

			mesh->SetHideFlags (Object::kHideAndDontSave);
			mesh->SetHideFromRuntimeStats(true);
#if UNITY_EDITOR
			mesh->SetName("TextMesh");
#endif
		}
		else
			mesh->Clear (true);

		size_t vCount = stringlen * 4 + 4;
		mesh->ResizeVertices (vCount, TextMeshGenerator2::Vertex::kFormat);
		mesh->SetVertexColorsSwizzled (gGraphicsCaps.needsToSwizzleVertexColors);

		vertexBegin = (TextMeshGenerator2::Vertex*)mesh->GetVertexDataPointer ();
		vertex = vertexBegin;

		// Set up the array & pointer for cursor positions.
		// This is 1 longer than the string, as asking for a cursor position AFTER the last character is a valid request
		tmgen.m_CursorPos.resize (stringlen + 1);
	}

	void InsertSpace ()
	{
		vertex[0].vert = vertex[1].vert = vertex[2].vert = vertex[3].vert = charOffset;
		vertex += 4;
		float w = font->GetCharacterWidth (' ', formatStack.Current().size, formatStack.Current().style);
		// If this is the first space following a word, we record the last word position for correct wrapping
		if (spacesLength == 0)
			endOfLastWord = charOffset.x;

		spacesLength += w;

		// all the vars that makes this a word delimiter
		startOfWord = characterPos + 1;
		wordLength = 0;
		startOfWordPos = charOffset.x + w;
		// we never wordwrap on a space (no matter how many spaces you have, if it doesn't fit the cursor just stops at the rightmost position)

		charOffset.x += w;
	}

	void InsertLineBreak ()
	{
		// Move the line down
		vertex[0].vert = vertex[1].vert = vertex[2].vert = vertex[3].vert = charOffset;
		vertex += 4;

		// if we're right or center aligned, move all character in this line to the right place.
		tmgen.FixLineOffset (lineLength, &vertexBegin[startOfLine * 4], &tmgen.m_CursorPos[startOfLine], characterPos - startOfLine + 1);

		charOffset.x = 0;
		charOffset.y += lineSize;
		maxLineLength = std::max(maxLineLength, lineLength);
		lineLength = 0;
		spacesLength = 0;
		startOfLine = startOfWord = characterPos + 1;
	}

	void InsertTab ()
	{
		if (spacesLength == 0)
			endOfLastWord = charOffset.x;
		spacesLength = 0;

		int tab = FloorfToInt (charOffset.x / tmgen.m_TabSize) + 1;
		if (wordWrap && tab * tmgen.m_TabSize > tmgen.m_WordWrapWidth) {
			// if we're right or center aligned, move all character in this line to the right place.
			tmgen.FixLineOffset (lineLength, &vertexBegin[startOfLine * 4], &tmgen.m_CursorPos[startOfLine], characterPos - startOfLine);
			charOffset.y += lineSize;
			charOffset.x = lineLength = tmgen.m_TabSize;
			startOfLine = startOfWord = characterPos + 1;
			maxLineLength = std::max(maxLineLength, lineLength);
		} else {
			//				float newPos = tab * m_TabSize;

			if (wordWrap && tab * tmgen.m_TabSize > tmgen.m_WordWrapWidth) {
				// if we're right or center aligned, move all character in this line to the right place.
				tmgen.FixLineOffset (lineLength, &vertexBegin[startOfLine * 4], &tmgen.m_CursorPos[startOfLine], characterPos - startOfLine);
				charOffset.y -= lineSize;
				charOffset.x = lineLength = tmgen.m_TabSize;
				startOfLine = startOfWord = characterPos + 1;
				maxLineLength = std::max(maxLineLength, lineLength);
			} else {
				float newPos = tab * tmgen.m_TabSize;

				lineLength = charOffset.x = newPos;
			}	
			vertex[0].vert = vertex[1].vert = vertex[2].vert = vertex[3].vert = charOffset;
			vertex += 4;

			// all the vars that makes this a word delimiter
			startOfWord = characterPos + 1;
			wordLength = 0;
			startOfWordPos = charOffset.x;
		}
	}

	void InsertCharacter (int c)
	{
		Rectf vert, charUv;		// rendering info
		bool flipped;
		float w;				// character width

		font->GetCharacterRenderInfo( c, formatStack.Current().size, formatStack.Current().style, vert, charUv, flipped );
		w = Roundf (font->GetCharacterWidth (c, formatStack.Current().size, formatStack.Current().style));

		// TODO: can these be non-floats? If they can't shouldn't we be doing this at load rather than when getting each character?
		// Possibly during serialization as well to conserve size (assume 100 characters * 4 floats  = 1600 bytes/font)
		vert.y = Roundf (vert.y);
		vert.x = Roundf (vert.x);
		vert.width = Roundf (vert.width);
		vert.height = Roundf (vert.height);

		// Add kerning information if present.
		if( !font->GetKerningValues().empty() && lastChar != -1 )
		{
			pair<UnicodeChar, UnicodeChar> kerningPair = pair<UnicodeChar, UnicodeChar>(lastChar, c);
			Font::KerningValues::iterator foundKerning = font->GetKerningValues().find(kerningPair);

			if (foundKerning != font->GetKerningValues().end())
			{
				float kerning = foundKerning->second;
				if (tmgen.m_FontSize != 0)
					kerning = Roundf(kerning * tmgen.m_FontSize/(float)font->GetFontSize());
				charOffset.x += kerning;
			}
		}

		// Add quad vertices and UVs
		vertex[0].vert = charOffset + Vector3f(vert.x, -vert.y, 0);
		vertex[flipped?2:0].uv = Vector2f (charUv.x, charUv.GetBottom());
		vertex[1].vert = charOffset + Vector3f(vert.GetRight(), -vert.y, 0);
		vertex[1].uv = Vector2f (charUv.GetRight(), charUv.GetBottom());
		vertex[2].vert = charOffset + Vector3f(vert.GetRight(), -vert.GetBottom(), 0);
		vertex[flipped?0:2].uv = Vector2f (charUv.GetRight(), charUv.y);
		vertex[3].vert = charOffset + Vector3f(vert.x, -vert.GetBottom(), 0);
		vertex[3].uv = Vector2f (charUv.x, charUv.y);
		ColorRGBA32 deviceColor = GfxDevice::ConvertToDeviceVertexColor(formatStack.Current().color);
		vertex[0].color = vertex[1].color = vertex[2].color = vertex[3].color = deviceColor;
		vertex += 4;

		// Add two triangles to indices
		int vertexIndex = characterPos * 4;	// since we always emit vertices we know the triangle index from the character index
		int material = formatStack.Current().material;
		materialTriangles[material].push_back(vertexIndex + 1);
		materialTriangles[material].push_back(vertexIndex + 2);
		materialTriangles[material].push_back(vertexIndex);

		materialTriangles[material].push_back(vertexIndex + 2);
		materialTriangles[material].push_back(vertexIndex + 3);
		materialTriangles[material].push_back(vertexIndex);

		// word-wrapping: If we're too large with this letter, we wrap the word to the next line by moving the verts
		if (wordWrap && charOffset.x + w > tmgen.m_WordWrapWidth /* && wordLength != 0.0f*/) {

			// Special case: if the word we're wrapping is longer than one line alone, we split the word at the offending
			// character.
			if (startOfWord == startOfLine) {
				startOfWord = characterPos;
				wordLength = 0;
				startOfWordPos = charOffset.x;
				endOfLastWord = charOffset.x;
			}

			// Horizontally align everything up to the last word (the part before the inserted newline)
			tmgen.FixLineOffset (endOfLastWord, &vertexBegin[startOfLine * 4], &tmgen.m_CursorPos[startOfLine], startOfWord - startOfLine);

			// Move all letters after the inserted newline one line down and to 0 in Xoffset.
			OffsetCharacters (Vector2f (-startOfWordPos, +lineSize), &vertexBegin[startOfWord * 4], &tmgen.m_CursorPos[startOfWord], characterPos - startOfWord + 1);
			maxLineLength = std::max(maxLineLength, lineLength);

			// Move the cursor
			charOffset.x -= startOfWordPos;
			// The length of the current line is the length of the word we just wrapped down
			lineLength = wordLength;

			charOffset.y += lineSize;

			// The new line starts at this word
			startOfLine = startOfWord;
			startOfWordPos = 0;

			// If we had any accumulated spaces, they don't apply now (as we just word-wrapped)
			spacesLength = 0;
		}
		wordLength += w;
		// advance the cursor
		charOffset.x += w;
		lineLength += w + spacesLength;	// Add this letter + any accumulated spaces to the line length
		spacesLength = 0;				// We have now used the accumulated spaces
		lastChar = c;
	}

	void ProcessFormatForPosition ()
	{
		while (formatChange < format.size() && characterPos >= format[formatChange].startPosition)
		{
			characterPos += format[formatChange].skipCharacters;

			// Make sure we don't overflow if the string is capped.
			if (characterPos > stringlen)
			{
				characterPos = stringlen;
				return;
			}

			for (int j=0; j<format[formatChange].skipCharacters; j++)
			{
				vertex[0].vert = vertex[1].vert = vertex[2].vert = vertex[3].vert = charOffset;
				vertex += 4;
			}

			formatStack.PushFormat(format[formatChange]);
			if ((format[formatChange].flags & (kFormatImage | kFormatPop)) == kFormatImage)
			{
				vertex -= 4;
				float size = formatStack.Current().size;
				if (font->GetFontSize() != 0 && IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1) || font->GetConvertCase() == Font::kDynamicFont)
				{
					if (size == 0)
						size = font->GetFontSize();
					size = size / font->GetFontSize() * font->GetAscent();
				}
				else
					size = font->GetAscent();
				Vector3f imageBase = charOffset + Vector3f(0, font->GetAscent(), 0);	
				Rectf& r = format[formatChange].format.imageRect;
				float aspect;
				if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
					aspect = r.width / r.height;
				else
					aspect = 1.0f;
				vertex[0].vert = imageBase + Vector3f(0, -size, 0);
				vertex[0].uv = Vector2f (r.x, r.GetYMax());
				vertex[1].vert = imageBase + Vector3f(size * aspect, -size, 0);
				vertex[1].uv = Vector2f (r.GetXMax(), r.GetYMax());
				vertex[2].vert = imageBase + Vector3f(size * aspect, 0, 0);
				vertex[2].uv = Vector2f (r.GetXMax(), r.y);
				vertex[3].vert = imageBase + Vector3f(0, 0, 0);
				vertex[3].uv = Vector2f (r.x, r.y);
				ColorRGBA32 deviceColor = GfxDevice::ConvertToDeviceVertexColor(formatStack.Current().color);
				vertex[0].color = vertex[1].color = vertex[2].color = vertex[3].color = deviceColor;
				charOffset += Vector3f(Roundf(size * aspect), 0, 0);
				
				if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
					lineLength = charOffset.x;

				vertex += 4;

				int vertexIndex = (characterPos-1) * 4;	// since we always emit vertices we know the triangle index from the character index
				int material = formatStack.Current().material;
				materialTriangles[material].push_back(vertexIndex + 1);
				materialTriangles[material].push_back(vertexIndex + 2);
				materialTriangles[material].push_back(vertexIndex);

				materialTriangles[material].push_back(vertexIndex + 2);
				materialTriangles[material].push_back(vertexIndex + 3);
				materialTriangles[material].push_back(vertexIndex);
			}
			formatChange++;
		}
	}

	void ProcessString ()
	{
		for (characterPos=0; characterPos <= stringlen; characterPos++)
		{	
			ProcessFormatForPosition ();

			// all the code becomes a lot simpler if we pretend we have a newline at the end. 
			// If not, the last line becomes a special-case requiring lots of code duplication.
			int c;
			if (characterPos < stringlen)
				c = tmgen.m_UTF16Text[characterPos];
			else
				c = '\n';

#if TEXTDEBUG
			printf_console ("%c", c);
#endif

			// store the character offset into the cursor position array
			tmgen.m_CursorPos[characterPos] = Vector2f (charOffset.x, charOffset.y - startY);

			switch (c) {
			case '\n': 
				InsertLineBreak();			
				break;
			case ' ': 
				InsertSpace();
				break;
			case '\t':
				InsertTab();
				break;
			default: 
				InsertCharacter(c);
				break;
			}
		}
#if TEXTDEBUG
		printf_console ("\n");
#endif
	}

	Rectf GetBounds ()
	{
		// Get the bounding volume.
		// Y coordinates are 0 - charOffset.y (which has been moved down one line  due to the fake \n we inserted.
		Rectf r;
		r.y = 0;
		r.SetBottom(charOffset.y-startY);
		switch (tmgen.m_Alignment) {
		case kLeft:
			r.x = 0;
			r.width = Roundf (maxLineLength);
			break;
		case kRight:
			r.x = -Roundf (maxLineLength);
			r.width = Abs(r.x);
			break;
		case kCenter:
			r.x = -Roundf (maxLineLength * .5f);
			r.width = Roundf (maxLineLength);
			break;
		default:
			break;
		}
		return r;
	}

	void SetMeshData ()
	{
		Mesh* mesh = tmgen.m_Mesh;
		mesh->SetSubMeshCount(materialCount);
		for (int i=0; i<materialCount; i++)
		{
			if (materialTriangles[i].size())
				mesh->SetIndicesComplex(&materialTriangles[i][0], materialTriangles[i].size(), i, kPrimitiveTriangles, Mesh::k16BitIndices);
		}

		AABB bounds;
		bounds.SetCenterAndExtent(Vector3f(tmgen.m_Rect.x, tmgen.m_Rect.y, 0.0f), Vector3f::zero);
		bounds.Encapsulate (Vector3f(tmgen.m_Rect.GetXMax(), tmgen.m_Rect.GetYMax(), 0.0f));
		mesh->SetLocalAABB (bounds);

		mesh->SetChannelsDirty (mesh->GetAvailableChannels (), false );
	}
};

void TextMeshGenerator2::Generate ()
{
	TextMeshGenerationData data (*this);
	data.ParseFormatAndCacheFont ();
	data.SetupBuffers ();

	// Allocate temp memory for the first material (which contains all triangles by default),
	// to avoid unnecessary allocations.
	UInt16 *triangles = NULL;
	int bufferSize = data.GetStringLength() * 6;
	ALLOC_TEMP(triangles, UInt16, bufferSize);
	data.GetTriangles()[0].assign_external (triangles, triangles + bufferSize);
	data.GetTriangles()[0].resize_uninitialized(0);

	data.ProcessString ();

	m_Rect = data.GetBounds ();
	data.SetMeshData ();
}

Font *TextMeshGenerator2::GetFont () const {
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

#if UNITY_EDITOR
void TextMeshGenerator2::CleanCache (const UTF16String &text)
{
	for (Generators::iterator i = s_Generators.begin(); i != s_Generators.end();)
	{
		TextMeshGenerator2 *gen = *i;
		if (gen->m_UTF16Text ==  text)
		{
			delete gen;
			i = s_Generators.erase(i);
		}
		else
			i++;
	}
}
#endif
