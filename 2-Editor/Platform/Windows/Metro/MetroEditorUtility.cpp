#include "UnityPrefix.h"
#include "MetroEditorUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Runtime/Misc/PlayerSettings.h"

#if UNITY_WIN

#include <wincrypt.h>

using namespace std;

namespace
{
	HRESULT MetroCreateTestCertificateInternal(string const& path, string const& publisher, string const& password, bool overwrite)
	{
		static LPWSTR const kContainer = L"Unity";

		HRESULT hr = E_FAIL;
		bool deleteKeySet = false;
		HCRYPTPROV provider = NULL;
		HCRYPTKEY key = NULL;
		LPWSTR issuerX500 = NULL;
		BYTE* issuerEncoded = NULL;
		BYTE* basicConstraintsEncoded = NULL;
		BYTE* ekuEncoded = NULL;
		PCCERT_CONTEXT certificate = NULL;
		HCERTSTORE store = NULL;
		CRYPT_DATA_BLOB pfxBlob;
		HANDLE file = INVALID_HANDLE_VALUE;

		pfxBlob.pbData = NULL;

		std::wstring pathWide;
		ConvertUnityPathName(path, pathWide);

		std::wstring publisherWide = Utf8ToWide(publisher);
		std::wstring passwordWide  = Utf8ToWide(password);

		// acquire key container

		BOOL const acquireContextResult = CryptAcquireContextW(&provider, kContainer, NULL, PROV_RSA_FULL, CRYPT_SILENT);

		if (FALSE == acquireContextResult)
		{
			BOOL const acquireNewContextResult = CryptAcquireContextW(&provider, kContainer, NULL, PROV_RSA_FULL, (CRYPT_NEWKEYSET | CRYPT_SILENT));
			Assert(FALSE != acquireNewContextResult);

			if (FALSE == acquireNewContextResult)
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				goto cleanup;
			}

			deleteKeySet = true;
		}

		// generate public/private key pair

		BOOL const genKeyResult = CryptGenKey(provider, AT_SIGNATURE, (0x08000000/*RSA2048BIT_KEY*/ | CRYPT_EXPORTABLE), &key);
		Assert(FALSE != genKeyResult);

		if (FALSE == genKeyResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// create issuer X.500 string

		size_t const publisherLength = publisherWide.length();
		size_t issuerX500Size = (3 + publisherLength + 1);
		bool protect = false;

		for (size_t i = 0; i < publisherLength; ++i)
		{
			if (L'"' == publisherWide[i])
			{
				++issuerX500Size;
				protect = true;
			}
			else if ((L' ' == publisherWide[i]) || (L'\t' == publisherWide[i]) || (L'\r' == publisherWide[i]) || (L'\n' == publisherWide[i]))
			{
				protect = true;
			}
		}

		if (protect)
		{
			issuerX500Size += 2;
		}

		issuerX500 = new WCHAR[issuerX500Size];
	
		LPWSTR dst = issuerX500;

		*dst++ = L'C';
		*dst++ = L'N';
		*dst++ = L'=';

		if (protect)
		{
			*dst++ = L'"';
		}

		LPCWSTR src = publisherWide.c_str();

		for (size_t i = 0; i < publisherLength; ++i)
		{
			if (L'"' == *src)
			{
				*dst++ = L'"';
			}

			*dst++ = *src++;
		}

		if (protect)
		{
			*dst++ = L'"';
		}

		*dst++ = L'\0';

		// encode issuer

		DWORD issuerEncodedSize;

		BOOL const strToNameLengthResult = CertStrToNameW(X509_ASN_ENCODING, issuerX500, CERT_X500_NAME_STR, NULL, NULL, &issuerEncodedSize, NULL);
		Assert(FALSE != strToNameLengthResult);

		if (FALSE == strToNameLengthResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		issuerEncoded = new BYTE[issuerEncodedSize];

		BOOL const strToNameResult = CertStrToNameW(X509_ASN_ENCODING, issuerX500, CERT_X500_NAME_STR, NULL, issuerEncoded, &issuerEncodedSize, NULL);
		Assert(FALSE != strToNameResult);

		if (FALSE == strToNameResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// initialize extensions

		CERT_EXTENSION extensionArray[2];

		CERT_EXTENSIONS extensions;
		extensions.cExtension = ARRAYSIZE(extensionArray);
		extensions.rgExtension = extensionArray;

		// initialize basic constraints

		CERT_BASIC_CONSTRAINTS2_INFO basicConstraintsInfo;

		basicConstraintsInfo.fCA = FALSE;
		basicConstraintsInfo.fPathLenConstraint = FALSE;
		basicConstraintsInfo.dwPathLenConstraint = 0;

		DWORD basicConstraintsEncodedSize;

		BOOL const encodeBasicConstaintsGetSizeResult = CryptEncodeObject(X509_ASN_ENCODING, szOID_BASIC_CONSTRAINTS2, &basicConstraintsInfo, NULL, &basicConstraintsEncodedSize);
		Assert(FALSE != encodeBasicConstaintsGetSizeResult);

		if (FALSE == encodeBasicConstaintsGetSizeResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		basicConstraintsEncoded = new BYTE[basicConstraintsEncodedSize];

		BOOL const encodeBasicConstaintsResult = CryptEncodeObject(X509_ASN_ENCODING, szOID_BASIC_CONSTRAINTS2, &basicConstraintsInfo, basicConstraintsEncoded, &basicConstraintsEncodedSize);
		Assert(FALSE != encodeBasicConstaintsResult);

		if (FALSE == encodeBasicConstaintsResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		extensionArray[0].pszObjId = szOID_BASIC_CONSTRAINTS2;
		extensionArray[0].fCritical = TRUE;
		extensionArray[0].Value.cbData = basicConstraintsEncodedSize;
		extensionArray[0].Value.pbData = basicConstraintsEncoded;

		// initialize enhanced key usage (code signing only)

		static LPSTR kUsageIdentifiers[] =
		{
			szOID_PKIX_KP_CODE_SIGNING
		};

		CERT_ENHKEY_USAGE eku;
		eku.cUsageIdentifier = ARRAYSIZE(kUsageIdentifiers);
		eku.rgpszUsageIdentifier = kUsageIdentifiers;

		DWORD ekuEncodedSize;

		BOOL const encodeEKUGetSizeResult = CryptEncodeObject(X509_ASN_ENCODING, szOID_ENHANCED_KEY_USAGE, &eku, NULL, &ekuEncodedSize);
		Assert(FALSE != encodeEKUGetSizeResult);

		if (FALSE == encodeEKUGetSizeResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		ekuEncoded = new BYTE[ekuEncodedSize];

		BOOL const encodeEKUResult = CryptEncodeObject(X509_ASN_ENCODING, szOID_ENHANCED_KEY_USAGE, &eku, ekuEncoded, &ekuEncodedSize);
		Assert(FALSE != encodeEKUResult);

		if (FALSE == encodeEKUResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		extensionArray[1].pszObjId = szOID_ENHANCED_KEY_USAGE;
		extensionArray[1].fCritical = TRUE;
		extensionArray[1].Value.cbData = ekuEncodedSize;
		extensionArray[1].Value.pbData = ekuEncoded;

		// create self-signed certificate

		CERT_NAME_BLOB issuerBlob;

		issuerBlob.cbData = issuerEncodedSize;
		issuerBlob.pbData = issuerEncoded;

		CRYPT_KEY_PROV_INFO kpi;

		kpi.pwszContainerName = kContainer;
		kpi.pwszProvName = NULL;
		kpi.dwProvType = PROV_RSA_FULL;
		kpi.dwFlags = CRYPT_SILENT;
		kpi.cProvParam = 0;
		kpi.rgProvParam = NULL;
		kpi.dwKeySpec = AT_SIGNATURE;

		CRYPT_ALGORITHM_IDENTIFIER signatureAlgorithm;

		signatureAlgorithm.pszObjId = szOID_RSA_SHA256RSA;
		signatureAlgorithm.Parameters.cbData = 0;
		//signatureAlgorithm.Parameters.pbData = NULL;

		certificate = CertCreateSelfSignCertificate(NULL, &issuerBlob, 0, &kpi, &signatureAlgorithm, NULL, NULL, &extensions);
		Assert(NULL != certificate);

		if (NULL == certificate)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// open certificate store

		store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, CERT_STORE_CREATE_NEW_FLAG, NULL);
		Assert(NULL != store);

		if (NULL == store)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// add certificate to the store

		BOOL const addCertificateContextToStoreResult = CertAddCertificateContextToStore(store, certificate, CERT_STORE_ADD_NEW, NULL);
		Assert(FALSE != addCertificateContextToStoreResult);

		if (FALSE == addCertificateContextToStoreResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// export certificate
	
		pfxBlob.cbData = 0;

		PFXExportCertStoreEx(store, &pfxBlob, passwordWide.c_str(), NULL, (EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY | PKCS12_INCLUDE_EXTENDED_PROPERTIES));

		pfxBlob.pbData = new BYTE[pfxBlob.cbData];

		BOOL const exportCertStoreExResult = PFXExportCertStoreEx(store, &pfxBlob, passwordWide.c_str(), NULL, (EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY | PKCS12_INCLUDE_EXTENDED_PROPERTIES));
		Assert(FALSE != exportCertStoreExResult);

		if (FALSE == exportCertStoreExResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// create certificate file

		file = CreateFileW(pathWide.c_str(), GENERIC_WRITE, 0, NULL, (overwrite ? CREATE_ALWAYS : CREATE_NEW), FILE_ATTRIBUTE_NORMAL, NULL);

		if (INVALID_HANDLE_VALUE == file)
		{
			DWORD const error = GetLastError();
			Assert(!overwrite && (ERROR_FILE_EXISTS == error));
			hr = HRESULT_FROM_WIN32(error);
			goto cleanup;
		}

		// store certificate data

		DWORD written;

		BOOL const writeFileResult = WriteFile(file, pfxBlob.pbData, pfxBlob.cbData, &written, NULL);
		Assert(FALSE != writeFileResult);

		if (FALSE == writeFileResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		Assert(written == pfxBlob.cbData);

		if (written != pfxBlob.cbData)
		{
			goto cleanup;
		}

		// done

		hr = S_OK;

		// cleanup

	cleanup:

		if (INVALID_HANDLE_VALUE != file)
		{
			BOOL const closeFileResult = CloseHandle(file);
			Assert(FALSE != closeFileResult);
		}

		delete[] pfxBlob.pbData;

		if (NULL != store)
		{
			BOOL const closeStoreResult = CertCloseStore(store, 0);
			Assert(FALSE != closeStoreResult);
		}

		if (NULL != certificate)
		{
			BOOL const freeCertificateContextResult = CertFreeCertificateContext(certificate);
			Assert(FALSE != freeCertificateContextResult);
		}

		delete[] ekuEncoded;
		delete[] basicConstraintsEncoded;
		delete[] issuerEncoded;
		delete[] issuerX500;

		if (NULL != key)
		{
			BOOL const destroyKeyResult = CryptDestroyKey(key);
			Assert(FALSE != destroyKeyResult);
		}

		if (NULL != provider)
		{
			BOOL const releaseContextResult = CryptReleaseContext(provider, 0);
			Assert(FALSE != releaseContextResult);
		}

		if (deleteKeySet)
		{
			BOOL const deleteKeySetResult = CryptAcquireContextW(&provider, kContainer, NULL, PROV_RSA_FULL, (CRYPT_DELETEKEYSET | CRYPT_SILENT));
			Assert(FALSE != deleteKeySetResult);
		}

		return hr;
	}

	HRESULT GetSubjectOrIssuer(CERT_NAME_BLOB const& blob, string& value)
	{
		HRESULT hr = E_FAIL;
		PBYTE buffer = NULL;

		// decode

		DWORD size;

		BOOL const decodeGetSizeResult = CryptDecodeObjectEx(X509_ASN_ENCODING, X509_NAME, blob.pbData, blob.cbData, CRYPT_DECODE_NOCOPY_FLAG, NULL, NULL, &size);
		Assert(FALSE != decodeGetSizeResult);

		if (FALSE == decodeGetSizeResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		buffer = new BYTE[size];

		BOOL const decodeResult = CryptDecodeObjectEx(X509_ASN_ENCODING, X509_NAME, blob.pbData, blob.cbData, CRYPT_DECODE_NOCOPY_FLAG, NULL, buffer, &size);
		Assert(FALSE != decodeResult);

		if (FALSE == decodeResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		CERT_NAME_INFO const& nameInfo = *reinterpret_cast<CERT_NAME_INFO const*>(buffer);

		// get name

		for (DWORD i = 0; (i < nameInfo.cRDN) && (E_FAIL == hr); ++i)
		{
			CERT_RDN const& rdn = nameInfo.rgRDN[i];

			for (DWORD j = 0; j < rdn.cRDNAttr; ++j)
			{
				CERT_RDN_ATTR const& rdnAttribute = rdn.rgRDNAttr[j];

				if (0 == strcmp(rdnAttribute.pszObjId, szOID_COMMON_NAME))
				{
					Assert((CERT_RDN_PRINTABLE_STRING == rdnAttribute.dwValueType) || (CERT_RDN_UNICODE_STRING == rdnAttribute.dwValueType));

					if (CERT_RDN_PRINTABLE_STRING == rdnAttribute.dwValueType)
					{
						LPCSTR name = reinterpret_cast<LPCSTR>(rdnAttribute.Value.pbData);
						value = name;
					}
					else if (CERT_RDN_UNICODE_STRING == rdnAttribute.dwValueType)
					{
						LPCWSTR name = reinterpret_cast<LPCWSTR>(rdnAttribute.Value.pbData);
						value = WideToUtf8(name);
					}
					else
					{
						goto cleanup;
					}

					// done

					hr = S_OK;
					break;
				}
			}
		}

		// cleanup

	cleanup:

		delete[] buffer;

		return hr;
	}

	HRESULT GetCertificateDetails(string const& path, string const& password, string& subject, string& issuer, SInt64& notAfter)
	{
		HRESULT hr = E_FAIL;
		HANDLE file = INVALID_HANDLE_VALUE;
		CRYPT_DATA_BLOB pfxBlob;
		HCERTSTORE store = NULL;
		PCCERT_CONTEXT certificate = NULL;

		pfxBlob.pbData = NULL;

		std::wstring pathWide;
		ConvertUnityPathName(path, pathWide);

		std::wstring passwordWide = Utf8ToWide(password);

		// open certificate file

		file = CreateFileW(pathWide.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		Assert(INVALID_HANDLE_VALUE != file);

		if (INVALID_HANDLE_VALUE == file)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// get file size

		LARGE_INTEGER fileSize;

		BOOL const getFileSizeResult = GetFileSizeEx(file, &fileSize);
		Assert(FALSE != getFileSizeResult);

		if (FALSE == getFileSizeResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		Assert(0 == fileSize.HighPart);

		if (0 != fileSize.HighPart)
		{
			goto cleanup;
		}

		// allocate memory for certificate data

		pfxBlob.cbData = fileSize.LowPart;
		pfxBlob.pbData = new BYTE[pfxBlob.cbData];

		// read certificate data

		DWORD read;

		BOOL const readFileResult = ReadFile(file, pfxBlob.pbData, pfxBlob.cbData, &read, NULL);
		Assert(FALSE != readFileResult);

		if (FALSE == readFileResult)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		Assert(read == pfxBlob.cbData);

		if (read != pfxBlob.cbData)
		{
			goto cleanup;
		}

		// import certificate

		store = PFXImportCertStore(&pfxBlob, passwordWide.c_str(), PKCS12_NO_PERSIST_KEY);

		if (NULL == store)
		{
			DWORD const error = GetLastError();
			Assert(ERROR_INVALID_PASSWORD == error);
			hr = HRESULT_FROM_WIN32(error);
			goto cleanup;
		}

		// there must be one certificate in the store

		certificate = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);
		Assert(NULL != certificate);

		if (NULL == certificate)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			goto cleanup;
		}

		// get subject

		HRESULT const getSubjectResult = GetSubjectOrIssuer(certificate->pCertInfo->Subject, subject);
		Assert(SUCCEEDED(getSubjectResult));

		if (FAILED(getSubjectResult))
		{
			hr = getSubjectResult;
			goto cleanup;
		}

		// get issuer

		HRESULT const getIssuerResult = GetSubjectOrIssuer(certificate->pCertInfo->Issuer, issuer);
		Assert(SUCCEEDED(getIssuerResult));

		if (FAILED(getIssuerResult))
		{
			hr = getIssuerResult;
			goto cleanup;
		}

		// get certificate expiration date

		notAfter = (static_cast<SInt64>(certificate->pCertInfo->NotAfter.dwLowDateTime) | (static_cast<SInt64>(certificate->pCertInfo->NotAfter.dwHighDateTime) << 32));

		// done

		hr = S_OK;

		// cleanup

	cleanup:

		if (NULL != certificate)
		{
			BOOL const freeCertificateContextResult = CertFreeCertificateContext(certificate);
			Assert(FALSE != freeCertificateContextResult);
		}

		if (NULL != store)
		{
			BOOL const closeStoreResult = CertCloseStore(store, 0);
			Assert(FALSE != closeStoreResult);
		}

		delete[] pfxBlob.pbData;

		if (INVALID_HANDLE_VALUE != file)
		{
			BOOL const closeFileResult = CloseHandle(file);
			Assert(FALSE != closeFileResult);
		}

		return hr;
	}
}

MetroCreateTestCertificateResult MetroCreateTestCertificate(string const& path, string const& publisher, string const& password, bool overwrite)
{
	HRESULT const hr = MetroCreateTestCertificateInternal(path, publisher, password, overwrite);

	if (SUCCEEDED(hr))
	{
		return kMetroCreateTestCertificateResult_Succeeded;
	}

	if (!overwrite)
	{
		Assert(hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));

		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS))
		{
			return kMetroCreateTestCertificateResult_FileExists;
		}
	}

	return kMetroCreateTestCertificateResult_Failed;
}

MetroSetCertificateResult MetroSetCertificate(string const& path, string const& password)
{
	EditorOnlyPlayerSettings& settings = GetPlayerSettings().GetEditorOnlyForUpdate();

	if (path.empty())
	{
		settings.metroCertificatePath.clear();
		settings.metroCertificatePassword.clear();
		settings.metroCertificateSubject.clear();
		settings.metroCertificateIssuer.clear();
		settings.metroCertificateNotAfter = 0;

		return kMetroSetCertificateResult_Succeeded;
	}

	HRESULT const hr = GetCertificateDetails(path, password, settings.metroCertificateSubject, settings.metroCertificateIssuer, settings.metroCertificateNotAfter);
	Assert(SUCCEEDED(hr) || (hr == HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD)));

	if (SUCCEEDED(hr))
	{
		settings.metroCertificatePath = path;
		settings.metroCertificatePassword = InsecureScramblePassword(password, path);
		return kMetroSetCertificateResult_Succeeded;
	}

	settings.metroCertificatePath.clear();
	settings.metroCertificatePassword.clear();
	settings.metroCertificateSubject.clear();
	settings.metroCertificateIssuer.clear();
	settings.metroCertificateNotAfter = 0;

	return ((hr == HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD)) ? kMetroSetCertificateResult_InvalidPassword : kMetroSetCertificateResult_Failed);
}

#endif
