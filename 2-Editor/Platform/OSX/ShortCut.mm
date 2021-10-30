/*
 Copyright (c) 2003, OTE Entertainments.  All rights reserved.
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Snoize nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ShortCut.h"
#include "Runtime/Utilities/Word.h"

typedef struct  {
	const char *keyMnemonic;
	unichar code;
	unsigned int modifier;
	unichar tooltipCharacter;
} KeyMnemonic; 

static KeyMnemonic s_keyNames[] = {
/*	{"LEFT", NSLeftArrowFunctionKey, 0x2190, NSFunctionKeyMask | NSNumericPadKeyMask}, 
	{"RIGHT", NSRightArrowFunctionKey, 0x2192, NSFunctionKeyMask | NSNumericPadKeyMask}, 
	{"UP", NSUpArrowFunctionKey, 0x2191, NSFunctionKeyMask | NSNumericPadKeyMask}, 
	{"DOWN", NSDownArrowFunctionKey,0x2193, NSFunctionKeyMask | NSNumericPadKeyMask}, 
*/	{"LEFT", NSLeftArrowFunctionKey, NSFunctionKeyMask | NSNumericPadKeyMask, 0x2190}, 
	{"RIGHT", NSRightArrowFunctionKey, NSFunctionKeyMask | NSNumericPadKeyMask, 0x2192}, 
	{"UP", NSUpArrowFunctionKey, NSFunctionKeyMask | NSNumericPadKeyMask, 0x2191}, 
	{"DOWN", NSDownArrowFunctionKey, NSFunctionKeyMask | NSNumericPadKeyMask, 0x2193}, 
	{"F1", NSF1FunctionKey,NSFunctionKeyMask,0 }, 
	{"F2", NSF2FunctionKey,NSFunctionKeyMask,0 }, 
	{"F3", NSF3FunctionKey,NSFunctionKeyMask,0 }, 
	{"F4", NSF4FunctionKey,NSFunctionKeyMask,0 }, 
	{"F5", NSF5FunctionKey,NSFunctionKeyMask,0 }, 
	{"F6", NSF6FunctionKey,NSFunctionKeyMask,0 }, 
	{"F7", NSF7FunctionKey,NSFunctionKeyMask,0 }, 
	{"F8", NSF8FunctionKey,NSFunctionKeyMask,0 }, 
	{"F9", NSF9FunctionKey,NSFunctionKeyMask,0 }, 
	{"F10", NSF10FunctionKey,NSFunctionKeyMask,0 }, 
	{"F11", NSF11FunctionKey,NSFunctionKeyMask,0 }, 
	{"F12", NSF12FunctionKey,NSFunctionKeyMask,0 }, 
	{"HOME", NSHomeFunctionKey,NSFunctionKeyMask,0 }, 
	{"END", NSEndFunctionKey,NSFunctionKeyMask,0 }, 
	{"PGUP", NSPageUpFunctionKey,NSFunctionKeyMask,0 }, 
	{"PGDN", NSPageDownFunctionKey,NSFunctionKeyMask,0 }, 
	{"KP0", '0', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP1", '1', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP2", '2', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP3", '3', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP4", '4', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP5", '5', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP6", '6', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP7", '7', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP8", '8', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP9", '9', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP.", '.', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP.", '.', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP+", '+', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP-", '-', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP*", '*', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP/", '/', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{"KP=", '=', NSNumericPadKeyMask | NSFunctionKeyMask,0 },
	{0, 0,0,0}
};

// Find start of hotkey string
static int FindStartIndex (const char *src) {
	int len = 0;
	bool wasSpace = false;
	while (*src && len < 127) {
		char c = *src;
		if( wasSpace && (c == '&' || c == '%' || c == '#' || c == '_') )
			break;
		wasSpace = IsSpace(c);
		src++;
		len++;
	}
	return len;
}

static int FindSplitIndex (const char *src) {
	int len = 0;
	while (*src && len < 127) {
		if (*src == '|')
			return len;
		src++;
		len++;
	}
	return 0;
}

@implementation ShortCut
-(void)release {
	if (m_MainString)
		[m_MainString release];
	if (m_KeyEquivalent)
		[m_KeyEquivalent release];
	if (m_LongString)
		[m_LongString release];
}
-(void)setKeyString:(NSString *)str {
	const char *cstr = [str UTF8String];
	const char *def;
	int reallen = strlen(cstr);
	int len = FindStartIndex (cstr);
	int len2 = FindSplitIndex (cstr);
	int len3 = len;
	while (len3 && cstr[len3] == ' ' || cstr[len3] == '\t') {
		len3--;
	}
	unichar key = 0;
	
	[m_MainString release];
	[m_LongString release];
	
	if (len2) {
		m_MainString = [[NSString alloc]initWithBytes: cstr length: len2 encoding: NSUTF8StringEncoding];
		m_LongString = [[NSString alloc]initWithBytes: cstr + len2 + 1 length: reallen-len2-1 encoding: NSUTF8StringEncoding];
	} else {
		m_MainString = [[NSString alloc]initWithBytes: cstr length: len encoding: NSUTF8StringEncoding];
		m_LongString = [[NSString alloc]initWithBytes: cstr length: reallen encoding: NSUTF8StringEncoding];
	}
	m_ModifierMask = 0;
	[m_KeyEquivalent release];
	def = &cstr[len];
	while (*def) {
		switch (*def) {
		case '&': // Alt
			m_ModifierMask |= NSAlternateKeyMask;
			break;
		case '%':
			m_ModifierMask |= NSCommandKeyMask;
			break;
		case '#':
			m_ModifierMask |= NSShiftKeyMask;
			break;
		case '_':
			break;
		default:
			if (*def == ToLower (*def)) 
				key = *def;
			else {
				KeyMnemonic *k = s_keyNames;
				while (k->keyMnemonic) {
					if (!strcmp (def, k->keyMnemonic)) {
						key = k->code;
						m_ModifierMask |= k->modifier;
						break;
					}
					k++;
				}
			}
		}
		if (key)
			break;
		def++;
	}
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
	;
#else
	if (!m_ModifierMask) {
		m_ModifierMask |= NSCommandKeyMask;
	}
#endif
	if (key)
		m_KeyEquivalent = [[NSString stringWithCharacters:&key length:1] retain];
	else
		m_KeyEquivalent = @"";
}
	
-(NSString *) tooltipString {
	
	if (m_MainString)
		return [NSString stringWithFormat:@"%@ (%@)", m_MainString, [self keyString]];
	else 
		return [self keyString];
}

-(NSString *) keyString {
	NSMutableString *str = [[[NSMutableString alloc] init] autorelease];
	static unichar s_CommandKey = 0x2318;
	static unichar s_ShiftKey = 0x21E7;
	static unichar s_AltKey = 0x2387;
	static unichar s_ControlKey = 0x2303;
	unichar key;
	if (m_ModifierMask & NSControlKeyMask)
		[str appendString: [NSString stringWithCharacters: &s_ControlKey length:1]];
	if (m_ModifierMask & NSAlternateKeyMask)
		[str appendString: [NSString stringWithCharacters: &s_AltKey length:1]];
	if (m_ModifierMask & NSShiftKeyMask)
		[str appendString: [NSString stringWithCharacters: &s_ShiftKey length:1]];
	if (m_ModifierMask & NSCommandKeyMask)
		[str appendString: [NSString stringWithCharacters: &s_CommandKey length:1]];

	if (![m_KeyEquivalent length])
		return @"";
	key = [m_KeyEquivalent characterAtIndex: 0];

	if (key < 0xF700)
		[str appendString: [m_KeyEquivalent uppercaseString]];
	else {
		KeyMnemonic *k = s_keyNames;
		while (k->keyMnemonic) {
			if (k->code == key) {
				if (k->tooltipCharacter) 
					[str appendString: [NSString stringWithCharacters: &k->tooltipCharacter length:1]];
				else
					[str appendString: [NSString stringWithUTF8String: k->keyMnemonic]];
				break;
			}
			k++;
		}
	}
	return str;
}

-(unsigned int) modifierKeyMask {
	return m_ModifierMask;
}

-(NSString *) mainString {
	return m_MainString;
}
-(NSString *) longString {
	return m_LongString;
}
-(NSString *) longStringNoKeys {
	const char *cstr = [m_LongString UTF8String];
	int len = FindStartIndex (cstr);
	NSString* result = [[[NSString alloc]initWithBytes: cstr length:len encoding: NSUTF8StringEncoding] autorelease];
	if (result == NULL)
		result = [[[NSString alloc]initWithCString: cstr length:len] autorelease]; 
	return result;
}

-(NSString *) keyEquivalent {
	return m_KeyEquivalent;
}

-(BOOL) accepts:(NSEvent *)theEvent {
	UInt32 mod = [theEvent modifierFlags];
	NSString *s;
	if ([theEvent type] != NSKeyDown && [theEvent type] != NSKeyUp)
		return NO;
	mod &= NSNumericPadKeyMask | NSFunctionKeyMask | NSShiftKeyMask |
			NSCommandKeyMask | NSControlKeyMask | NSAlternateKeyMask;
	if ((UInt32)mod != (UInt32)m_ModifierMask)
		return NO;
	s = [[theEvent charactersIgnoringModifiers]lowercaseString];
	return [s characterAtIndex: 0] == [m_KeyEquivalent characterAtIndex: 0];
}

@end
