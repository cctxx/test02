#pragma once

#include <string>

// Finds token in text, properly skipping C/C++ comments. Token must be preceded by non-alpha numeric
// character as well (or come at start of text)
size_t FindTokenInText( const std::string& text, const std::string& token, size_t startPos = 0 );

// Counts number of lines in text up to and including endPos
size_t CountLines( const std::string& text, size_t endPos );

// Skips whitespace. Returned position is at the end or point to non-whitespace already.
size_t SkipWhiteSpace( const std::string& text, size_t startPos );

// Skips to start of next line
size_t SkipToNextLine (const std::string& text, size_t startPos);

// Reads non-whitespace characters starting from startPos
std::string ReadNonWhiteSpace( const std::string& text, size_t& startPos );

// Extracts from startPos up to (but not including) newline or end of text
std::string ExtractRestOfLine( const std::string& text, size_t startPos );

bool GetValueAfterPrefix (const std::string& token, const std::string& prefix, std::string& outValue);


static inline bool IsNewline( char c ) { return c == '\r' || c == '\n'; }
