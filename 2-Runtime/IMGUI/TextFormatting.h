#ifndef TEXTFORMATTING_H
#define TEXTFORMATTING_H

#include "TextUtil.h"
#include "Runtime/Math/Rect.h"

enum {
	kStyleDefault = 0,
	kStyleFlagBold = 1 << 0,
	kStyleFlagItalic = 1 << 1,
};

struct TextFormat
{
	int style;
	ColorRGBA32 color;
	int size;
	int material;
	Rectf imageRect;
	
	TextFormat () :
	style(0),
	color(0xff,0xff,0xff,0xff),
	size(0),
	material(0),
	imageRect(0,0,1,1)
	{
	}
};

enum FormatFlags {
	kFormatBold = 1 << 0,
	kFormatItalic = 1 << 1,
	kFormatColor = 1 << 2,
	kFormatSize = 1 << 3,
	kFormatMaterial = 1 << 4,
	kFormatImage = 1 << 5,
	kFormatPop = 1 << 15,
};

struct TextFormatChange
{
	int startPosition;
	int skipCharacters;
	TextFormat format;
	int flags;
};

class FormatStack : std::vector<TextFormat>
{
public:
	FormatStack (ColorRGBA32 _color, int _size, int _style)
	{
		push_back (TextFormat());
		back().color = _color;
		back().size = _size;
		back().style = _style;
	}
	
	void PushFormat (TextFormatChange &change)
	{
		if (change.flags & kFormatPop)
			pop_back();
		else
		{
			push_back(back());
			if (change.flags & kFormatBold)
				back().style |= kStyleFlagBold;
			if (change.flags & kFormatItalic)
				back().style |= kStyleFlagItalic;
			if (change.flags & kFormatColor)
				back().color = change.format.color;
			if (change.flags & kFormatSize)
				back().size = change.format.size;
			if (change.flags & kFormatMaterial)
				back().material = change.format.material;
		}			
	}
	
	TextFormat& Current() { return back(); }
};

void GetFormatString (UTF16String& input, std::vector<TextFormatChange> &format);

#endif