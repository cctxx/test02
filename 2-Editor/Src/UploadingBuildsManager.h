#pragma once
#include "Editor/Src/Utility/CurlRequest.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include <string>


class UploadingBuild
{
public:
	enum Status
	{
		kAuthorizing = 0,
		kAuthorized,
		kUploading,
		kUploaded, 
		kUploadFailed
	}; // Ensure in-sync with enum 'UploadBuildStatus' (in UploadingBuildsUtility.txt)
	enum OverwriteHandling
	{
		kPrompt = 0,
		kOverwrite,
		kVersion,
		kCancel
	}; // Ensure in-sunc with enum 'OverwriteHandling' (int UploadingBuildsUtility.txt)


	UploadingBuild (Status buildStatus, OverwriteHandling overwriteHandling);
	
	void Upload ();
	void UploadPlayer (bool useNewToken = false);
	float GetUploadProgress ();
	void UploadFailure (const std::string& errorMessage);
	void UploadSuccess ();
	void Unauthorized (const std::string& errorMessage, bool recoverable);
	void BuildExists (const std::string& uploadToken, const std::string& newUploadToken);
	void Authorized (const std::string& uploadToken);


	Status m_Status;
	OverwriteHandling m_OverwriteHandling;
	std::string m_SessionID;
	std::string m_DisplayName;
	std::string m_URL;
	std::string m_PlayerPathBeingUploaded;
	std::string m_UploadToken;
	std::string m_NewUploadToken;
	size_t m_BytesSent;
	size_t m_TotalSize;
	bool m_RunWhenUploaded;
};


class UploadingBuildsManager 
{
public:
	// Async upload of a build
	void BeginUploadBuild (const std::string& buildPath, const std::string& displayName, UploadingBuild::OverwriteHandling overwriteHandling, bool autoRun = false);
	void BeginUploadBuild (const std::string& buildPath, bool autoRun = false);
	bool ResumeBuildUpload (const std::string& displayName, bool replace = true);
	void EndUploadBuild ();

	// Get all UploadingBuilds
	const std::vector<UploadingBuild*>& GetUploadingBuilds ();
	
	// Stop tracking the UploadingBuild identified by the given display name
	void RemoveUploadingBuild (const std::string& displayName);
	
	// Check if the user is logged into UDN properly and complain if not
	bool VerifyLogin ();
	bool VerifyLogin (const std::string &displayName);
	bool ValidateSession ();

private:
	void InternalStateChanged ();
	
	std::string m_SessionID;
	std::vector<UploadingBuild*> m_UploadingBuilds;
};

UploadingBuildsManager& GetUploadingBuildsManager ();