#ifndef NATIVEMESHFORMATCONVERTER_H
#define NATIVEMESHFORMATCONVERTER_H

class FBXImporter;
// TODO : these all should be moved int MeshFormatConverter
std::string ConvertMayaToFBX (FBXImporter& importer, const std::string& mbFile, std::string* file, int attemptNumber);
std::string ConvertMaxToFBX (FBXImporter& importer, const std::string& maxFile, std::string* file, int attemptNumber, bool useFileUnits);
std::string ConvertC4DToFBX (FBXImporter& importer, const std::string& sourceFile, std::string* file);

#endif
