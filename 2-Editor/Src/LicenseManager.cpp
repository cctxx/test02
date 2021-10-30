#include "UnityPrefix.h"

#include "Runtime/Threads/Thread.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/Argv.h"

#include "Editor/Src/Utility/Base64.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/LicenseManager.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLBigInteger.hpp>
#include <openssl/evp.h>

#if UNITY_WIN
#ifndef __MSXML_LIBRARY_DEFINED__
#define __MSXML_LIBRARY_DEFINED__
#endif
#include <WinIoCtl.h>
#include <ShlObj.h>
#include <WinSock.h>
#include <Lmcons.h>
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "PlatformDependent/Win/Registry.h"
#pragma warning(disable:4200)
#endif

#if UNITY_LINUX
#include <arpa/inet.h> // ntohl and friends
#endif

#if UNITY_WIN
#undef CreateDirectory
#undef DeleteFile
bool CreateDirectory (const string& pathName)
{
	if (IsFileCreated (pathName))
		return false;
	else if (IsDirectoryCreated (pathName))
		return true;

	string absolutePath = PathToAbsolutePath (pathName);

#if UNITY_EDITOR
	ConvertSeparatorsToUnity(absolutePath);

	std::string fileNamePart = GetLastPathNameComponent(absolutePath);
	if( CheckValidFileNameDetail(fileNamePart) == kFileNameInvalid) {
		ErrorStringMsg ("%s is not a valid directory name. Please make sure there are no unallowed characters in the name.", fileNamePart.c_str());
		return false;
	}
#endif

	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathName.c_str(), widePath, kDefaultPathBufferSize );
	if( CreateDirectoryW( widePath, NULL ) )
		return true;
	else
	{
		DWORD err = GetLastError();
		printf_console("CreateDirectory '%s' failed: %s\n", absolutePath.c_str(), winutils::ErrorCodeToMsg(err).c_str() );
		return false;
	}
}

bool RemoveReadOnlyW(LPCWSTR path);

bool DeleteFile (const string& path)
{
	string absolutePath = PathToAbsolutePath (path);
	if (IsFileCreated (absolutePath))
	{
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName( absolutePath.c_str(), widePath, kDefaultPathBufferSize );

		if (!DeleteFileW( widePath ))
		{
			#if UNITY_EDITOR
			if (ERROR_ACCESS_DENIED == GetLastError())
			{
				if (RemoveReadOnlyW(widePath))
				{
					return (FALSE != DeleteFileW(widePath));
				}
			}
			#endif

			return false;
		}

		return true;
	}
	else
		return false;
}
#endif

const char * cert = NULL;

// X509 public key for the signed license file.
// The Unity license server has a private key for this certificate and signs it before sending it to Unity.
const char cert_unity[] =
"MIIE7zCCA9egAwIBAgIRAJZpZsDepakv5CaXJit+yx4wDQYJKoZIhvcNAQEFBQAwVDELMAkGA1UE"
"BhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExKjAoBgNVBAMTIUdsb2JhbFNpZ24gUGVy"
"c29uYWxTaWduIDIgQ0EgLSBHMjAeFw0xMjA4MDcxMzA3MzdaFw0xMzA4MDgxMzA3MzdaMIGHMQsw"
"CQYDVQQGEwJESzETMBEGA1UECBMKQ29wZW5oYWdlbjETMBEGA1UEBxMKQ29wZW5oYWdlbjEfMB0G"
"A1UEChMWVW5pdHkgVGVjaG5vbG9naWVzIEFwczELMAkGA1UEAxMCSVQxIDAeBgkqhkiG9w0BCQEW"
"EWFkbWluQHVuaXR5M2QuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA18DUjObN"
"s/rPbTGfVbQkt0FVOEhBb3FF90ZzkLs5dApGvtuKTUmo94Xoha5rkXBWFTnRGRXSANuqAljfHFiJ"
"3QtXB4l9SzrwGrvswWHh3hZh/AIhbwBanDWT02NEd92hOPOoCkMzHxGcJWT+dKYgSTgTmDx28tsG"
"urvgkdETqO8Ueo/Y0hIXRTQMtJ0wih6U6WQ1RxY+qTo6ImrAz/CjhtpfkyZ+yj8iZbW5uCJ8/bjO"
"MmTpO/awDcrkooFxd16/hMuCuhkq4Iejuk8/9i48DyqtA7Q3utQJ2FA97NONuWdOz7lms/MeHHNG"
"Izhhe+vyAbZuSrtr8gQF3Rw5Edu1bwIDAQABo4IBhjCCAYIwDgYDVR0PAQH/BAQDAgWgMEwGA1Ud"
"IARFMEMwQQYJKwYBBAGgMgEoMDQwMgYIKwYBBQUHAgEWJmh0dHBzOi8vd3d3Lmdsb2JhbHNpZ24u"
"Y29tL3JlcG9zaXRvcnkvMBwGA1UdEQQVMBOBEWFkbWluQHVuaXR5M2QuY29tMAkGA1UdEwQCMAAw"
"HQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMEMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9j"
"cmwuZ2xvYmFsc2lnbi5jb20vZ3MvZ3NwZXJzb25hbHNpZ24yZzIuY3JsMFUGCCsGAQUFBwEBBEkw"
"RzBFBggrBgEFBQcwAoY5aHR0cDovL3NlY3VyZS5nbG9iYWxzaWduLmNvbS9jYWNlcnQvZ3NwZXJz"
"b25hbHNpZ24yZzIuY3J0MB0GA1UdDgQWBBS77TdJmqER3qBFAT+yU4zBB8a1xTAfBgNVHSMEGDAW"
"gBQ/FdJtfC/nMZ5DCgaolGwsO8XuZTANBgkqhkiG9w0BAQUFAAOCAQEAC//kW1Pu07FhYo0Wmju5"
"ZCDaolsieAQpnDeJC1P76dfQxnGGibscK1xRk3lhiFx6cDoxnOyCiKt/CdWWtAMO6bnO4UmFEtO3"
"UljKg4VmayvGMhW5dup3M7FRn/CDM6UJl3dHJ3PmclbDZEQ0ctiXxBwIEPFy1Y1X9b3SwznX3pWJ"
"/UsQ270DtKuVz3kUqSpZEhBo8Gb1m+FoGsnGQb+8vnfEGgD9/bxURhTUeteQ1N+CGyfTCd0QVqKx"
"zPO43SpWwQ50SDtQT0bEZeA+UOdqSH04W4XCkcmx+1zZ8GtHihaefyxDceZOKKPq4Gi+02JbwWuX"
"JXFP96+m73xQXG96dg==";

const char cert_pace[] =
"MIIFaTCCBFGgAwIBAgIDD0gjMA0GCSqGSIb3DQEBBQUAMIHGMQswCQYDVQQGEwJVUzETMBEGA1UE"
"CBMKQ2FsaWZvcm5pYTERMA8GA1UEBxMIU2FuIEpvc2UxGTAXBgNVBAoTEFBBQ0UgQW50aS1QaXJh"
"Y3kxKDAmBgNVBAsTH0NlcnRpZmljYXRpb24gU2VydmljZXMgRGl2aXNpb24xJDAiBgNVBAMTG1BB"
"Q0UgQW50aS1QaXJhY3kgTGljZW5zZSBDQTEkMCIGCSqGSIb3DQEJARYVY2VydG1hc3RlckBwYWNl"
"YXAuY29tMB4XDTExMDQxNzIzNTk1OVoXDTEyMDUwMTIzNTk1OVowgZ0xHzAdBgNVBAoTFlVuaXR5"
"IFRlY2hub2xvZ2llcyBBcFMxIDAeBgkqhkiG9w0BCQEWEWFkbWluQHVuaXR5M2QuY29tMQswCQYD"
"VQQIEwJESzETMBEGA1UEBxMKQ29wZW5oYWdlbjELMAkGA1UEBhMCREsxDjAMBgNVBAMTBVVuaXR5"
"MRkwFwYDVQQLExBMaWNlbnNlIFNlcnZpY2VzMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC"
"AQEAufINUqkxvbHyaWs7aJtiJVesTvffokmp1TiWbrMtMIGdw37G0HGOxWReBoWidp1SD3+mBNjI"
"436SvJeupRO2CRLzFTQcQZajmRjxx9pJFWgB3jf78MbekorNfxzD197n0CLybEufp9YW9pahL5Z8"
"RkR5Z6Fn1UbPFk60qSfFwi2Pl2DkFJ9zvBF5N0mYNCI8D89+raM3Ov287LvHoeP/1Rhk5VYyghm1"
"8vOuEyNSHLhwWoyE0WAmv30/2VNan9OsVfbRrQabuB9HdMSjN70fG85jQsg3OnCfupZbORXFOGvC"
"5vZApGxEFdREDXaGPTxNJXxDmDD3Ex2uaBIqk4wMrwIDAQABo4IBhTCCAYEwgfMGA1UdIwSB6zCB"
"6IAUaaDJ9hm9tu3Jx32tplIstyUNB8ehgcykgckwgcYxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpD"
"YWxpZm9ybmlhMREwDwYDVQQHEwhTYW4gSm9zZTEZMBcGA1UEChMQUEFDRSBBbnRpLVBpcmFjeTEo"
"MCYGA1UECxMfQ2VydGlmaWNhdGlvbiBTZXJ2aWNlcyBEaXZpc2lvbjEkMCIGA1UEAxMbUEFDRSBB"
"bnRpLVBpcmFjeSBMaWNlbnNlIENBMSQwIgYJKoZIhvcNAQkBFhVjZXJ0bWFzdGVyQHBhY2VhcC5j"
"b22CAQAwHQYDVR0OBBYEFPvfTQX7xC7EijhPu/JC0t4FElFYMAwGA1UdEwEB/wQCMAAwDgYDVR0P"
"AQH/BAQDAgSwMDMGCSsGAQQB2mQAAQQmFiQ2MzNCNzU5RS0zM0RGLTNGRDItQjdFRS0wNjVEMDRG"
"NDY1MjMwFwYJKwYBBAHaZAEBBAoWCElnQUFBQT09MA0GCSqGSIb3DQEBBQUAA4IBAQCPMSIcfDwT"
"GlmjXvtk32CvbDnFZbv0exSL82Xn/N6y8kyN+/Q4LaZ31nZLQufDz10BTd9Kv4pm0swe6w6y3lKP"
"AQx8mIlQeymE8aZKGsFp2vOwB9byN43wuXRJhKUy2n0Bj+cOWT6vhAkW13G46qVjEcAK7GJQ9xll"
"AIFXPnz/d4ELW8fdbF+WsG9+X/Eb7FEGxPKU6AX1m253DBY0olQpOXrn86C/6IyJEEE3DnMzrLZ7"
"Xqi9g5Ix9x9sqPo9jlU03mbuYJgop+y4QCWOXRZ5o0cHCPZCSwsa7kTvGdQr2Hz4NLhV8xbTDSoZ"
"IIg+2T0Fo2ua+N2X9WRjHEYl/auM";


const char kActivationURL[] = "https://activation.unity3d.com/license.fcgi";
const char kLicenseURL[] = "https://license.unity3d.com/update";

const int kUpdateRequestLimit = 10;

void LicenseLog(const char* format, ...);

#if UNITY_OSX
bool CreateLicenseDirectory(const char* path);
#endif

namespace {

	const int kBufferSize = 512;

	void PrintMessage(std::string &uri, std::vector<std::string> &headers)
	{
		LicenseLog("Received %s", uri.c_str());
		std::string allHeaders;
		for(std::vector<std::string>::iterator i = headers.begin(); i != headers.end(); ++i )
			allHeaders.append("    " + *i + "\n");
		LicenseLog("Headers:\n%s", allHeaders.c_str());
	}


	#if UNITY_OSX
	void VerifyUnityLicenseFolderExists(const string& path)
	{
		if (!IsPathCreated(path))
		{
			DisplayDialog("Information", "The license file directory does not exist and needs to be created in a process with elevated privileges. A seperate process will be spawned to do this and you will be prompted for permissions. To avoid this run the installer which automatically creates this directory", "OK");
			if (!CreateLicenseDirectory(path.c_str()))
			{
				DisplayDialog("Error", "Unity license path in /Library/Application Support does not exist and we failed to create it, use the installer to ensure the folder exists", "Quit");
				exit(1);
			}

		}
	}
	#endif

	void ReplaceChar(string& str, char c, string const& replacement)
	{
		for (size_t pos = str.find(c); pos != string::npos; pos = str.find(c, pos + 1))
			str.replace(pos, 1, replacement);
	}

	string XmlEscape(string const& value)
	{
		string escaped = value;

		ReplaceChar(escaped, '&', "&amp;");
		ReplaceChar(escaped, '<', "&lt;");
		ReplaceChar(escaped, '>', "&gt;");
		ReplaceChar(escaped, '\'', "&apos;");
		ReplaceChar(escaped, '\"', "&quot;");

		return escaped;
	}

	time_t GetGMT()
	{
		time_t rawtime;
		time (&rawtime);
		struct tm *ptm = gmtime (&rawtime);
		return mktime(ptm);
	}

	int RecordTime()
	{
		int lsValue = GetGMT() ^ 0xF857CBAF;
		EditorPrefs::SetInt("LSValue", lsValue);
		return lsValue ;
	}

	bool VerifyTime()
	{
		int lsValue = EditorPrefs::GetInt("LSValue");
		if (lsValue == 0)
		{
			RecordTime();
			return true;
		}
		int storedTime = (lsValue ^ 0xF857CBAF);
		int currentTime = GetGMT();

		// Allow one hour discrepancy but don't record it
		if (currentTime > (storedTime - 3600))
			return true;

		if (currentTime < storedTime)
			return false;

		RecordTime();

		return true;
	}

	xercesc_3_1::DOMDocument* GetDOM (const unsigned char * XML)
	{
		xercesc::MemBufInputSource xmlMemBuf( XML, strlen((const char*)XML), "Unity License Data" );
		XercesDOMParser parser;
		parser.setDoNamespaces(true);
		ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
		parser.setErrorHandler(errHandler);
		try
		{
			parser.parse( xmlMemBuf );
		}
		catch (const xercesc::XMLException &e)
		{
			return NULL;
		}
		catch (...) {
			return NULL;
		}

		return parser.adoptDocument();
	}

	bool ValidateXML(std::string &data)
	{
		xercesc_3_1::DOMDocument* doc = GetDOM((const unsigned char*)data.c_str());
		if (doc == NULL)
			return false;
		doc->release();
		return true;
	}

	std::string Encode(const unsigned char* input, int inputSize)
	{
		unsigned char ebuf[kBufferSize+24];
		int ebuflen;
		std::string result;

		EVP_ENCODE_CTX ectx;
		EVP_EncodeInit(&ectx);
		EVP_EncodeUpdate(&ectx, ebuf, &ebuflen, input, inputSize);
		result.append((char*)ebuf, ebuflen);
		EVP_EncodeFinal(&ectx, ebuf, &ebuflen);
		result.append((char*)ebuf, ebuflen);

		// Remove the trailing newline the encoder adds
		size_t pos = result.find("\n");
		if (pos != string::npos)
			result.erase(pos);

		return result;
	}

	int Decode(const unsigned char* input, int inputSize, unsigned char* output)
	{
		int decodedChunk;
		int totalSize = 0;

		EVP_ENCODE_CTX ectx;
		EVP_DecodeInit(&ectx);
		EVP_DecodeUpdate(&ectx, output, &decodedChunk, input, inputSize);
		totalSize += decodedChunk;
		EVP_DecodeFinal(&ectx, output+decodedChunk, &decodedChunk);
		totalSize += decodedChunk;
		return totalSize;
	}

	int DoCipher(const unsigned char* message, int messageSize, unsigned char* output, const char* initVector, int operation)
	{
		int chunkSize;
		int totalSize = 0;
		unsigned char iv[EVP_MAX_IV_LENGTH], key[EVP_MAX_KEY_LENGTH];

		EVP_CIPHER_CTX ectx;
		EVP_CIPHER_CTX_init(&ectx);
		EVP_CipherInit_ex(&ectx, EVP_bf_cfb(), NULL, NULL, NULL, operation);

		memset(iv, 0, sizeof(iv));
		memset(key, 0, sizeof(key));
		int keyLength = EVP_CIPHER_CTX_key_length(&ectx);
		int ivLength = EVP_CIPHER_CTX_iv_length(&ectx);
		memcpy(iv, initVector, ivLength);
		memcpy(key, cert_unity, keyLength);

		EVP_CipherInit_ex(&ectx, EVP_bf_cfb(), NULL, key, iv, operation);
		EVP_CipherUpdate(&ectx, output, &chunkSize, message, messageSize);
		totalSize += chunkSize;

		EVP_CipherFinal_ex(&ectx, output+chunkSize, &chunkSize);
		totalSize += chunkSize;

		EVP_CIPHER_CTX_cleanup(&ectx);

		return totalSize;
	}

	std::string Encrypt(int value, const char* initVector)
	{
		unsigned char encrypted[kBufferSize];
		memset(encrypted, 0, kBufferSize);
		std::string valueString = Format("%d", value);
		int encryptedSize = DoCipher((const unsigned char*)valueString.c_str(), valueString.length(), encrypted, initVector, 1);
		return Encode(encrypted, encryptedSize);
	}

	int Decrypt(const std::string& msg, const char* initVector)
	{
		unsigned char decoded[kBufferSize];
		unsigned char decrypted[kBufferSize];
		memset(decoded, 0, kBufferSize);
		memset(decrypted, 0, kBufferSize);
		int decodedSize = Decode((const unsigned char*)msg.c_str(), msg.length(), decoded);
		DoCipher(decoded, decodedSize, decrypted, initVector, 0);
		std::istringstream input((char*)decrypted);
		int result;
		if (!(input >> result))
			return 0;
		return result;
	}

	void AppendXMLTime(int time, std::string &data, const char* hwid)
	{
		std::string timestampedData = data;
		std::string rootName = "<root>";
		size_t rootElement = timestampedData.find(rootName);
		if (rootElement != string::npos)
		{
			std::string timestampString = "<TimeStamp";
			size_t timestamp = timestampedData.find(timestampString);
			while (timestamp != string::npos)
			{
				size_t start = timestampedData.rfind("\n", timestamp);
				size_t end = timestampedData.find("\n", timestamp);
				if (start != string::npos && end != string::npos)
				{
					int count = end-start;
					timestampedData.erase(start, count);
					timestamp = timestampedData.find(timestampString);
				}
				else
					break;
			}
			std::string stringValue = Format("\n  <TimeStamp Value=\"");
			stringValue.append(Encrypt(time, hwid));
			stringValue.append("\"/>");
			timestampedData.insert(rootElement + rootName.length(), stringValue);
		}
		if (ValidateXML(timestampedData))
			data = timestampedData;
	}

	const XMLCh* GetXMLValueFromTag (const char* tag, const xercesc_3_1::DOMDocument* doc)
	{
		if (doc == NULL) return 0;
		XMLCh* tagName = XMLString::transcode(tag);
		DOMNodeList* elemList = doc->getElementsByTagName(tagName);
		if (elemList == NULL) return 0;
		XMLString::release(&tagName);

		DOMNode* currentNode = elemList->item(0);
		if (currentNode == NULL) return 0;

		DOMNamedNodeMap * attributes = currentNode->getAttributes();
		if (attributes == NULL) return 0;
		XMLCh* attribute = XMLString::transcode("Value");
		DOMNode * attrValue = attributes->getNamedItem(attribute);
		XMLString::release(&attribute);
		if (attrValue == NULL) return 0;

		return attrValue->getNodeValue();
	}

	// Check if the client which wrote the xml file is higher than specified version
	bool CheckXMLClientVersion(const xercesc_3_1::DOMDocument* doc, const char* version)
	{
		const XMLCh* xmlVersion = GetXMLValueFromTag("ClientProvidedVersion", doc);
		std::string xmlVersionString = XMLString::transcode(xmlVersion);
		if (GetNumericVersion(xmlVersionString) > GetNumericVersion(version))
			return true;
		else
			return false;
	}


	int GetXMLTime (const xercesc_3_1::DOMDocument* doc, const char* hwid)
	{
		const XMLCh* timeChar = GetXMLValueFromTag("TimeStamp", doc);
		if (timeChar == NULL)
			return 0;
		std::string timeString = XMLString::transcode(timeChar);
		int result = Decrypt(timeString, hwid);
		// Allow old style timestamp if license was activated by older version
		if (result == 0 && !CheckXMLClientVersion(doc, "4.0.0f5"))
		{
			XMLBigInteger *integer = NULL;
			try
			{
				integer = new XMLBigInteger(timeChar);
			}
			catch (const XMLException &exception)
			{
				return 0;
			}
			result = integer->intValue() ^ 0xF857CBAF;
		}
		return result;
	}


	bool VerifyXMLTime(xercesc_3_1::DOMDocument* doc, const char* hwid)
	{
		int storedTime = GetXMLTime(doc, hwid);

		// It's ok for older client version to not have the timestamp since the license file written by them didn't have it
		if (storedTime == 0 && !CheckXMLClientVersion(doc, "4.0.0f3"))
			return true;
		if (storedTime == 0)
		{
			LicenseLog("Time validation failed (1)\n");
 			return false;
		}

		int currentTime = GetGMT();

		// Allow one hour discrepancy
		if (currentTime > (storedTime - 3600))
			return true;

		if (currentTime < storedTime)
		{
			LicenseLog("Time validation failed (2)\n");
 			return false;
		}

		return true;
	}
}


class LicenseRequestMessage : public CurlRequestMessage
{
private:
	LicenseManager* m_LicenseManager;

public:
	virtual void Done ()
	{
		// Copy http status code for later use
		m_LicenseManager->LastCurlHttpResponseCode(m_ResponseCode);
		PrintMessage(m_Uri, m_ResponseHeaders);
		if (m_Success && m_ResponseCode == 200)
		{
			if ( m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0 ||
				 m_LicenseManager->m_Command.compare(LICENSE_CMD_UPDATE) == 0 ||
				m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0 ||
				 m_LicenseManager->m_Command.compare(LICENSE_CMD_RETURN) == 0 )
			{
				// Flag we can use later to determine if we need to show UI from License Server
				m_LicenseManager->m_State = kContactLicenseServer;

				std::vector<std::string>::iterator i = m_ResponseHeaders.begin();
				for( ; i != m_ResponseHeaders.end(); ++i )
				{
					if( i->compare(0, 5, "X-RX:" ) == 0 )
					{
						// Flag that we got a "rx_id" http header, so we don't need to call the License Server.
						// We can call the ActivationServer directly using the returned rx_id value.
						m_LicenseManager->m_State = kContactActivationServer;
						m_LicenseManager->m_rxID = i->substr(6,i->length()-6);
						break;
					}
				}

				if (m_LicenseManager->m_Command.compare(LICENSE_CMD_UPDATE) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Update;
				}
				else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_New;
				}
				else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Convert;
				}
				else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_RETURN) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Return;
				}
			}
			else
			{
				m_LicenseManager->m_State = kIdle;
			}
		}
		else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0)
		{
			const char* reason = NULL;
			for(std::vector<std::string>::iterator i = m_ResponseHeaders.begin(); i != m_ResponseHeaders.end(); ++i )
			{
				std::string reasonHeader = "Reason: ";
				size_t pos = i->find(reasonHeader);
				if (pos != string::npos)
					reason = &(i->c_str()[reasonHeader.size()]);
			}
			LicenseLog("License conversion failed: %s\n", reason);
			// If server gives a reason for the error it was probably a server side conversion error, so lets activate
			// with a new flow instead, otherwise maybe the server was unreachable or there was some breakdown and we should handle the error
			if (reason)
				m_LicenseManager->StartNewLicenseFlow();
			else
				m_LicenseManager->HandleServerErrorAndExit();
		}
		else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0 && m_LicenseManager->RecoverBackupLicense())
		{
			LicenseLog("Failed to re-activate, server error.\n");
			DisplayDialog("Failed to re-activate.", "There was a problem communicating with the license server, please try again later or contact support@unity3d.com", "OK");
		}
		else
		{
			LicenseLog("Error fetching %s\n%s\n", m_Uri.c_str(), m_Result.c_str());

			if ( m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0 ||
				m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0 )
			{
				m_LicenseManager->HandleServerErrorAndExit();
			}
			m_LicenseManager->m_State = kIdle;
		}
		m_Done = true;
	}

	virtual void Progress (size_t, size_t) {}

	virtual void Connecting () {}

	bool m_Done;

	LicenseRequestMessage (string postData, string queryParams, LicenseManager* licenseManager);
};


LicenseRequestMessage::LicenseRequestMessage (string postData, string queryParams, LicenseManager* licenseManager)
{
	m_LicenseManager = licenseManager;
	m_Done = false;
	m_Uri.append(m_LicenseManager->m_BaseLicenseURL);
	m_Uri.append("/poll");
	m_Uri.append("?cmd=");
	m_Uri.append(queryParams);
	m_Uri.append("&tx_id=");
	UnityGUID guid;
	guid.Init();
	m_Uri.append(GUIDToString(guid));

	m_LicenseManager->m_txID = GUIDToString(guid);
	m_LicenseManager->m_State = kOngoingLicenseServerCommunication;

	std::vector<std::string> cmd;
	std::stringstream f(queryParams.c_str());
	std::string s;
	while (std::getline(f, s, '&'))
	{
		cmd.push_back(s);
	}

	m_Method = "POST";
	m_PostData = postData;

	m_FailOnError = 0;
	m_ConnectTimeout = 20;

	m_Headers.push_back("Content-Type: text/xml");
	// Clear Expect header, lighttpd doesn't like it
	m_Headers.push_back("Expect:");
	m_Headers.push_back(Format("Length: %d", (int)postData.length()));

	LicenseLog("Opening %s\n", m_Uri.c_str());
	LicenseLog("Posting %s\n", postData.c_str());
}


class ActivationRequestMessage : public CurlRequestMessage
{
private:
	LicenseManager* m_LicenseManager;

public:
	virtual void Done ()
	{
		m_LicenseManager->m_State = kIdle;
		// Copy http status code for later use
		m_LicenseManager->LastCurlHttpResponseCode(m_ResponseCode);
		PrintMessage(m_Uri, m_ResponseHeaders);
		if( m_Success && m_ResponseCode == 200 )
		{
			if ( m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0 ||
				m_LicenseManager->m_Command.compare(LICENSE_CMD_UPDATE) == 0 ||
				m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0 )
			{
				m_LicenseManager->WriteLicenseFile( m_Result, false );

				if (m_LicenseManager->m_Command.compare(LICENSE_CMD_UPDATE) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Updated;
				}
				else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_NEW) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Issued;
				}
				else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0)
				{
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Converted;
				}

			}
			else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_RETURN) == 0)
			{
				m_LicenseManager->m_LicenseStatus = kLicenseStatus_Returned;
			}
		}
		else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_CONVERT) == 0)
		{
			LicenseLog("License conversion failed: %s\n", m_ResponseString.c_str());
			m_LicenseManager->StartNewLicenseFlow();
		}
		// Failure to update a license should not be a hard error since a valid license is
		// required in place before an update can be triggered.
		else if (m_LicenseManager->m_Command.compare(LICENSE_CMD_UPDATE) == 0)
		{
			LicenseLog("Failed to update license: %s\n", m_ResponseString.c_str());
			// Set to valid status again or an error in the activation server response will make the update query go into a loop
			// Successful license server response sets it to update
			m_LicenseManager->m_LicenseStatus = kLicenseStatus_Valid;
		}
		else
		{
			LicenseLog("Activation failure: %s\n\t%s\n", m_ResponseString.c_str(), m_Result.c_str());
			DisplayDialog ("Error", "A valid Unity license is not available.\nPlease contact support@unity3d.com", "Quit");

			ExitDontLaunchBugReporter ();
		}
		m_Done = true;
	}

	virtual void Progress (size_t, size_t) {}

	virtual void Connecting () {}

	bool m_Done;

	ActivationRequestMessage (string postData, string queryParams, LicenseManager* licenseManager);
};


ActivationRequestMessage::ActivationRequestMessage (string postData, string queryParams, LicenseManager* licenseManager)
{
	m_LicenseManager = licenseManager;
	m_Done = false;
	m_Uri.append(m_LicenseManager->m_BaseActivationURL);
	m_Uri.append("?CMD=");
	m_Uri.append(queryParams);

	m_LicenseManager->m_State = kOngoingActivationServerCommunication;
	m_FailOnError = 0; // FALSE to don't fail silently if the HTTP code returned is greater than or equal to 400.
	m_ConnectTimeout = 20;

	std::vector<std::string> cmd;
	std::stringstream f(queryParams.c_str());
	std::string s;
	while (std::getline(f, s, '&'))
	{
		cmd.push_back(s);
	}

	m_Method = "POST";
	m_PostData = postData;

	m_Headers.push_back("Content-Type: text/xml");
	// Clear Expect header, lighttpd doesn't like it
	m_Headers.push_back("Expect:");
	m_Headers.push_back(Format("Length: %d", (int)postData.length()));

	LicenseLog("Opening %s\n", m_Uri.c_str());
	LicenseLog("Posting %s\n", postData.c_str());
}


LicenseManager::LicenseManager ()
{
	m_Tokens = 0;
	m_HasExpirationDate = false;
	m_UpdateCount = 0;
	m_Command = LICENSE_CMD_UPDATE;
	memset(m_SerialNumber, 0, sizeof(m_SerialNumber));

	m_txID.clear();
	m_rxID.clear();

	if (HasARGV("activationserver"))
	{
		m_BaseActivationURL = "https://";
		m_BaseActivationURL.append(GetFirstValueForARGV("activationserver"));
		m_BaseActivationURL.append("/license.fcgi");
	}
	else
		m_BaseActivationURL = kActivationURL;

	if (HasARGV("licenseserver"))
	{
		m_BaseLicenseURL = "https://";
		m_BaseLicenseURL.append(GetFirstValueForARGV("licenseserver"));
		m_BaseLicenseURL.append("/update");
	}
	else
		m_BaseLicenseURL = kLicenseURL;
}


LicenseManager::~LicenseManager ()
{
	// TO DO: Avoid this until we have identified why it crash.
	XSECPlatformUtils::Terminate();
	XMLPlatformUtils::Terminate();
}


bool LicenseManager::InitializeLocalData ()
{
#if UNITY_OSX
	string path = GetApplicationSupportPath();
#elif UNITY_WIN
	std::string path;
	wchar_t _path[kDefaultPathBufferSize];
	if( SHGetSpecialFolderPathW( NULL, _path, CSIDL_COMMON_APPDATA, false ) )
	{
		ConvertWindowsPathName( _path, path );
	}
	else
	{
		LicenseLog("Failed to find license file path\n");
		return false;
	}
#elif UNITY_LINUX
	string path = GetUserAppDataFolder ();
#else
#error Unknown platform!
#endif
	m_PaceLicenseFilePath = m_UnityLicenseDirPath = m_UnityLicenseFilePath = path;
	m_PaceLicenseFilePath.append("/PACE Anti-Piracy/License Files/Unity_v3.x.ilf");

	m_UnityLicenseFilePath = m_UnityLicenseDirPath.append("/Unity");
	#if UNITY_OSX
	VerifyUnityLicenseFolderExists(m_UnityLicenseFilePath);
	#endif
	m_UnityLicenseFilePath.append("/Unity_v");
	m_UnityLicenseFilePath.append(Format("%d", UNITY_VERSION_VER));
	m_UnityLicenseFilePath.append(".x.ulf");
	m_BackupUnityLicenseFile = m_UnityLicenseFilePath;
	m_BackupUnityLicenseFile.append(".backup");

	try
	{
		XMLPlatformUtils::Initialize();
		XSECPlatformUtils::Initialise();
	}
	catch( const XMLException &e )
	{
		LicenseLog("Failed to initialize XML utilities\n");
		return false;
	}

	// Collect information used for "node locking"
#if UNITY_OSX
	io_registry_entry_t ioServiceRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
	// Get the machine's UUID as a string (kIOPlatformUUIDKey)
	CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioServiceRoot, CFSTR("IOPlatformUUID"), kCFAllocatorDefault, 0);
	m_MachineBinding1 = XmlEscape(CFStringToString(uuidCf));
	// Get the machine's SerialNumber as a string (kIOPlatformSerialNumberKey)
	CFStringRef serialNumberCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioServiceRoot, CFSTR("IOPlatformSerialNumber"), kCFAllocatorDefault, 0);
	m_MachineBinding2 = XmlEscape(CFStringToString(serialNumberCf));
	CFRelease(uuidCf);
	CFRelease(serialNumberCf);
	IOObjectRelease(ioServiceRoot);
#elif UNITY_WIN
	HKEY hkResult = NULL;
	REGSAM samDesired = KEY_READ | KEY_WOW64_64KEY;
	LONG lRetOpen = ::RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", NULL, samDesired, &hkResult);

	WCHAR szBufferW[512];
	DWORD dwBufferWSize = sizeof(szBufferW);
	ULONG nError = RegQueryValueExW(hkResult, L"ProductId", 0, NULL, (LPBYTE)szBufferW, &dwBufferWSize);

	if (ERROR_SUCCESS == nError)
	{
		std::string uf8String;
		ConvertWideToUTF8String(szBufferW, uf8String);
		m_MachineBinding1 = XmlEscape(uf8String);
	}
	else
	{
		LicenseLog("Failed to parse machine binding 1\n");
		return false;
	}

	TCHAR  infoBuf[MAX_PATH];
	GetSystemDirectory( infoBuf, MAX_PATH );

	LPCSTR pcszDrive = "\\\\.\\";
	TCHAR drive[8];
	memset(drive,0x00,8);
	strcpy(drive, pcszDrive );
	strncat(drive, infoBuf, 1);
	strncat(drive, ":", 1);

	HANDLE hPhysicalDriveIOCTL = CreateFile( drive, 0,
											FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
											OPEN_EXISTING, 0, NULL);
	if (hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
	{
		LicenseLog("Failed to parse machine binding 2\n");
		return false;
	}
	else
	{
		STORAGE_PROPERTY_QUERY query;
		DWORD cbBytesReturned = 0;
		char buffer [10000];

		memset ((void *) & query, 0, sizeof (query));
		query.PropertyId = StorageDeviceProperty;
		query.QueryType = PropertyStandardQuery;

		memset (buffer, 0, sizeof (buffer));

		if( DeviceIoControl (hPhysicalDriveIOCTL, IOCTL_STORAGE_QUERY_PROPERTY,
							 &query, sizeof (query),
							 &buffer, sizeof (buffer),
							 &cbBytesReturned, NULL) )
		{
			STORAGE_DEVICE_DESCRIPTOR * descrip = (STORAGE_DEVICE_DESCRIPTOR *) & buffer;
			char serialNumber [1000];
			FlipAndCodeBytes( buffer, descrip -> SerialNumberOffset, 1, serialNumber );
			m_MachineBinding2 = XmlEscape(serialNumber);
		}
	}
#elif UNITY_LINUX
#warning Linux: Improve unique ID for licensing
	m_MachineBinding1 = m_MachineBinding2 = systeminfo::GetDeviceUniqueIdentifier ();
#else
#error Unknown platform!
#endif

	string id = m_MachineBinding1;
	id.append(m_MachineBinding2);

#if UNITY_WIN
	std::string bios = systeminfo::GetBIOSIdentifier();
	Base64Encode((unsigned char*)bios.c_str(), bios.length(), m_MachineBinding4, 0, string());
	id.append(m_MachineBinding4);
#endif

	memset( m_MachineID, 0x00, _SHA1_DIGEST_LENGTH);
	ComputeSHA1Hash((unsigned char *)id.c_str(), id.length(), m_MachineID);
	return true;
}

int LicenseManager::Initialize ()
{
	xercesc_3_1::DOMDocument * doc = NULL;
	unsigned char * licenseData = NULL;

	if (!InitializeLocalData())
	{
		m_LicenseStatus = kLicenseErrorFlag_Initialization;
		return m_LicenseStatus;
	}

	// If a previous re-activation attempt failed and left a backup license file behind we will recover it here
	RecoverBackupLicense(false);

	m_LicenseStatus = ProcessLicense( &doc, &licenseData );

	if( doc != NULL )
		doc->release();
	delete[] licenseData;

	return m_LicenseStatus;
}


void LicenseManager::ManualActivation ()
{
	int r = -1;

	bool continueDialogs = true;
	while (continueDialogs)
	{
		r = DisplayDialogComplex ( "Information",
			"Would you like to save your license to a file and manually submit it?\n\nActivation URL: https://license.unity3d.com/manual\n\nOr do you already have a license file that you want to load?\n\n",
			"Save License", "Cancel", "Load License" );

		unsigned char * licenseData = NULL;

		string name = "Unity_v";
		string path = "";

		switch( r )
		{
			case 0:
				CreateNewCommandXML();

				name.append (UNITY_VERSION);
				name.append (".alf");
				path = RunSavePanel ("Save license information for offline activation.", "" , "alf", name);
				if( ! path.empty() && WriteStringToFile (m_Data, path, kNotAtomic, 0) )
				{
					DisplayDialog ("Information", "License file saved successfully.", "Ok");
				}
				else
				{
					DisplayDialog ("Information", "License file was not saved.", "Ok");
				}
				break;

			case 2:
				path = RunOpenPanel ("Load license information for offline activation.", "" , "ulf");
				if( ! path.empty() && ReadLicenseFile (&licenseData, path) )
				{
					m_Data = reinterpret_cast< const char* >(licenseData);

					if( WriteLicenseFile (m_Data, false) == 0 )
					{
						DisplayDialog ("Information", "License file loaded successfully.", "Ok");
						continueDialogs = false;
					}
					else
						DisplayDialog ("Information", "License file was not loaded.", "Ok");
				}
				else
				{
					DisplayDialog ("Information", "License file was not loaded.", "Ok");
				}
				break;

			case 1:
				continueDialogs = false;

			default:
				break;
		}
	}
}


void LicenseManager::HandleServerErrorAndExit ()
{
	int result = DisplayDialogComplex ("Information", "There was a problem communicating with the license server. Would you like to do a manual activation instead?", "Manual Activation", "Report a bug", "Quit");
	if (result == 0)
	{
		ManualActivation();
		std::vector<std::string> args = GetAllArguments();
		RelaunchWithArguments(args);
	}
	else if (result == 1)
	{
		LaunchBugReporter(kManualSimple);
	}

	ExitDontLaunchBugReporter ();
}


bool LicenseManager::IsLicenseUpdated()
{
	if( m_LicenseStatus == kLicenseStatus_Updated )
		return true;
	else
		return false;
}

void LicenseManager::AppendSystemInfoXML(std::string& xmlString)
{
	xmlString.append("<SystemInfo>");
	xmlString.append("<IsoCode>");
#if UNITY_WIN|| UNITY_LINUX
	xmlString.append(systeminfo::GetSystemLanguageISO());
#elif UNITY_OSX
	CFArrayRef langArray = (CFArrayRef)CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),CFSTR("Apple Global Domain"));
	CFStringRef langString = (CFStringRef)CFArrayGetValueAtIndex(langArray,0);
	CFRelease(langArray);
	CFStringRef ISO_639_1 = CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorSystemDefault, langString);
	xmlString.append(CFStringToString(ISO_639_1));
	CFRelease(ISO_639_1);
#else
#error Unknown platform!
#endif
	xmlString.append("</IsoCode>");

	xmlString.append("<UserName>");
#if UNITY_OSX || UNITY_LINUX
	xmlString.append(getenv("USER"));
#elif UNITY_WIN
	wchar_t name [ UNLEN + 1];
	DWORD size = UNLEN + 1;
	if( GetUserNameW((wchar_t*)name, &size) )
	{
		std::string utf8string;
		ConvertWideToUTF8String(name, utf8string);
		xmlString.append(XmlEscape(utf8string));
	}
	else
	{
		xmlString.append("n/a");
	}
#else
#error Unknown platform!
#endif
	xmlString.append("</UserName>");

	xmlString.append("<OperatingSystem>");
	xmlString.append(XmlEscape(systeminfo::GetOperatingSystem()));
	xmlString.append("</OperatingSystem>");

	xmlString.append("<OperatingSystemNumeric>");
	xmlString.append(IntToString(systeminfo::GetOperatingSystemNumeric()));
	xmlString.append("</OperatingSystemNumeric>");

	xmlString.append("<ProcessorType>");
	xmlString.append(XmlEscape(systeminfo::GetProcessorType()));
	xmlString.append("</ProcessorType>");

	xmlString.append("<ProcessorSpeed>");
	xmlString.append(IntToString(systeminfo::GetProcessorSpeed()));
	xmlString.append("</ProcessorSpeed>");

	xmlString.append("<ProcessorCount>");
	xmlString.append(IntToString(systeminfo::GetProcessorCount()));
	xmlString.append("</ProcessorCount>");

	xmlString.append("<ProcessorCores>");
	xmlString.append(IntToString(systeminfo::GetNumberOfCores()));
	xmlString.append("</ProcessorCores>");

	xmlString.append("<PhysicalMemoryMB>");
	xmlString.append(IntToString(systeminfo::GetPhysicalMemoryMB()));
	xmlString.append("</PhysicalMemoryMB>");

#if 0
	xmlString.append("<GraphicsName>");
	xmlString.append(gGraphicsCaps.rendererString);
	xmlString.append("</GraphicsName>");

	xmlString.append("<GraphicsVendor>");
	xmlString.append(gGraphicsCaps.vendorString);
	xmlString.append("</GraphicsVendor>");

	xmlString.append("<GraphicsVersion>");
	xmlString.append(gGraphicsCaps.fixedVersionString);
	xmlString.append("</GraphicsVersion>");

	xmlString.append("<GraphicsDriver>");
	xmlString.append(gGraphicsCaps.driverLibraryString);
	xmlString.append("</GraphicsDriver>");

	xmlString.append("<GraphicsMemoryMB>");
	xmlString.append(IntToString(gGraphicsCaps.videoMemoryMB));
	xmlString.append("</GraphicsMemoryMB>");
#endif

	xmlString.append("<ComputerName>");
	xmlString.append(XmlEscape(systeminfo::GetDeviceName()));
	xmlString.append("</ComputerName>");

	xmlString.append("<ComputerModel>");
	xmlString.append(XmlEscape(systeminfo::GetDeviceModel()));
	xmlString.append("</ComputerModel>");

	// Version tag must be in this format (version is attribute of element) or needs to be renamed to something other than UnityVersion
	xmlString.append("<UnityVersion>");
	xmlString.append(UNITY_VERSION);
	xmlString.append("</UnityVersion>");

	xmlString.append("</SystemInfo>");
}


string LicenseManager::CreateLicenseXMLSegment()
{
	string result= "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>";

	AppendSystemInfoXML(result);

	result.append("<License id=\"Terms\">");
	result.append("<MachineID Value=\"");
	result.append(GetMachineID());
	result.append("\" />");
	result.append("<MachineBindings><Binding Key=\"1\" Value=\"");
	result.append(m_MachineBinding1);
	result.append("\" /><Binding Key=\"2\" Value=\"");
	result.append(m_MachineBinding2);
	result.append("\" />");
#if UNITY_WIN
	result.append("<Binding Key=\"4\" Value=\"");
	result.append(m_MachineBinding4);
	result.append("\" />");
#endif
	result.append("</MachineBindings><UnityVersion Value=\"");
	result.append(UNITY_VERSION);
	result.append("\" /></License>");
	return result;
}


string LicenseManager::GetMachineID()
{
	std::string result=string();
	Base64Encode(m_MachineID, _SHA1_DIGEST_LENGTH, result, 0, string());
	return result;
}


string LicenseManager::GetAuthToken ()
{
	if(strncmp(m_SerialNumber, "00000000000000000000000000000000", strlen(m_SerialNumber) == 0))
	{
		return "feffffffffffffffffffffffffffffffffffffff" + DoHash("*MACHINE*"+GetMachineID());
	}
	else
	{
		return DoHash(Format("*SERIAL*%s",m_SerialNumber)) + DoHash("*MACHINE*"+GetMachineID());
	}
}


inline string LicenseManager::DoHash (string input)
{
	UInt8 hash[20];
	ComputeSHA1Hash((const UInt8*)input.c_str(), input.size(), hash);
	return SHA1ToString(hash);
}


void LicenseManager::LastCurlHttpResponseCode ( int HttpResponseCode )
{
	m_HttpResponseCode = HttpResponseCode;
}


int LicenseManager::ProcessLicense (xercesc_3_1::DOMDocument ** doc, unsigned char ** licenseData)
{
	if( ! IsDirectoryCreated( m_UnityLicenseDirPath ) )
	{
		CreateDirectory( m_UnityLicenseDirPath );
	}

	if( ! ReadLicenseFile( licenseData ) )
	{
		// Look for old license file
		if(!PaceLicenseExists())
		{
			// No PACE or ULF license found.
			StartNewLicenseFlow();

			return kLicenseErrorFlag_NoLicense;
		}
		else
		{
			return StartPaceConversionFlow();
		}
	}

	*doc = GetDOM( (*(licenseData)) );

	// Validate Unity license file is not compromised
	if( ! ValidateLicenseDocument( (*(doc)) ) )
	{
		return kLicenseErrorFlag_FileCompromised;
	}

	if (!ValidateLicenseType((*(doc))))
	{
		return kLicenseErrorFlag_VersionMismatch;
	}

	// Validate that license file is ready for use and has not expired.
	int r = ValidateDates( (*(doc)) );
	if( r!= 0 )
	{
		return r;
	}

	// Validate Unity license file is valid for this machine
	r = ValidateMachineBindings( (*(doc)) );
	if( r!= 0 )
	{
		return r;
	}

	// Get functionality
	if( ! ReadTokens( (*(doc)) ) )
	{
		return kLicenseErrorFlag_Tokens;
	}

	// Get license serial number
	if( ! ReadDeveloperData( (*(doc)) ) )
	{
		return kLicenseErrorFlag_DeveloperData;
	}

	// Cache license data for later use when running an update license check
	m_Data = reinterpret_cast< const char* >(*(licenseData));

	// Validate clock integrety based on recorded time in the XML file and update it afterwards
	if (m_HasExpirationDate)
	{
		if (VerifyXMLTime(*doc, (char*)m_MachineID))
			WriteLicenseFile(m_Data, false);
		else
			return kLicenseErrorFlag_LicenseExpired;
	}

	return kLicenseStatus_Valid;
}

bool LicenseManager::AppendPACEFile(std::string& data)
{
	int licenseDataSize = GetFileLength(m_PaceLicenseFilePath);
	unsigned char* licenseData = new unsigned char[licenseDataSize + 1];
	memset( licenseData, 0x00, licenseDataSize + 1 );
	if (ReadFromFile( m_PaceLicenseFilePath, licenseData, 0, licenseDataSize))
	{
		data.append("<![CDATA[");
		std::string sld(reinterpret_cast< const char* >(licenseData), reinterpret_cast< const char* >(licenseData) + licenseDataSize);
		data.append(sld);
		data.append("]]>");
		return true;
	}
	return false;
}

void LicenseManager::CreateNewCommandXML()
{
	m_Data = CreateLicenseXMLSegment();
	AppendPACEFile(m_Data);
	m_Data.append("</root>");
}


void LicenseManager::StartNewLicenseFlow ()
{
	m_txID.clear();
	m_rxID.clear();
	m_State = kContactLicenseServer;
	m_Command = LICENSE_CMD_NEW;
	CreateNewCommandXML();
	SendLicenseServerCommand();
}


int LicenseManager::StartPaceConversionFlow ()
{
	int licenseDataSize = GetFileLength ( m_PaceLicenseFilePath );
	unsigned char* licenseData = new unsigned char[licenseDataSize + 1];
	xercesc_3_1::DOMDocument* doc = NULL;
	int state = 0;
	memset( licenseData, 0x00, licenseDataSize + 1 );
	if (!ReadFromFile( m_PaceLicenseFilePath, licenseData, 0, licenseDataSize))
	{
		state = kLicenseErrorFlag_NoLicense;
	}
	else
	{
		doc = GetDOM(licenseData);
		// Check if PACE license file is compromised
		if (!ValidateLicenseDocument(doc))
		{
			state = kLicenseErrorFlag_FileCompromised;
		}
		else
		{
			// PACE to ULF conversion
			m_Command = LICENSE_CMD_CONVERT;
			m_LicenseStatus = kLicenseStatus_Convert;

			m_Data = CreateLicenseXMLSegment();
			m_Data.append("<![CDATA[");
			string sld ( reinterpret_cast< const char* >(licenseData), reinterpret_cast< const char* >(licenseData) + licenseDataSize);
			m_Data.append( sld );
			m_Data.append("]]></root>");

			// Get state for the PACE license
			SendLicenseServerCommand();

			state = kLicenseStatus_Unknown;
		}
	}
	delete[] licenseData;
	licenseData = NULL;
	if( doc != NULL )
		doc->release();
	return state;
}


bool LicenseManager::PaceLicenseExists ()
{
	if (IsFileCreated(m_PaceLicenseFilePath))
		return true;
	size_t location = m_PaceLicenseFilePath.find("Unity_v3.x.ilf");
	if (location == string::npos)
		return false;
	m_PaceLicenseFilePath.replace(location, 14, "UnityLicence.ilf");
	if (IsFileCreated( m_PaceLicenseFilePath ) )
		return true;
	m_PaceLicenseFilePath.replace(location, 16, "Unity_v4.x.ilf");
	if (IsFileCreated( m_PaceLicenseFilePath ) )
		return true;
	return false;
}


void LicenseManager::SendLicenseServerCommand ()
{
	if (m_UpdateCount >= kUpdateRequestLimit)
	{
		LicenseLog("Update request limit reached (license)\n");
		return;
	}
	m_UpdateCount++;
	LicenseRequestMessage * lrm = new LicenseRequestMessage( m_Data, m_Command, this );
	CurlRequestGet( lrm, kCurlRequestGroupMulti );
}


void LicenseManager::DownloadLicense(string queryParams)
{
	if (m_State == kOngoingActivationServerCommunication)
	{
		LicenseLog("Communication with activation server already in progress\n");
		return;
	}
	if (m_UpdateCount >= kUpdateRequestLimit)
	{
		LicenseLog("Update request limit reached (activation)\n");
		m_LicenseStatus = kLicenseStatus_Valid;
		return;
	}
	m_UpdateCount++;
	ActivationRequestMessage * arm = new ActivationRequestMessage( m_Data, queryParams, this );
	CurlRequestGet( arm, kCurlRequestGroupMulti );
}


bool LicenseManager::ReadLicenseFile (unsigned char ** licenseData)
{
	return ReadLicenseFile(licenseData, m_UnityLicenseFilePath);
}


bool LicenseManager::ReadLicenseFile (unsigned char ** licenseData, string path)
{
	int licenseDataSize = GetFileLength ( path );
	*licenseData = new unsigned char[licenseDataSize + 1];
	memset( *licenseData, 0x00, licenseDataSize + 1 );

	if( ! ReadFromFile( path, (*(licenseData)), 0, licenseDataSize ) )
		return false;
	else
		return true;
}


int LicenseManager::WriteLicenseFile (string licenseData, bool isBase64Encoded)
{
	DeleteFile(m_BackupUnityLicenseFile);

	AppendXMLTime(GetGMT(), licenseData, (char*)m_MachineID);

	if( isBase64Encoded )
	{
		XMLSize_t outputLength = 0;
		XMLByte* data = Base64::decode((XMLByte *)licenseData.c_str(), &outputLength);

		if( data == NULL )
		{
			return -1;
		}

		licenseData = (const char*)data;

		delete data;
	}

	// Sanity check data before replacing license file
	if( ValidateLicenseData( licenseData ) )
	{
		if( ! WriteStringToFile( licenseData, GetLicenseFilePath(), kNotAtomic, 0) )
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	return 0;
}


int LicenseManager::DeleteLicenseFile ()
{
	// Sanity check data before deleting license file
	if( ValidateLicenseData( m_Data ) )
	{
		DeleteFile( GetLicenseFilePath() );
	}
	else
	{
		return -1;
	}

	return 0;
}


void LicenseManager::TransmitLicenseFile (string cmd)
{
	unsigned char * licenseData = NULL;
	if( ! ReadLicenseFile( &licenseData ) )
	{
		// No license file loaded. This can happend when a new user run Unity for the first time.
		return;
	}

	int licenseDataSize = strlen(reinterpret_cast< const char* >(licenseData));

	string postData ( reinterpret_cast< const char* >(licenseData), reinterpret_cast< const char* >(licenseData) + licenseDataSize);
	delete [] licenseData;

	// Append current system info + unity version
	std::string rootName = "<root>";
	size_t rootElement = postData.find(rootName);
	if (rootElement != string::npos)
	{
		string versionInfo;
		AppendSystemInfoXML(versionInfo);
		postData.insert(rootElement + rootName.length(), versionInfo);
	}

	// Cache license data for activation server (it is triggered after license request message)
	m_Data = postData.c_str();
	m_Command = cmd;

	SendLicenseServerCommand();
}


void LicenseManager::CallActivationServer ()
{
	string TX_RX_IDS;
	TX_RX_IDS.append(m_txID);
	TX_RX_IDS.append(";");
	TX_RX_IDS.append(m_rxID);

	CallActivationServer(TX_RX_IDS);
}

void LicenseManager::CallActivationServer (string TX_RX_IDS)
{
	std::vector<std::string> TX_RX;
	std::stringstream f(TX_RX_IDS.c_str());
	std::string s;
	while (std::getline(f, s, ';'))
	{
		TX_RX.push_back(s);
	}

	if( TX_RX.size() != 2 )
		return;

	string queryParams (m_Command);
	queryParams.append ("&TX=");
	queryParams.append (TX_RX.at(0));
	queryParams.append ("&RX=");
	queryParams.append (TX_RX.at(1));

	DownloadLicense (queryParams);
}


void LicenseManager::QueryLicenseUpdate ()
{
	// Only allow one licens update at a time
	if( m_LicenseStatus != kLicenseStatus_Update )
		TransmitLicenseFile (LICENSE_CMD_UPDATE);
}


void LicenseManager::ReturnLicense ()
{
	TransmitLicenseFile( LICENSE_CMD_RETURN );
}


void LicenseManager::NewActivation ()
{
	if(IsFileCreated(m_UnityLicenseFilePath))
		MoveReplaceFile(m_UnityLicenseFilePath, m_BackupUnityLicenseFile);
	StartNewLicenseFlow();
}

bool LicenseManager::RecoverBackupLicense(bool setLicenseState)
{
	if (IsFileCreated(m_BackupUnityLicenseFile) && !IsFileCreated(m_UnityLicenseFilePath))
	{
		if (MoveReplaceFile(m_BackupUnityLicenseFile, m_UnityLicenseFilePath))
		{
			// Set state to Valid, whatever license was validated last will now be used again, don't do this on initialization
			if (setLicenseState)
				m_LicenseStatus = kLicenseStatus_Valid;
			LicenseLog("Recovered backup license file\n");
			return true;
		}
	}
	return false;
}


bool LicenseManager::ReadDeveloperData (xercesc_3_1::DOMDocument * doc)
{
	bool r = true;

	XMLCh* _ch = XMLString::transcode("DeveloperData");
	DOMNodeList* elemList = doc->getElementsByTagName(_ch);
	XMLString::release(&_ch);
	DOMNode* node = elemList->item(0);

	XMLCh* ATTR_Value = XMLString::transcode("Value");
	DOMNamedNodeMap * attributes = node->getAttributes();
	DOMNode * attrValue = attributes->getNamedItem(ATTR_Value);
	string value = XMLString::transcode(attrValue->getNodeValue());
	XMLString::release(&ATTR_Value);

	XMLSize_t outputLength = 0;
	XercesMemoryManager *mm = new XercesMemoryManager();
	XMLByte* developerData = Base64::decode((XMLByte *)value.c_str(), &outputLength, mm);

	if( developerData == NULL || outputLength != 31 )
	{
		r = false;
	}
	else
	{
		m_ConcurrentUsers = (int)ntohl(atol((const char*)developerData));

		memcpy( m_SerialNumber, developerData + 4, 28 );
	}

	UNITY_FREE(kMemDefault, developerData);

	return r;
}


char * LicenseManager::SerialNumber ()
{
	return m_SerialNumber;
}


bool LicenseManager::ReadTokens (xercesc_3_1::DOMDocument * doc)
{
	XMLCh* _ch = XMLString::transcode("Features");
	DOMNodeList* elemList = doc->getElementsByTagName(_ch);
	XMLString::release(&_ch);
	DOMNodeList* children = elemList->item(0)->getChildNodes();
	const XMLSize_t nodeCount = children->getLength();

	XMLCh* TAG_Feature = XMLString::transcode("Feature");
	XMLCh* ATTR_Value = XMLString::transcode("Value");

	m_Tokens = 0;

	for( XMLSize_t xx = 0; xx < nodeCount; ++xx )
	{
		DOMNode* currentNode = children->item(xx);
		if( currentNode->getNodeType() && currentNode->getNodeType() == DOMNode::ELEMENT_NODE )
		{
			if( XMLString::equals(currentNode->getNodeName(), TAG_Feature) )
			{
				DOMNamedNodeMap * attributes = currentNode->getAttributes();
				DOMNode * attrValue = attributes->getNamedItem(ATTR_Value);
				string value = XMLString::transcode(attrValue->getNodeValue());
				UInt64 one = 1; // when writing just (1<<foo), VS2008 will generate a 32 bit result
				m_Tokens |= (one << atoi(value.c_str()) );
			}
		}
	}
	
	// Allow you to force a free license no matter what kind of license was activated (overwrite tokens above)
	if (HasARGV("force-free"))
	{
		// Enable bits 1/3/17/19/62 (ios/android/bb10/winrt basic + free)
		m_Tokens = 0x40000000000A000AULL;
	}

	XMLString::release(&ATTR_Value);
	XMLString::release(&TAG_Feature);

	return true;
}


UInt64 LicenseManager::GetTokens ()
{
	return m_Tokens;
}


string LicenseManager::GetDate (const char* ElementName, xercesc_3_1::DOMDocument* doc)
{
	XMLCh* ATTR_Value = XMLString::transcode("Value");

	XMLCh* _ch = XMLString::transcode(ElementName);
	DOMNodeList* elemList = doc->getElementsByTagName(_ch);
	XMLString::release(&_ch);

	DOMNode* currentNode = elemList->item(0);

	DOMNamedNodeMap * attributes = currentNode->getAttributes();
	DOMNode * attrValue = attributes->getNamedItem(ATTR_Value);
	return XMLString::transcode(attrValue->getNodeValue());
}


int LicenseManager::ValidateDates (xercesc_3_1::DOMDocument* doc)
{
	time_t rawtime;
	struct tm * ptm;
	time (&rawtime);
	ptm = gmtime (&rawtime);

	std::stringstream formattedTodayString;
	formattedTodayString << ptm->tm_year + 1900 << "-" << std::setfill('0') << std::setw(2) << ptm->tm_mon + 1 << "-" << std::setfill('0') << std::setw(2) << ptm->tm_mday << "T00:00:00";
	XMLDateTime* todayDate = new XMLDateTime();
	XMLCh* dateString = XMLString::transcode(formattedTodayString.str().c_str());
	todayDate->setBuffer(dateString);
	todayDate->parseDateTime();
	XMLString::release(&dateString);

	std::string startString = GetDate("StartDate", doc);
	std::string stopString = GetDate("StopDate", doc);
	// If either of the start or stop dates are present, then continue validation, else skip this entirely
	if (startString.length() != 0 || stopString.length() != 0)
	{
		if (startString.length() < 1)
			startString = formattedTodayString.str().c_str();

		XMLDateTime* startDate = new XMLDateTime();
		dateString = XMLString::transcode(startString.c_str());
		startDate->setBuffer(dateString);
		startDate->parseDateTime();
		XMLString::release(&dateString);

		if (XMLDateTime::compare(todayDate, startDate) == XMLDateTime::LESS_THAN)
		{
			// License is not ready for use because StartDate is in the future.
			return kLicenseErrorFlag_LicenseNotReadyForUse;
		}

		if (stopString.length() < 1)
			stopString = "2112-01-27T08:00:00";
		else
			m_HasExpirationDate = true;

		XMLDateTime* stopDate = new XMLDateTime();
		dateString = XMLString::transcode(stopString.c_str());
		stopDate->setBuffer(dateString);
		stopDate->parseDateTime();
		XMLString::release(&dateString);

		if (XMLDateTime::compare(todayDate, stopDate) == XMLDateTime::GREATER_THAN || !VerifyTime())
			return kLicenseErrorFlag_LicenseExpired;
	}
	else
		LicenseLog("No start/stop license dates set");

	std::string updateString = GetDate("UpdateDate", doc);
	LicenseLog("Next license update check is after %s\n", updateString.c_str());
	XMLDateTime* updateDate = new XMLDateTime();
	dateString = XMLString::transcode(updateString.c_str());
	updateDate->setBuffer(dateString);
	updateDate->parseDateTime();
	XMLString::release(&dateString);

	if (XMLDateTime::compare(todayDate, updateDate) == XMLDateTime::GREATER_THAN)
	{
		m_MustUpdate = true;
	}
	else
	{
		m_MustUpdate = false;
	}

	return 0;
}


int LicenseManager::ValidateMachineBindings (xercesc_3_1::DOMDocument* doc)
{
	XMLCh* _ch = XMLString::transcode("MachineBindings");
	DOMNodeList* elemList = doc->getElementsByTagName(_ch);
	XMLString::release(&_ch);
	DOMNodeList* children = elemList->item(0)->getChildNodes();
	const XMLSize_t nodeCount = children->getLength();

	XMLCh* TAG_Binding = XMLString::transcode("Binding");
	XMLCh* ATTR_Value = XMLString::transcode("Value");
	XMLCh* ATTR_Key = XMLString::transcode("Key");

	int r = 0;
	for( XMLSize_t xx = 0; xx < nodeCount; ++xx )
	{
		DOMNode* currentNode = children->item(xx);
		if( currentNode->getNodeType() && currentNode->getNodeType() == DOMNode::ELEMENT_NODE )
		{
			if( XMLString::equals(currentNode->getNodeName(), TAG_Binding))
			{
				DOMNamedNodeMap * attributes = currentNode->getAttributes();
				DOMNode * attrKey = attributes->getNamedItem(ATTR_Key);
				string key = XMLString::transcode(attrKey->getNodeValue());
				DOMNode * attrValue = attributes->getNamedItem(ATTR_Value);
				string value = XMLString::transcode(attrValue->getNodeValue());
				if( key.compare( "1" ) == 0 )
				{
					if( m_MachineBinding1.compare(XmlEscape(value)) != 0 )
					{
						LicenseLog("%s != %s\n", m_MachineBinding1.c_str(), value.c_str());
						// If the ProductID or IOPlatformUUID has changed continue checking other bindings
						// if it is the only binding which changed we have special handling for this
						r = kLicenseErrorFlag_MachineBinding1;
					}
				}
				else if( key.compare("2") == 0 )
				{
					if( m_MachineBinding2.compare(XmlEscape(value)) != 0 )
					{
						LicenseLog("%s != %s\n", m_MachineBinding2.c_str(), value.c_str());
						r = kLicenseErrorFlag_MachineBinding2;
						break;
					}
				}
#if UNITY_WIN
				else if( key.compare("4") == 0 )
				{
					if( m_MachineBinding4.compare(value) != 0 )
					{
						LicenseLog("%s != %s\n", m_MachineBinding4.c_str(), value.c_str());
						r = kLicenseErrorFlag_MachineBinding4;
						break;
					}
				}
#endif
			}
		}
	}

	XMLString::release(&ATTR_Key);
	XMLString::release(&ATTR_Value);
	XMLString::release(&TAG_Binding);

	return r;
}


// Only fail when LicenseVersion tag is actually found and checked. If
// we also fail when it's not found existing licenses will stop working.
bool LicenseManager::ValidateLicenseType (xercesc_3_1::DOMDocument * doc)
{
	XMLCh* valueAttribute = XMLString::transcode("Value");
	XMLCh* licenseValueElement = XMLString::transcode("LicenseVersion");

	DOMNodeList* elemList = doc->getElementsByTagName(licenseValueElement);
	if (!elemList)
		return true;
	XMLString::release(&licenseValueElement);

	DOMNode* currentNode = elemList->item(0);
	if (!currentNode)
		return true;

	DOMNamedNodeMap * attributes = currentNode->getAttributes();
	DOMNode * attrValue = attributes->getNamedItem(valueAttribute);
	XMLString::release(&valueAttribute);

	std::string licenseVersion = XMLString::transcode(attrValue->getNodeValue());
	if (licenseVersion.length() > 0 && licenseVersion[0] == UNITY_VERSION_VER+48)
		return true;
	return false;
}


bool LicenseManager::ValidateLicenseData (string data)
{
	xercesc_3_1::DOMDocument * doc = GetDOM( (unsigned char *)data.c_str() );
	bool r = ValidateLicenseDocument( doc );
	doc->release();
	return r;
}


bool LicenseManager::ValidateLicenseDocument (xercesc_3_1::DOMDocument * doc)
{
	XMLCh* TAG_RootName = XMLString::transcode("root");
	DOMElement* elementRoot = doc->getDocumentElement();

	XMLCh* TAG_SignatureName = XMLString::transcode("Signature");
	DOMNodeList* sigElemList = doc->getElementsByTagName(TAG_SignatureName);

	if (sigElemList->getLength() < 1)
	{
		XMLString::release(&TAG_SignatureName);
		TAG_SignatureName = XMLString::transcode("dsig:Signature");
		sigElemList = doc->getElementsByTagName(TAG_SignatureName);
	}

	if( elementRoot == NULL || sigElemList == NULL || sigElemList->getLength() < 1 )
	{
		LicenseLog("Could not find XML root element\n");
		return false;
	}

	if( XMLString::equals( elementRoot->getTagName(), TAG_RootName ) )
	{
		// License file start with <root>
		cert = cert_unity;
	}
	else
	{
		// License file start with <Signature>
		cert = cert_pace;
	}

	XMLString::release(&TAG_RootName);
	XMLString::release(&TAG_SignatureName);

	bool r = true;

	OpenSSLCryptoX509* x509 = new OpenSSLCryptoX509();
	XSECProvider prov;
	DSIGSignature* sig;
	try
	{
		x509->loadX509Base64Bin(cert, strlen(cert));
		sig = prov.newSignatureFromDOM(doc, sigElemList->item(0));
		sig->load();
		sig->setSigningKey(x509->clonePublicKey());
		if (!sig->verify())
		{
			LicenseLog("Failed to verify signature\n");
			r = false;
		}
	}
	catch (XSECCryptoException &e)
	{
		LicenseLog("Exception while verifying signature\n");
		r = false;
	}

	delete x509;
	prov.releaseSignature(sig);

	return r;
}

string LicenseManager::GetLicenseFilePath ()
{
	return m_UnityLicenseFilePath;
}


bool LicenseManager::UnknownStatus()
{
	return m_LicenseStatus == kLicenseStatus_Unknown;
}


const std::string LicenseManager::GetLicenseURL()
{
	std::string url = m_BaseLicenseURL;
	url.append("/start?tx_id=");
	url.append(m_txID);

	std::string loader_template = AppendPathName (GetApplicationContentsPath (), "Resources/licenseloader_template.html");

	// Load loader html, and patch it up to redirect to the actual url we want to use,
	// then write to a temp file.
	InputString htmlInputString;
	if (!ReadStringFromFile (&htmlInputString, loader_template))
	{
		DisplayDialog ("Error", "Could not read license loader html.", "Quit");
		ExitDontLaunchBugReporter ();
	}

	std::string html(htmlInputString.c_str());
	std::string replaceKey = "__URL__";
	size_t pos = html.find(replaceKey);
	if (pos == std::string::npos)
	{
		DisplayDialog ("Error", "Could not read license loader html.", "Quit");
		ExitDontLaunchBugReporter ();
	}
	std::string patchedHTML = html.substr (0, pos) + url + html.substr (pos + replaceKey.size());
#if UNITY_OSX || UNITY_LINUX
	std::string loader = "/tmp/licenseloader.html";
#elif UNITY_WIN
	wchar_t tempPath[MAX_PATH];
	std::string loader;
	DWORD size = GetTempPathW(MAX_PATH, tempPath);
	if (size > 0 || size < MAX_PATH)
	{
		ConvertWindowsPathName(tempPath, loader);
		loader.append("/licenseloader.html");
	}
	else
	{
		DisplayDialog ("Error", "Could not find license loader directory.", "Quit");
		ExitDontLaunchBugReporter ();
	}
#else
#error Unknown platform!
#endif
	if (!WriteBytesToFile (patchedHTML.c_str(), patchedHTML.length(), loader))
	{
		DisplayDialog ("Error", "Could not write license loader html.", "Quit");
		ExitDontLaunchBugReporter ();
	}

#if UNITY_WIN
	return "file:///" + loader;
#else
	return loader;
#endif
}


//  Function to decode the serial numbers of IDE hard drives
//  using the IOCTL_STORAGE_QUERY_PROPERTY command
char* LicenseManager::FlipAndCodeBytes (const char* str, int pos, int flip, char* buf)
{
	int i;
	int j = 0;
	int k = 0;

	buf [0] = '\0';
	if (pos <= 0)
		return buf;

	if ( ! j)
	{
		char p = 0;

		// First try to gather all characters representing hex digits only.
		j = 1;
		k = 0;
		buf[k] = 0;
		for (i = pos; j && str[i] != '\0'; ++i)
		{
			char c = tolower(str[i]);

			if (isspace(c))
				c = '0';

			++p;
			buf[k] <<= 4;

			if (c >= '0' && c <= '9')
				buf[k] |= (unsigned char) (c - '0');
			else if (c >= 'a' && c <= 'f')
				buf[k] |= (unsigned char) (c - 'a' + 10);
			else
			{
				j = 0;
				break;
			}

			if (p == 2)
			{
				if (buf[k] != '\0' && ! isprint((unsigned char)buf[k]))
				{
					j = 0;
					break;
				}
				++k;
				p = 0;
				buf[k] = 0;
			}

		}
	}

	if ( ! j)
	{
		// There are non-digit characters, gather them as is.
		j = 1;
		k = 0;
		for (i = pos; j && str[i] != '\0'; ++i)
		{
			char c = str[i];

			if ( ! isprint((unsigned char)c))
			{
				j = 0;
				break;
			}

			buf[k++] = c;
		}
	}

	if ( ! j)
	{
		// The characters are not there or are not printable.
		k = 0;
	}

	buf[k] = '\0';

	if (flip)
		// Flip adjacent characters
		for (j = 0; j < k; j += 2)
		{
			char t = buf[j];
			buf[j] = buf[j + 1];
			buf[j + 1] = t;
		}

	// Trim any beginning and end space
	i = j = -1;
	for (k = 0; buf[k] != '\0'; ++k)
	{
		if (! isspace(buf[k]))
		{
			if (i < 0)
				i = k;
			j = k;
		}
	}

	if ((i >= 0) && (j >= 0))
	{
		for (k = i; (k <= j) && (buf[k] != '\0'); ++k)
			buf[k - i] = buf[k];
		buf[k - i] = '\0';
	}

	return buf;
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

void AppendBogusXMLTime(const char* bogusTime, std::string &data)
{
	std::string rootName = "<root>";
	size_t rootElement = data.find(rootName);
	if (rootElement != string::npos)
	{
		std::string stringValue = Format("  <TimeStamp Value=\"%s\"/>\n", bogusTime);
		data.insert(rootElement + rootName.length(), stringValue);
	}
}

int GetXMLTimestampCount(std::string &testXML)
{
	int count = 0;
	size_t pos = 0;
	while ((pos = testXML.find("TimeStamp", pos+1)) != 0)
	{
		if (pos == string::npos)
			break;
		count++;
	}
	return count;
}

void GetHWID(unsigned char* hwid, const char* binding1, const char* binding2, const char* binding3)
{
	memset(hwid, 0x00, _SHA1_DIGEST_LENGTH);
	std::string id;
	id.append(binding1);
	id.append(binding2);
	id.append(binding3);
	ComputeSHA1Hash((unsigned char *)id.c_str(), id.length(), hwid);
}

#if UNITY_OSX
void InitNSAutoreleasePool();
void ReleaseNSAutoreleasePool();
#endif

SUITE (LicenseTests)
{
	TEST(LicenseTimerTests)
	{
#if UNITY_OSX
		InitNSAutoreleasePool();
#endif
		// Make sure absense of key doesn't fail time verification
		EditorPrefs::DeleteKey("LSValue");
		CHECK(VerifyTime());

		// Verify time is recorded consistently
		CHECK(EditorPrefs::HasKey("LSValue"));

		// Verify setting the recorded time forward by 1/2 hour (same as setting local clock back 1 hour) is ok
		int pastTime = (GetGMT()+1800) ^ 0xF857CBAF;
		EditorPrefs::SetInt("LSValue", pastTime);
		CHECK(VerifyTime());

		// ... but 3 hours won't work
		pastTime = (GetGMT()+3*3600) ^ 0xF857CBAF;
		EditorPrefs::SetInt("LSValue", pastTime);
		CHECK(VerifyTime() == false);

		// Normal operation works (check 3 hours after recording)
		pastTime = (GetGMT()-3*3600) ^ 0xF857CBAF;
		EditorPrefs::SetInt("LSValue", pastTime);
		CHECK(VerifyTime());
#if UNITY_OSX
		ReleaseNSAutoreleasePool();
#endif
	}

// Disable until random error can be fixed
#if 0
	TEST(LicenseFileTimeStamp)
	{
		const std::string sourceLicenseXML = "<root>\n  <License id=\"Terms\"><ClientProvidedVersion Value=\"4.0.0f4\"/></License></root>";
		const std::string sourceLicenseXMLf3 = "<root>\n  <License id=\"Terms\"><ClientProvidedVersion Value=\"4.0.0f3\"/></License></root>";
		const std::string sourceLicenseXMLf6 = "<root>\n  <License id=\"Terms\"><ClientProvidedVersion Value=\"4.0.0f6\"/></License></root>";
		try
		{
			XMLPlatformUtils::Initialize();
		}
		catch( const XMLException &e )
		{
			printf_console("Failed to initialize XML utilities\n");
		}

		const char* hwid = "fumjh4eYJNNzLPQh02sSfyZpzyI=";
		
		// Make sure Base64 encoding does include null terminator and data beyond it
		unsigned char nullTest[5] = { '1', '2', 0, '3', '4' };
		std::string encoded = Encode(nullTest, 5);
		unsigned char decoded[kBufferSize];
		Decode((const unsigned char*)encoded.c_str(), encoded.length(), decoded);
		for (int i = 0; i < 5; ++i)
			CHECK_EQUAL(nullTest[i], decoded[i]);

		int time = GetGMT();
		std::string encrypted = Encrypt(time, hwid);
		int decryptedTime = Decrypt(encrypted, hwid);
		CHECK_EQUAL(time, decryptedTime);

		// Verify inserting and fetching timestamp from xml file works
		std::string testXML = sourceLicenseXML;
		AppendXMLTime(time, testXML, hwid);
		xercesc_3_1::DOMDocument* doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK_EQUAL(time, GetXMLTime(doc, hwid));

		// Checking the recorded timestamp works
		CHECK(VerifyXMLTime(doc, hwid));
		doc->release();


		// Add a duplicate TimeStamp to try and mess things up
		testXML.insert(6, "\n<TimeStamp Value=\"1111111111\"/>\n");

		// Verify previous time is overwritten properly
		int halfHour = GetGMT()+1800;
		AppendXMLTime(halfHour, testXML, hwid);
		CHECK_EQUAL(1, GetXMLTimestampCount(testXML));
		CHECK(ValidateXML(testXML));


		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK_EQUAL(halfHour, GetXMLTime(doc, hwid));
		CHECK(VerifyXMLTime(doc, hwid)); // 1800 second discrepency is ok
		if (doc)
			doc->release();


		// Appending a few times keeps xml correct
		AppendXMLTime(111111, testXML, hwid);
		AppendXMLTime(222222, testXML, hwid);
		AppendXMLTime(333333, testXML, hwid);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK_EQUAL(333333, GetXMLTime(doc, hwid));
		CHECK_EQUAL(1, GetXMLTimestampCount(testXML));
		CHECK(ValidateXML(testXML));


		// Verify error when the current time is in the past compared to timestamp
		AppendXMLTime(GetGMT()+7200, testXML, hwid);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK(VerifyXMLTime(doc, hwid) == false);
		if (doc)
			doc->release();

		// Verify result when there is bogus text inside
		testXML = sourceLicenseXML;
		AppendBogusXMLTime("blabla", testXML);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK_EQUAL(0, GetXMLTime(doc, hwid));
		if (doc)
			doc->release();

		// Verify result when there is unescaped xml inside
		testXML = sourceLicenseXML;
		AppendBogusXMLTime("&&&&", testXML);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK_EQUAL(0, GetXMLTime(doc, hwid));
		if (doc)
			doc->release();

		// Verify result when there is no timestamp
		doc = GetDOM((const unsigned char*)sourceLicenseXML.c_str());
		CHECK_EQUAL(0, GetXMLTime(doc, hwid));

		// 4.0.0f4 should pass client check
		CHECK(CheckXMLClientVersion(doc, "4.0.0f3"));
		if (doc)
			doc->release();

		// and 4.0.0f3 should not pass
		doc = GetDOM((const unsigned char*)sourceLicenseXMLf3.c_str());
		CHECK(CheckXMLClientVersion(doc, "4.0.0f3") == false);
		if (doc)
			doc->release();

		// Verify the old type recorded time (shipped in 4.0.0f4)
		testXML = sourceLicenseXML;
		AppendBogusXMLTime("2832067573", testXML);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK(VerifyXMLTime(doc, hwid));
		if (doc)
			doc->release();

		// Verify the old type recorded time doesn't work in 4.0.0f6
		testXML = sourceLicenseXMLf6;
		AppendBogusXMLTime("2832067573", testXML);
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK(VerifyXMLTime(doc, hwid) == false);
		if (doc)
			doc->release();

		// Case/bug where verifying XML time failed because of zeros which appeard in the encrypted data blob (at location 0)
		testXML = sourceLicenseXMLf6;
		unsigned char customHwid[_SHA1_DIGEST_LENGTH];
		GetHWID(customHwid, "00178-50000-05479-AA715", "VCRP3105309U61D0NG", "Q05EMDIzMUNWRA==");
		AppendXMLTime(1354112428, testXML, (char*)customHwid);
		
		doc = GetDOM((const unsigned char*)testXML.c_str());
		CHECK(VerifyXMLTime(doc, (char*)customHwid));
		if (doc)
			doc->release();

		XMLPlatformUtils::Terminate();
	}
#endif
}
#endif // ENABLE_UNIT_TESTS

