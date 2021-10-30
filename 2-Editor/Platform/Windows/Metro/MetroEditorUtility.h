#ifndef METROEDITORUTILITY_H
#define METROEDITORUTILITY_H

enum MetroCreateTestCertificateResult
{
	kMetroCreateTestCertificateResult_Succeeded,
	kMetroCreateTestCertificateResult_Failed,
	kMetroCreateTestCertificateResult_FileExists
};

MetroCreateTestCertificateResult MetroCreateTestCertificate(std::string const& path, std::string const& publisher, std::string const& password, bool overwrite);

enum MetroSetCertificateResult
{
	kMetroSetCertificateResult_Succeeded,
	kMetroSetCertificateResult_Failed,
	kMetroSetCertificateResult_InvalidPassword
};

MetroSetCertificateResult MetroSetCertificate(std::string const& path, std::string const& password);

#endif