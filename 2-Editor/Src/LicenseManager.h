#undef Assert
#undef DebugAssert
#ifdef verify
#undef verify
#endif

#define XERCES_STATIC_LIBRARY 1

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLException.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/Base64.hpp>
#include <xercesc/util/XMLDateTime.hpp>
#include <xercesc/framework/MemoryManager.hpp>
#include <xsec/framework/XSECProvider.hpp>
#include <xsec/utils/XSECPlatformUtils.hpp>
#include <xsec/framework/XSECProvider.hpp>
#include <xsec/enc/OpenSSL/OpenSSLCryptoX509.hpp>
#include <xsec/enc/XSECCryptoException.hpp>
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Configuration/UnityConfigureVersion.h"

#if UNITY_OSX
#include <IOKit/IOKitKeys.h>
#endif

#include <sstream>
#include <iostream>
#include <iomanip>

#define _SHA1_DIGEST_LENGTH 20

using namespace xercesc;

// Convert PACE license to Unity license
#define LICENSE_CMD_CONVERT "1"
// Update Unity license
#define LICENSE_CMD_UPDATE "2"
// Return Unity license
#define LICENSE_CMD_RETURN "3"
// New Unity license
#define LICENSE_CMD_NEW "9"


class XercesMemoryManager : public xercesc::MemoryManager
{
	void* allocate (XMLSize_t size)
	{
		return UNITY_MALLOC(kMemDefault, size);
	}
	
	void deallocate (void* p)
	{
		UNITY_FREE(kMemDefault, p);
	}
	
	MemoryManager* getExceptionMemoryManager ()
	{
		return xercesc::XMLPlatformUtils::fgMemoryManager;
	}
};

class LicenseManager
{
private:
	string m_UnityLicenseDirPath;
	string m_PaceLicenseFilePath;
	string m_UnityLicenseFilePath;
	string m_BackupUnityLicenseFile;
	
	unsigned char m_MachineID[_SHA1_DIGEST_LENGTH];
	
	string m_MachineBinding1;
	string m_MachineBinding2;
	string m_MachineBinding4;
	
	UInt64 m_Tokens;
	
	string m_Data;
	string m_Command;
	
	int m_LicenseStatus;

	bool m_MustUpdate;
	
	int m_State;
	string m_txID;
	string m_rxID;
	
	char m_SerialNumber[32]; 
	int	m_ConcurrentUsers;
	bool m_HasExpirationDate;
	int m_UpdateCount;
	
	int m_HttpResponseCode;

	string m_BaseActivationURL;
	string m_BaseLicenseURL;
	
	void StartNewLicenseFlow ();
	int StartPaceConversionFlow ();
	bool PaceLicenseExists ();
	void TransmitLicenseFile (string cmd);
	int ProcessLicense (xercesc_3_1::DOMDocument **, unsigned char **);
	bool ReadLicenseFile (unsigned char ** licenseData);
	bool ReadLicenseFile (unsigned char ** licenseData, string path);
	bool ValidateLicenseDocument (xercesc_3_1::DOMDocument * doc);
	int ValidateMachineBindings (xercesc_3_1::DOMDocument* doc);
	bool ValidateLicenseType (xercesc_3_1::DOMDocument * doc);
	bool ReadTokens (xercesc_3_1::DOMDocument * doc);
	bool ReadDeveloperData (xercesc_3_1::DOMDocument * doc);
	
	char* FlipAndCodeBytes (const char *, int, int, char *);
	
	inline string DoHash (std::string input);
	
	void CreateNewCommandXML();
	string CreateLicenseXMLSegment();
	
	void HandleServerErrorAndExit ();

	string GetDate (const char*, xercesc_3_1::DOMDocument*);
	
	bool AppendPACEFile(std::string& data);
	void AppendSystemInfoXML(std::string& xmlString);
	bool RecoverBackupLicense(bool setLicenseState = true);

	bool InitializeLocalData ();
	void SendLicenseServerCommand ();
	
public:
	
	string GetMachineBindingQueryParameters ();
	
	LicenseManager ();
	~LicenseManager ();
	
	int Initialize ();
	
	bool ValidateLicenseData (string licenseData);
    
    int ValidateDates (xercesc_3_1::DOMDocument* doc);
	
	UInt64 GetTokens ();
	
	char* SerialNumber ();
	
	string GetLicenseFilePath ();
	void QueryLicenseUpdate();

	void CallActivationServer ();
	void CallActivationServer (string TX_RX_IDS);

	void NewActivation ();
	void ManualActivation ();

	void ReturnLicense ();
	int WriteLicenseFile (string, bool isBase64Encoded);
	int DeleteLicenseFile ();

	void DownloadLicense (string queryParams);
	
	bool IsLicenseUpdated();
	
	string GetMachineID();
	
	string GetAuthToken ();
	
	void LastCurlHttpResponseCode(int HttpResponseCode);
	
	const std::string GetLicenseURL();

	bool UnknownStatus();
	
	friend class LicenseInfo;

	friend class LicenseRequestMessage;
	friend class ActivationRequestMessage;
};

/*
	States
*/
enum
{
	kIdle                                   = 0x40000000L,
	kOngoingActivationServerCommunication   = 0x40000001L,
	kOngoingLicenseServerCommunication      = 0x40000004L,
	kContactActivationServer                = 0x40000010L,
	kContactLicenseServer                   = 0x40000020L
};


/*
 Status codes
 */
enum
{
	kLicenseStatus_Valid      = 0x00000000L,
	kLicenseStatus_New        = 0x00000001L,    // Issue new license.
	kLicenseStatus_Issued     = 0x00000004L,    // New license issued.
	kLicenseStatus_Convert    = 0x00000010L,    // Convert PACE license.
	kLicenseStatus_Converted  = 0x00000018L,    // PACE license converted to Unity license.
	kLicenseStatus_Update     = 0x00000020L,    // License must be updated.
	kLicenseStatus_Updated    = 0x00000028L,    // License is updated.
	kLicenseStatus_Return     = 0x00000030L,    
	kLicenseStatus_Returned   = 0x00000038L,    
	kLicenseStatus_Invalid    = 0x00000040L,
	kLicenseStatus_Unknown    = 0x00000050L
};

/*
	Error codes
*/
enum
{
	kLicenseErrorFlag_NotSpecified			= 0x80000000L,
	kLicenseErrorFlag_NoLicense				= 0x80000001L,
	kLicenseErrorFlag_PaceLicenseExpired	= 0x80000004L,
	kLicenseErrorFlag_FileCompromised		= 0x80000010L,
	kLicenseErrorFlag_MachineBinding1		= 0x80000020L,
	kLicenseErrorFlag_MachineBinding2		= 0x80000040L,
	kLicenseErrorFlag_MachineBinding4		= 0x80000080L,
	kLicenseErrorFlag_Tokens				= 0x80000100L,
	kLicenseErrorFlag_DeveloperData			= 0x80000110L,
	kLicenseErrorFlag_LicenseNotReadyForUse = 0x80000120L,
	kLicenseErrorFlag_LicenseExpired        = 0x80000140L,
	kLicenseErrorFlag_Initialization		= 0x80000180L,
	kLicenseErrorFlag_VersionMismatch		= 0x80000200L,
};
