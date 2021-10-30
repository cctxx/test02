#ifndef TEXTUTIL_H
#define TEXTUTIL_H

namespace Unity { class Material; }
class Transform;
class Camera;
class Vector3f;
class Matrix4x4f;
class Object;
class Texture;
class Shader;
class Vector2f;
class Font;

//#include <string>
#include <set>
#include <vector>
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"

using namespace Unity;

enum TextAlignment {
	kLeft,
	kCenter,
	kRight,
	kAuto,
};

enum TextAnchor {
	kUpperLeft,
	kUpperCenter,
	kUpperRight,
	kMiddleLeft,
	kMiddleCenter,
	kMiddleRight,
	kLowerLeft,
	kLowerCenter,
	kLowerRight,
	kDontCare		///< Special case for getting text mesh generators: The anchoring used for the text doesn't modify the size of the generated text, so if you just want to query for it you don't care about anchoring
};

enum TextClipping {
	/// Text flows freely outside the element.
	kOverflow = 0,
	/// Text gets clipped to be inside the element.
	kClip = 1,
};

struct MonoString;

// Opaque UTF16 string representation
struct UTF16String
{
#if ENABLE_SCRIPTING
	explicit UTF16String (ScriptingStringPtr sourceSting);
#endif
	UTF16String () {text = NULL; length = 0; owns = false;}
	explicit UTF16String (const char* str);
	UTF16String (const UTF16String &other);
	~UTF16String ();

	UInt16 operator [] (int index) const         { return text[index]; }
	friend bool operator == (const UTF16String& lhs, const UTF16String& rhs)
	{
		using namespace std;
		if (lhs.length != rhs.length)
			return false;
		return (rhs.text == NULL || memcmp(lhs.text, rhs.text,lhs.length * sizeof (UInt16)) == 0);
	}

	UInt16* text;
	int length;
	bool owns;

	ScriptingStringPtr GetScriptingString ();
	void CopyString (const UTF16String &other);
#if ENABLE_MONO || UNITY_WINRT
	void BorrowString (ScriptingStringPtr sourceString);
#endif
	void TakeOverPreAllocatedUTF16Bytes(UInt16* bytes, int size);
private:
	void InitFromCharPointer(const char* str);	
};

void UTF8ToUTF16String (const char* src, UTF16String& dst);

Vector2f GetCursorPosition (const Rectf &screenRect, const UTF16String &text, TextAnchor align, bool wordWrap, int textPosition);

// Set a custom font to use.
// If null, it will use the default resource font
void SetTextFont (Font *font);
Font *GetTextFont ();

// Calculate size of a piece of text
Vector2f CalcTextSize (const UTF16String &text);

/// Calculate height of text word-wrapped to width
float CalcTextHeight (const UTF16String &, float width);

/// Set a custom shader to use for drawing text.
/// If null, the material set in the font will be used
void SetDrawTextMaterial (Material *m);
Material *GetDrawTextMaterial ();




#endif
