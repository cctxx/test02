#ifndef CURL_REQUEST_H
#define CURL_REQUEST_H


class CurlRequest;
class CurlHandle; // Used to track individual requests
class CurlCacheResults;

class CurlRequestMessage
{
	public:
	CurlRequestMessage() : m_Tag(""), m_Method("GET"), m_PostData(""), m_FailOnError(1), m_ConnectTimeout(5), m_ResponseCode(0), m_ShouldAbortOnExit(false), m_AllowCaching(false), m_ShouldAbort (0), m_Cache(NULL) {}
	virtual ~CurlRequestMessage();
	virtual void Done() {} // Called on the main thread when the request is done
	virtual void Progress(size_t /*fetched*/, size_t /*total*/) {} // Called on the main thread to inform of download progress
	virtual void Connecting() {} // Called on the main thread to inform that the connection to the server is being initiated

	std::string m_Tag; // Set this tag to a value if you need to be able to cancel a queued request later
	std::string m_Uri;
	std::string m_Method;
	std::string m_PostData;
	std::vector<std::string> m_Headers;

    std::vector<std::string> m_ResponseHeaders;
    std::string m_ResponseString;
    int m_FailOnError;
	int m_ConnectTimeout;

	std::string m_UploadPathName; // if non-empty and m_Method is neither "HEAD" or "GET", the contents of the file pointed to will be sent to the Uri as the request body. Remember to set m_Method to either "PUT" or "POST" as required.
	std::string m_PathName; // if non-empty, the resonse body will be saved into the path pointed to by this string

	std::string m_Result; // Response string if m_Success == true (and m_DestFilename is not empty) otherwise string describing the error
	int m_ResponseCode; // HTTP response code (such as 200 for OK and 500 for internal server error, etc.)
	bool m_Success;
	// Should only be set at construction time before adding to the queue
	bool m_ShouldAbortOnExit;
	bool m_AllowCaching;

	void Abort();

	private:
	// The private variables should only be modified while holding the queue lock
	enum State {
		kStateNew = 0,
		kStateRunning,
		kStateConnecting,
		kStateProgress,
		kStateDone
	};
	State m_RequestState;
	size_t m_BytesFetched;
	size_t m_BytesTotal;
	int m_ShouldAbort;	// implemented as int so we can increment it atomically
    CurlCacheResults* m_Cache;

	friend class CurlRequest;
	friend class CurlHandle;
};

enum CurlRequestGroup {
	kCurlRequestGroupSingle	= 0,
	kCurlRequestGroupMulti = 1,
	kCurlRequestGroupAsyncHTTPClient = 2,
	kCurlRequestGroupCount = 3
};

// An asynchronous HTTP request can be submitted into one of two request groups.
// - kCurlRequestGroupSingle should be used for requests that are not supposed to run in parallel, such as when submitting analytics.
//	Requests will be queued up until there is an available slot and thus the order delivery will be in the same order the requests are added to
//	the queue.
// - kCurlRequestGroupMulti supports up to 10 parallel requests at the same time.
//	Use this for long running requests such as asset store downloads or requests where you don't need to
//	guarantee the message is delivered in any specific order related to other requests.
// - kCurlRequestGroupAsyncHTTPClient supports up to 20 parallel requests at the same time.
//	Should only be used by the AsyncHTTPClient implementation.
// Async HTTP GET request
void CurlRequestGet(CurlRequestMessage *pMessage, int group = kCurlRequestGroupSingle);

// Calles on the main thread to check for completed requests
void CurlRequestCheck();

// Returns true if the requests has failed N times
bool CurlRequestFailed( int group = kCurlRequestGroupSingle );

// Initialize the curl thread
void CurlRequestInitialize();

// Cleanup the curl thread
void CurlRequestCleanup();

// Removes all requests tagged with "tag" from the pending queue
void CurlRequestAbortTag(const std::string& tag, int group = kCurlRequestGroupSingle );

// Removes the current request from the pending queue
void CurlRequestAbort(CurlRequestMessage *pMessage, int group = kCurlRequestGroupSingle );

// Url encode/decode a string using curl
std::string CurlUrlEncode (const std::string &s);
std::string CurlUrlDecode (const std::string &s);

#endif
