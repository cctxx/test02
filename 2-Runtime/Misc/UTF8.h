#pragma once

#include "Runtime/Utilities/dynamic_array.h"

typedef UInt16 UnicodeChar;

bool ConvertUTF8toUTF16 (const char* source, int srcLength, UnicodeChar* output, int& outlength);
bool ConvertUTF8toUTF16 (const std::string& source, dynamic_array<UnicodeChar>& utf16);
bool ConvertUTF16toUTF8 (const UInt16* source, int srcLength, char* output, int& outlength);
bool ConvertUTF16toUTF8 (const dynamic_array<UnicodeChar>& source, std::string& utf8);
bool ConvertUTF16toUTF8 (const UInt16* source, int srcLength, char* output, int& outlength);
bool ConvertUTF16toUTF8 (const UInt16 utf16character, std::string& utf8);
