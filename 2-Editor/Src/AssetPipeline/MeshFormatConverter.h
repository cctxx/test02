#ifndef MESHFORMATCONVERTER_H
#define MESHFORMATCONVERTER_H

// This will convert Modo to Collada, but users can change that to 
// convert it to FBX (or any other format that can be read by FBX SDK)
std::string ConvertModoToFBXSDKReadableFormat(const std::string& sourceFile, std::string* file);

#endif
