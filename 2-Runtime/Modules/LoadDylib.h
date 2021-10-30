#ifndef LOADDYLIB_H
#define LOADDYLIB_H
#include <string>

std::string GetPathWithPlatformSpecificDllExtension(const std::string& path);
void* LoadDynamicLibrary (const std::string& absolutePath);
void* LoadAndLookupSymbol (const std::string& absolutePath, const std::string& name);
void UnloadDynamicLibrary (void* libraryReference);
void UnloadDynamicLibrary (const std::string& absolutePath);
bool LoadAndLookupSymbols (const char* path, ...);
void* LookupSymbol(void* libraryReference, const std::string& symbolName);

#endif
