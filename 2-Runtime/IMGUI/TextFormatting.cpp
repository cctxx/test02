#include "UnityPrefix.h"
#include "TextFormatting.h"

enum FormattingTag {
	kTagBold,
	kTagItalic,
	kTagColor,
	kTagSize,
	kTagMaterial,
	kTagImage,
	kTagX,
	kTagY,
	kTagWidth,
	kTagHeight,
	kNumTags,
};

const char* kFormattingTagStrings[] = 
{
	"b",
	"i",
	"color",
	"size",
	"material",
	"quad",
	"x",
	"y",
	"width",
	"height",
};


const char* kFormattingHTMLColorStrings[] = 
{
	"red",
	"cyan",
	"blue",
	"darkblue",
	"lightblue",
	"purple",
	"yellow",
	"lime",
	"fuchsia",
	"white",
	"silver",
	"grey",
	"black",
	"orange",
	"brown",
	"maroon",
	"green",
	"olive",
	"navy",
	"teal",
	"aqua",
	"magenta",
};

#define kNumHTMLColors (sizeof(kFormattingHTMLColorStrings)/sizeof(kFormattingHTMLColorStrings[0]))

const UInt8 kFormattingHTMLColorValues[kNumHTMLColors*4] = 
{
	0xff,0x00,0x00,0xff,
	0x00,0xff,0xff,0xff,
	0x00,0x00,0xff,0xff,
	0x00,0x00,0xa0,0xff,
	0xad,0xd8,0xe6,0xff,
	0x80,0x00,0x80,0xff,
	0xff,0xff,0x00,0xff,
	0x00,0xff,0x00,0xff,
	0xff,0x00,0xff,0xff,
	0xff,0xff,0xff,0xff,
	0xc0,0xc0,0xc0,0xff,
	0x80,0x80,0x80,0xff,
	0x00,0x00,0x00,0xff,
	0xff,0xa5,0x00,0xff,
	0xa5,0x2a,0x2a,0xff,
	0x80,0x00,0x00,0xff,
	0x00,0x80,0x00,0xff,
	0x80,0x80,0x00,0xff,
	0x00,0x00,0x80,0xff,
	0x00,0x80,0x80,0xff,
	0x00,0xff,0xff,0xff,
	0xff,0x00,0xff,0xff,
};

FormattingTag GetTag(UTF16String& input, int &pos, bool &closing)
{
	if (input[pos] == '<')
	{
		int outpos = pos + 1;
		if (outpos == input.length)
			return (FormattingTag)-1;
			
		closing = input[outpos] == '/';
		if (closing)
			outpos++;
			
		for (int i=0;i<kNumTags;i++)
		{
			bool match = true;
			for (int j=0;kFormattingTagStrings[i][j] != '\0'; j++)
			{
				if (outpos+j == input.length || ToLower((char)input[outpos+j]) != kFormattingTagStrings[i][j])
				{
					match = false;
					break;
				}
			}
			if (match)
			{
				int paramPos = outpos + strlen(kFormattingTagStrings[i]);
				if ((!closing && input[paramPos] == '=') || (input[paramPos] == ' ' && i == kTagImage))
				{
					while (input[paramPos] != '>' && paramPos < input.length)
						paramPos++;						
				}
				else if (input[paramPos] != '>')
					continue;
				outpos += strlen(kFormattingTagStrings[i]);
				pos = outpos;
				return (FormattingTag)i;
			}
		}
	}
	return (FormattingTag)-1;
}

FormattingTag GetImageTag(UTF16String& input, int &pos)
{
	for (int i=0;i<kNumTags;i++)
	{
		bool match = true;
		for (int j=0;kFormattingTagStrings[i][j] != '\0'; j++)
		{
			if (pos+j == input.length || ToLower((char)input[pos+j]) != kFormattingTagStrings[i][j])
			{
				match = false;
				break;
			}
		}
		if (match)
		{
			int paramPos = pos + strlen(kFormattingTagStrings[i]);
			if (input[paramPos] == '=')
			{
				pos = paramPos;
				return (FormattingTag)i;
			}
			else
				continue;
		}
	}
	return (FormattingTag)-1;
}

std::string GetParameter(UTF16String& input, int &pos, bool allowMultipleParamaters = false)
{
	std::string parameter;
	if (input[pos] == '=')
	{
		pos++;
		while (input[pos] != '>' && (input[pos] != ' ' || !allowMultipleParamaters) && pos < input.length)
			parameter.push_back(input[pos++]);
	}
	if (parameter.size() > 2 && parameter[0] == parameter[parameter.size()-1])
	{
		if (parameter[0] == '\'' || parameter[0] == '"')
			parameter = parameter.substr(1,parameter.size()-2);
	}
	return parameter;
}

ColorRGBA32 ParseHTMLColor(std::string colorstring)
{
	ColorRGBA32 color = ColorRGBA32(0xffffffff);
	if (colorstring[0] == '#')
	{
		if (colorstring.size() == 4 || colorstring.size() == 5)
		{
			std::string longcolorstring = "#";
			for (int i=1;i<colorstring.size();i++)
			{
				longcolorstring += colorstring[i];
				longcolorstring += colorstring[i];
			}
			colorstring = longcolorstring;
		}
		if (colorstring.size() == 7 || colorstring.size() == 9)
			HexStringToBytes (&colorstring[1], colorstring.size()/2, &color);
	}
	else for (int i=0; i<kNumHTMLColors; i++)
	{
		if (StrICmp(colorstring, kFormattingHTMLColorStrings[i]) == 0)
			return ColorRGBA32 (*(UInt32*)(kFormattingHTMLColorValues+i*4));
	}
	return color;
}

bool ValidateFormat (std::vector<TextFormatChange> &format)
{
	std::vector<int> stack;
	for (std::vector<TextFormatChange>::iterator i = format.begin(); i != format.end(); i++)
	{
		int flags = i->flags;
		if (flags & kFormatPop)
		{
			if (stack.empty())
				// closing tag without opening tag.
				return false;
			flags &= ~kFormatPop;
			if (stack.back() != flags)
				// closing does not match opening tag.
				return false;
			stack.pop_back();
		}
		else
			stack.push_back(flags);
	}
	if (!stack.empty())
		// unclosed tags.
		return false;
	return true;
}

void ParseImageParameters (UTF16String& input, int &pos, TextFormatChange &change)
{
	while (pos < input.length && input[pos] != '>')
	{
		FormattingTag tag = GetImageTag(input, pos);
		if (tag != -1)
		{
			switch (tag)
			{
				case kTagMaterial:
					change.flags |= kFormatMaterial;
					change.format.material = StringToInt(GetParameter(input, pos, true));
					break;
				case kTagSize:
					change.flags |= kFormatSize;
					change.format.size = StringToInt(GetParameter(input, pos, true));
					break;
				case kTagColor:
					change.flags |= kFormatColor;
					change.format.color = ParseHTMLColor(GetParameter(input, pos, true));
					break;
				case kTagX:
					 sscanf(GetParameter(input, pos, true).c_str(), "%f", &change.format.imageRect.x);
					break;
				case kTagY:
					sscanf(GetParameter(input, pos, true).c_str(), "%f", &change.format.imageRect.y);
					break;
				case kTagWidth:
					sscanf(GetParameter(input, pos, true).c_str(), "%f", &change.format.imageRect.width);
					break;
				case kTagHeight:
					sscanf(GetParameter(input, pos, true).c_str(), "%f", &change.format.imageRect.height);
					break;
				default:
					break;
			}
		}
		else 
			pos++;
	}
}

void GetFormatString (UTF16String& input, std::vector<TextFormatChange> &format)
{
	int pos = 0;
	while (pos < input.length)
	{
		int oldpos = pos;
		bool closing;
		FormattingTag tag = GetTag(input, pos, closing);
		if (tag != -1)
		{
			TextFormatChange change;
			change.flags = kFormatPop;
			switch (tag)
			{
				case kTagBold:
					change.flags = kFormatBold;
					break;
				case kTagItalic:
					change.flags = kFormatItalic;
					break;
				case kTagSize:
					change.flags = kFormatSize;
					break;
				case kTagColor:
					change.flags = kFormatColor;
					break;
				case kTagMaterial:
					change.flags = kFormatMaterial;
					break;
				case kTagImage:
					change.flags = kFormatImage;
					break;
				default:
					break;
			}
			if (closing)
				change.flags |= kFormatPop;
			else switch (tag)
			{
				case kTagSize:
					change.format.size = StringToInt(GetParameter(input, pos));
					break;
				case kTagColor:
					change.format.color = ParseHTMLColor(GetParameter(input, pos));
					break;
				case kTagMaterial:
					change.format.material = StringToInt(GetParameter(input, pos));
					break;
				case kTagImage:
					ParseImageParameters(input, pos, change);
					break;
				default:
					break;
			}
			change.skipCharacters = pos + 1 - oldpos;
			change.startPosition = oldpos;
			format.push_back(change);
			if (tag == kTagImage)
			{
				change.flags |= kFormatPop;
				change.skipCharacters = 0;
				format.push_back(change);
			}
		}
		pos ++;	
	}
	if (!ValidateFormat(format))
	// Fail silently here rather then spamming the console with errors while typing unfinished markup.
		format.clear();
}