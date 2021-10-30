#ifndef COPYPASTE_H
#define COPYPASTE_H

/// Get the system-wide copy buffer for pasting into a textfield
std::string GetCopyBuffer ();

/// Set the system-wide copybuffer for pasting from a textfield
void SetCopyBuffer (const std::string &utf8string);

#endif
