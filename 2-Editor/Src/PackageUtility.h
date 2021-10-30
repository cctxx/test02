#ifndef PACKAGE_UTILITY_H
#define PACKAGE_UTILITY_H


#include "Runtime/Utilities/GUID.h"

struct ImportPackageAsset
{
	std::string exportedAssetPath;
	UnityGUID guid;
	UnityGUID parentGuid;
	std::string message;
	bool		enabled;
	std::string      guidFolder;
	bool		isFolder;
	std::string previewPath;
	bool		exists;

	ImportPackageAsset ()
	{
		enabled = true;
	}
	
	friend bool operator < (const ImportPackageAsset& lhs, const ImportPackageAsset& rhs)
	{
		return StrICmp (lhs.message, rhs.message) < 0;
	}
};

// This info is stored in the gzip header of packages downloaded from the Asset Store
// For other packages, this is empty.
class PackageInfo
{
public:
	std::string packagePath;
	
	// Unparsed JSON string containing info about the package
	// Parsing of the data is handled by the Asset Store application running in Webkit
	std::string jsonInfo;

	// A PNG icon contained in the package header
	std::vector<UInt8> pngIcon;

	PackageInfo(const std::string& packagePath);
	~PackageInfo();
	
	std::string GetIconDataURL();
};

enum { kDisallowMovedFiles = 1 << 0 };

bool ImportPackageStep1 (const std::string& packagePath, std::vector<ImportPackageAsset>& assets, std::string& outPackageIconPath);
bool ImportPackageStep2 (std::vector<ImportPackageAsset>& assets);
void ExportPackage (const std::set<UnityGUID>& guids, const std::string& packagePathName, bool forceBatchMode = false);
void BuildExportPackageAssetList (const std::set<UnityGUID>* guids, std::vector<ImportPackageAsset>* assets, bool dependencies, bool includeUnmodified=true, bool includeNameConflicts=false);
bool ImportPackageNoGUI (const std::string& packagePath);

bool ImportPackageGUI (const std::string& packagePath);

void DelayedImportPackageStep2 (std::vector<ImportPackageAsset>& assets);
void DelayedExportPackage (const std::set<UnityGUID>& guids, const std::string& packagePathName);

void TickPackageImport ();

void GetPackageLocations (std::vector<std::string>& paths, bool includeStandard, bool include3rdPlace, bool includeTargetSpecific);
void GetPackageList (std::vector<PackageInfo>& packages, bool includeStandard = true, bool include3rdPlace = true, bool includeTargetSpecific = true);

//Defined in ASMonoUtility.cpp
void ShowImportPackageGUI (const std::vector<ImportPackageAsset> &src, const std::string &packageIconPath);


#endif
