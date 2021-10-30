#pragma once
/*
 *  CurlFileCache.h
 *
 */
class SQLite;
class CurlFileCache;

class CurlCacheResults 
{
	SInt32 m_ResponseID;
	SInt32 m_TimeStamp;
	SInt32 m_MaxAge;
	bool m_AllowCache;
	std::string m_Url;
	
	std::vector<std::string> m_ValidationHeaders;
	std::vector<std::string> m_ResponseHeaders;
	
	int GetAge();
	
public:
	CurlCacheResults(const std::string& url);
	
	bool IsCached() { return m_ResponseID >= 0; }
	void ClearResponseHeaders();
	void AddResponseHeader(const std::string& header, bool max_age_is_known=false);
	void SetResponseHeaders(const std::vector<std::string>& headers);
	std::pair<std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator> GetResponseHeaders();
	std::pair<std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator> GetValidationHeaders();
	bool ShouldValidate() { return GetAge() > m_MaxAge; }
	bool Fetch();
	void Store(const std::string& response);
	void Touch();
	void Remove();
	
	std::string GetResponseData();
	
	friend class CurlFileCache;
};

class CurlFileCache {

private:
	SQLite* m_Db;
	static CurlFileCache* s_SingletonInstance;
	CurlFileCache();
	~CurlFileCache();
	int GetSize();
	
public:
	void Cleanup();
	void Clear();
	bool Fetch(CurlCacheResults* res);
	void Store(CurlCacheResults* res, const std::string& response);
	void Touch(CurlCacheResults* res);
	void Remove(CurlCacheResults* res);
	
	std::string GetResponseData(CurlCacheResults* res);
	static CurlFileCache& Instance();

};

void CurlFileCacheCleanup();