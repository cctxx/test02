#include "UnityPrefix.h"

#include "CurlRequest.h"
#include "Editor/Src/Utility/CurlFileCache.h"
#include <curl/curl.h>
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Configuration/UnityConfigureRevision.h"
#include <list>
#include <set>

// Defined in Runtime/Export/WWW.h, but we can't include that since it imports minimalcurl.h
// Todo: change WWW.h to use the official curl/curl.h header file instead
void SetupCurlProxyServerBasedOnEnvironmentVariable(CURL* curl, const char *url);

#include "Runtime/Utilities/File.h"
#if UNITY_WIN
#	include <Winsock2.h>
#else
#	include <sys/select.h>
#endif

using namespace std;

// Sample string: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; en-US; rv:1.9.1.2) Gecko/20090729 Firefox/3.5.2 GTB6
static string GetUserAgentString()
{
	int platform = systeminfo::GetRuntimePlatform();
	string system = platform == WindowsEditor ? "Windows" : (platform == OSXEditor ? "Macintosh" : (platform == LinuxEditor ? "Linux" : ""));
	Assert(!system.empty());
	
	// Convert to numeric version: alpha = .0x, beta = .1x, RC = .2x
	string version = UNITY_VERSION_WIN;
	int osNumeric = systeminfo::GetOperatingSystemNumeric();
	string os;
	if ( system == "Windows" )
	{
		if ( systeminfo::GetOperatingSystem() == "Windows 9x" ) 
			os = "Windows 98";
		else
			os = Format("Windows NT %d.%d", osNumeric/100, (osNumeric%100)/10);
	}
	else
	{
		os = Format("Intel Mac OS X %d.%d", osNumeric/100, (osNumeric%100)/10);
	}
	
	
	string language = systeminfo::GetSystemLanguageISO();
	
	string userAgentString = Format("UnityEditor/%s (%s; U; %s; %s)", version.c_str(), system.c_str(), os.c_str(), language.c_str()); 	
	return userAgentString;
}

#define CHECK_outRes(x) outRes = (x); if(outRes != CURLE_OK) return;
class CurlHandle
{
public:


	CurlHandle(CurlRequestMessage *pMessage, string userAgent, CURLcode& outRes, int group) 
		: m_Curl(NULL)
		, m_Headers(NULL)
        , m_ResponseHeaders()
		, m_Message(pMessage)
		, m_Group(group)
		, m_IsWritingToFile(false)
	{
		
		m_Curl = curl_easy_init();
				
		if ( m_Curl ) 
		{
			s_ActiveHandles[m_Curl]=this;
			//printf_console("Allocating curl handle 0x%08x for URL: %s\n", m_Curl, m_Message->m_Uri.c_str());
			
			outRes = curl_easy_setopt(m_Curl, CURLOPT_ERRORBUFFER, m_CurlErrorBuffer);
			if ( outRes != CURLE_OK )
			{
				SetErrorMessage(curl_easy_strerror(outRes));
				return;
			}

			m_ResponseString = "";
			m_Message->m_RequestState = CurlRequestMessage::kStateRunning;
			
			CHECK_outRes( curl_easy_setopt(m_Curl, CURLOPT_URL, m_Message->m_Uri.c_str()));
			CHECK_outRes( curl_easy_setopt(m_Curl, CURLOPT_USERAGENT, userAgent.c_str()) ); // "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; en-US; rv:1.9.1.2) Gecko/20090729 Firefox/3.5.2 GTB6"
			// Don't verify SSL certificates, as we don't have the SSL root CA keys anywhere
			CHECK_outRes( curl_easy_setopt(m_Curl, CURLOPT_SSL_VERIFYPEER, 0));

			// Would be nice if we could just give the method name to Curl
			
			if ( m_Message->m_Method == "HEAD" )
			{
				CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_NOBODY, 1));
				
			}
			else if ( m_Message->m_Method != "GET" ) // GET is the default
 			{
				string expectedMethod = "";
				if(m_Message->m_PostData != "") {
					expectedMethod = "POST";
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_POST, 1));
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_POSTFIELDS, m_Message->m_PostData.c_str() ));
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_POSTFIELDSIZE, m_Message->m_PostData.size() ));
				}
				else if( m_Message->m_UploadPathName != "" )
				{
					expectedMethod = "PUT";
					if (! m_ReadHandle.Open(m_Message->m_UploadPathName, File::kReadPermission, File::kSilentReturnOnOpenFail|File::kRetryOnOpenFail)) 
					{
						strncpy(m_CurlErrorBuffer, Format("Could not open %s for reading.", m_Message->m_UploadPathName.c_str()).c_str(), CURL_ERROR_SIZE);
						outRes=CURLE_READ_ERROR;
						return;
					}
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_UPLOAD, 1));
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_READDATA, this));
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_READFUNCTION, ReadFileCallback));
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_INFILESIZE, GetFileLength(m_Message->m_UploadPathName)));
				}
				
				// In case the user uploading a file using some weird method 
				if ( m_Message->m_Method != expectedMethod )
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_CUSTOMREQUEST, m_Message->m_Method.c_str() ));

			}

			m_Headers = NULL;
			if (m_Message->m_Cache && m_Message->m_Cache->ShouldValidate() ) 
			{
				pair<vector<string>::const_iterator,vector<string>::const_iterator> validation = m_Message->m_Cache->GetValidationHeaders();
				for( vector<string>::const_iterator i = validation.first; i != validation.second; ++i)
				{	
					m_Headers = curl_slist_append(m_Headers, i->c_str());
				}
			}
			for ( vector<string>::const_iterator i = m_Message->m_Headers.begin(); i != m_Message->m_Headers.end(); ++i ) 
			{
				m_Headers = curl_slist_append(m_Headers, i->c_str());  
			}
			
			// Also sets CURLOPT_HTTPHEADER to NULL if there are no custom headers
			CHECK_outRes( curl_easy_setopt(m_Curl, CURLOPT_HTTPHEADER, m_Headers));
			if ( m_Message->m_PathName.empty() )
			{
				m_IsWritingToFile = false;
				CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback));
			}
			else 
			{
				if ( IsFileCreated(m_Message->m_PathName) ) // Attempt resume ¨C it's the Message subclass' responsibility to delete any existing files prior if resume is not desired
				{  
					int fileLength = GetFileLength(m_Message->m_PathName);
					
					if (! m_WriteHandle.Open(m_Message->m_PathName, File::kAppendPermission)) 
					{
						strncpy(m_CurlErrorBuffer, Format("Could not open %s for appending.", m_Message->m_PathName.c_str()).c_str(), CURL_ERROR_SIZE);
						outRes=CURLE_WRITE_ERROR;

						return;
					}
					CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_RESUME_FROM, fileLength));
				}
				else
				{
	
					if (! m_WriteHandle.Open(m_Message->m_PathName, File::kWritePermission)) 
					{
						strncpy(m_CurlErrorBuffer, Format("Could not open %s for writing.", m_Message->m_PathName.c_str()).c_str(), CURL_ERROR_SIZE);
						outRes=CURLE_WRITE_ERROR;
						return;
					}
				
					
				}
				m_IsWritingToFile = true;
				CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, WriteFileCallback));

			}


			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, this));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_NOPROGRESS, 0));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_PROGRESSDATA, this));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_HEADERFUNCTION, HeaderCallback));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_HEADERDATA, this));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_FAILONERROR, m_Message->m_FailOnError));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_CONNECTTIMEOUT, m_Message->m_ConnectTimeout));
			CHECK_outRes(curl_easy_setopt(m_Curl, CURLOPT_NOSIGNAL, 1)); // Needs to be set when running CURL in multible threads
			SetupCurlProxyServerBasedOnEnvironmentVariable(m_Curl, m_Message->m_Uri.c_str());


		}
		else 
		{
			strcpy(m_CurlErrorBuffer, "Internal Error: Could not initialize CURL handle");
			outRes = CURLE_FAILED_INIT;
		}

		
	}
	
	~CurlHandle() 
	{
		if ( m_Headers )  
			curl_slist_free_all(m_Headers);

		if ( m_Curl )
		{
			s_ActiveHandles.erase(m_Curl);
			curl_easy_cleanup(m_Curl);
		}
		
	}
    	
	static CurlHandle* GetHandleFromInternalHandle( CURL* internal )
	{
		std::map<CURL*, CurlHandle*>::iterator found = s_ActiveHandles.find(internal);
		if ( found != s_ActiveHandles.end() )
			return found->second;
		else
			return NULL;
	}

	void CompleteTransfer(CURLcode res);
	bool CompleteTransferFromCache();
	CURL* GetInternalHandle() const
	{
		return m_Curl;
	}
	
	int GetGroup() const
	{
		return m_Group;
	}
	
	void SetErrorMessage(const char* c)
	{
		strncpy(m_CurlErrorBuffer, c, CURL_ERROR_SIZE);
	}
	
	string GetErrorMessage()
	{
		return string(m_CurlErrorBuffer, CURL_ERROR_SIZE);
	}

private:


	static size_t WriteMemoryCallback(void *data, size_t size, size_t elements, void *stream);
	static size_t WriteFileCallback(void *data, size_t size, size_t elements, void *stream);
	static size_t ReadFileCallback(void *data, size_t size, size_t elements, void *stream);
	static size_t HeaderCallback(void *data, size_t size, size_t elements, void *stream);
	static int ProgressCallback(void *clientp, double dltotal, double dlnow,  double ultotal, double ulnow);

	CurlRequestMessage * m_Message;
	CURL* m_Curl;
	int m_Group;
	
	std::string m_ResponseString;
	vector<std::string> m_ResponseHeaders;
	char m_CurlErrorBuffer[CURL_ERROR_SIZE];
	File m_WriteHandle;
	File m_ReadHandle;

	bool m_IsWritingToFile;
	struct curl_slist * m_Headers;
	static std::map<CURL*, CurlHandle*> s_ActiveHandles;

};

std::map<CURL*, CurlHandle*> CurlHandle::s_ActiveHandles;


class CurlRequest
{
public:
	CurlRequest()
		: groupRequestsMax(kCurlRequestGroupCount)
		, groupRequestsInProgress(kCurlRequestGroupCount, 0)
		, groupFailedCount(kCurlRequestGroupCount, 0)
		, groupMaxFailedCount(kCurlRequestGroupCount)
		, groupMaxDonePerTick(kCurlRequestGroupCount, 0)
	{
		userAgent = GetUserAgentString();
		thread.Run(_ThreadEntryPoint, this);
		groupRequestsMax[kCurlRequestGroupSingle]=1;
		groupRequestsMax[kCurlRequestGroupMulti]=10;
		groupRequestsMax[kCurlRequestGroupAsyncHTTPClient]=20;
		groupMaxFailedCount[kCurlRequestGroupSingle]=5;
		groupMaxFailedCount[kCurlRequestGroupMulti]=0;
		groupMaxFailedCount[kCurlRequestGroupAsyncHTTPClient]=0;
		groupMaxDonePerTick[kCurlRequestGroupAsyncHTTPClient]=10;
	}
	
	~CurlRequest()
	{
		thread.WaitForExit();
		Check(); // Deliver the final notifications
	}
	
	void AbortTag(const string& tag, int group)
	{
		CurlMessageTempList shouldBeMarkedAsDone;
		
		mutex.Lock();
		// First mark running requests as aborted
		for( CurlMessageSet::iterator i = activeRequests[group].begin(); i != activeRequests[group].end(); ++i )
		{
			if ((*i)->m_Tag == tag)
				(*i)->Abort(); // These will be reported as aborted next time curl multi invokes a callback function
		}
		// Then remove all matching pending requests immediately
		for( CurlMessageList::iterator i = newMessageQueue[group].begin(); i != newMessageQueue[group].end();  )
		{
			CurlMessageList::iterator curr = i++; // move to next before checking so that we can remove items while iterating
			if ((*curr)->m_Tag == tag) {
				(*curr)->Abort();
				shouldBeMarkedAsDone.push_back(*curr);
				newMessageQueue[group].erase(curr);
			}
		}

		// Finally remove all done requests that matches
		for( CurlMessageList::iterator i = eventMessageQueue[group].begin(); i != eventMessageQueue[group].end(); ++i )
		{
			if ((*i)->m_Tag == tag)
				(*i)->Abort(); // These will be reported as aborted next time curl multi invokes a callback function
		}

		mutex.Unlock();
				
		// Post a Done message for all the removed requests
		for( CurlMessageTempList::iterator i = shouldBeMarkedAsDone.begin(); i != shouldBeMarkedAsDone.end(); ++i )
		{
			(*i)->m_Result = "Aborted Request";
			(*i)->m_Success = false;
			(*i)->m_ResponseCode = 299;
			PushDone(*i, group);
		}
		
	}

	void AbortRequestMessage(CurlRequestMessage* pMessage, int group)
	{
		
		mutex.Lock();

		// First search through running requests
		for( CurlMessageSet::iterator i = activeRequests[group].begin(); i != activeRequests[group].end(); ++i )
		{
			if ((*i) == pMessage) {
				(*i)->Abort(); // The request will be reported as aborted next time curl multi invokes a callback function
				
				mutex.Unlock();
				return;

			}
		}
		
		// Then loop through pending requests
		for( CurlMessageList::iterator i = newMessageQueue[group].begin(); i != newMessageQueue[group].end(); ++i )
		{
			if (*i == pMessage) {
				// Since we removed  the request from the queue, we will have to post the Done message ourselves
				newMessageQueue[group].erase(i);
				pMessage->Abort();
				pMessage->m_Result = "Aborted Request";
				pMessage->m_Success = false;
				pMessage->m_ResponseCode = 299;
				mutex.Unlock(); // unlock mutex before we call PushDone

				PushDone(pMessage, group);

				return;
			}
		}
		
		// Finally remove all done requests that matches
		for( CurlMessageList::iterator i = eventMessageQueue[group].begin(); i != eventMessageQueue[group].end(); ++i )
		{
			if ((*i) == pMessage) {
				(*i)->Abort(); // The request will be reported as aborted next time curl multi invokes a callback function
				
				mutex.Unlock();
				return;
				
			}
		}

		mutex.Unlock();
	}
	
	void PushNew(CurlRequestMessage *pMessage, int group )
	{
        // Set up caching
		if ( pMessage->m_AllowCaching && pMessage->m_Method == "GET" && pMessage->m_PathName.empty() ) {
			pMessage->m_Cache= UNITY_NEW(CurlCacheResults(pMessage->m_Uri), kMemCurl);
			pMessage->m_Cache->Fetch();
			// We have (potentially) stale content, so we'll have to check with the server if its still valid
            // Otherwise return immediately as we will return the results directly from the cache
            if (pMessage->m_Cache->IsCached() && ! pMessage->m_Cache->ShouldValidate() ) 
            {
                PushDoneFromCache(pMessage, group);
                return;
            }
		}
        
        mutex.Lock();
		newMessageQueue[group].push_back( pMessage );
		mutex.Unlock();
	}
	
	CurlRequestMessage * PopNew(int group)
	{
		CurlRequestMessage * entry = NULL;
		mutex.Lock();
		if ( !newMessageQueue[group].empty() ) 
		{
			entry = newMessageQueue[group].front();
			newMessageQueue[group].pop_front();
			activeRequests[group].insert( entry );
		}
		mutex.Unlock();
		return entry;
	}
    
    void PushDoneFromCache(CurlRequestMessage* pMessage, int group)
    {
        Assert( pMessage->m_Cache != NULL && pMessage->m_Cache->IsCached() );
        
        pMessage->m_Result = pMessage->m_Cache->GetResponseData();
        pMessage->m_ResponseCode = 200;
        pMessage->m_Success=true;
        pMessage->m_BytesFetched = pMessage->m_BytesTotal = pMessage->m_Result.size();
        
		mutex.Lock();
		pMessage->m_RequestState = CurlRequestMessage::kStateDone;
        eventMessageQueue[group].push_back(pMessage);
		mutex.Unlock();
    }
	
	void PushDone(CurlRequestMessage *pMessage, int group)
	{
		
		mutex.Lock();
		CurlRequestMessage::State oldState = pMessage->m_RequestState;
		pMessage->m_RequestState = CurlRequestMessage::kStateDone;
		groupRequestsInProgress[group]--;
		activeRequests[group].erase( pMessage );
		
		// Only need to add the message to the queue if the state indicates that the message is not already in the queue
		if ( oldState <= CurlRequestMessage::kStateRunning )
			eventMessageQueue[group].push_back(pMessage);
		mutex.Unlock();
	}
	
	bool PushProgress(CurlRequestMessage *pMessage, int group, size_t bytesFetched, size_t bytesTotal)
	{
		
		mutex.Lock();
		CurlRequestMessage::State oldState = pMessage->m_RequestState;
		pMessage->m_BytesFetched = bytesFetched;
		if (bytesTotal > 0)
			pMessage->m_BytesTotal = bytesTotal;

		// Don't change the state if we're already done
		if ( oldState < CurlRequestMessage::kStateProgress )
			pMessage->m_RequestState = CurlRequestMessage::kStateProgress;
		
		// Only need to add the message to the queue if the state indicates that the message is not already in the queue
		if ( oldState <= CurlRequestMessage::kStateRunning )
			eventMessageQueue[group].push_back(pMessage);

		mutex.Unlock();
		return pMessage->m_ShouldAbort || pMessage->m_ShouldAbortOnExit && thread.IsQuitSignaled();
	}
	
	void PushConnecting(CurlRequestMessage *pMessage, int group)
	{
		mutex.Lock();
		// Only need to add the message to the queue if the state indicates that the message is not already in the queue
		if( pMessage->m_RequestState <= CurlRequestMessage::kStateRunning )
		{
			pMessage->m_RequestState = CurlRequestMessage::kStateConnecting;
			eventMessageQueue[group].push_back(pMessage);
		}
		
		mutex.Unlock();
	}
	
	CurlRequestMessage *PopEvent(int group, CurlRequestMessage::State &outState, int &outFetched, int &outTotal)
	{
		CurlRequestMessage *pMessage = NULL;
		mutex.Lock();
		if ( !eventMessageQueue[group].empty() ) 
		{
			pMessage = eventMessageQueue[group].front();
			outState = pMessage->m_RequestState; // Copy message state while we have a lock
			outFetched = pMessage->m_BytesFetched;
			outTotal = pMessage->m_BytesTotal;
			pMessage->m_RequestState=CurlRequestMessage::kStateRunning; // and reset it to indicate that the current state has been received
			eventMessageQueue[group].pop_front();
		}
		mutex.Unlock();
		return pMessage;
	}   
	
	
	static void *_ThreadEntryPoint(void *p)
	{
		return (void*)static_cast<CurlRequest *>(p)->MessageThread(); // Chain into a non-static method makes the main loop look much nicer.
	}
	
	int MessageThread()
	{		
		CURLM * multiHandle = curl_multi_init( );
		
		int runningRequests = 0;
		while ( true ) 
		{
			// Setup timeout values and filedescriptor sets
			struct timeval timeout;
			long curlTimeout = -1;
			int rc; /* select() return code */ 

			fd_set fdread;
			fd_set fdwrite;
			fd_set fdexcep;
			int maxfd = -1;

			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);


			// Check if there are new requests to add to the queue
			int newRequests = 0;
			for (int group = 0;  group < groupRequestsMax.size(); ++group )
			{
			
				while ( groupRequestsInProgress[group] < groupRequestsMax[group] )
				{
					CurlRequestMessage *pMessage = PopNew(group);
					if (! pMessage ) // If queue is empty, exit the inner loop
						break;
					
					++groupRequestsInProgress[group]; // Keep tally of running requests in each group

					// Abort the request without trying in case we are exiting and we allow aborting the current request
					// or if the request was canceled before we even started
					if ( pMessage->m_ShouldAbort || pMessage->m_ShouldAbortOnExit && thread.IsQuitSignaled() )
					{
						// We don't use pHandle->CompleteTransfer as the internal curl handle has not been initialized
						pMessage->m_Result = "Aborted Request";
						pMessage->m_Success = false;
						pMessage->m_ResponseCode = 299;
						PushDone(pMessage, group);
						continue;
					}
					

					CURLcode res = CURLE_OK;
					CURLMcode mres = CURLM_OK;
					CurlHandle* pHandle = UNITY_NEW(CurlHandle(pMessage, userAgent, res, group), kMemCurl);
					if ( res == CURLE_OK )
					{
						mres = curl_multi_add_handle(multiHandle, pHandle->GetInternalHandle());
						if ( mres != CURLM_OK)
						{	
							pHandle->SetErrorMessage(curl_multi_strerror(mres));
							AtomicIncrement(&(groupFailedCount[group]));
							pHandle->CompleteTransfer(CURLE_FAILED_INIT);
						}
						else
						{
							PushConnecting(pMessage, group);
							++newRequests;
						}

					}
					else
					{	// Request (or initialization) failed
						AtomicIncrement(&(groupFailedCount[group]));
						pHandle->CompleteTransfer(res);
					}
				}
			}
			
			// Call multi_perform a few times if we added new requests to the pool
			while(newRequests && CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multiHandle, &runningRequests))
			{}
			
			// We wait one second by default
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			
			// Ask Curl if it wants a shorter timeout than the default 1 second
			curl_multi_timeout(multiHandle, &curlTimeout);
			if(curlTimeout >= 0)
			{
				timeout.tv_sec = curlTimeout / 1000;
				if(timeout.tv_sec > 1)
					timeout.tv_sec = 1;
				else
					timeout.tv_usec = (curlTimeout % 1000) * 1000;
			}
			
			// Get filedescriptors to pass to select
			curl_multi_fdset(multiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);

			// And then wait
			if ( maxfd == -1 )
			{	// In case of no active handles, just sleep for half a second
				// NOTE: we do not simply call select(0, ...., &timeout) and let that handle the sleep,
				// as on Win32, select will not sleep but return with an error causing a busy loop.
				rc=0;
				Thread::Sleep(0.5F);
			}
			else
				rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

			// Handle select return value
			switch(rc) {
				case -1:
					// TODO: handle error here 
				break;
				case 0:
				default:
					while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multiHandle, &runningRequests))
					{
					}
				break;
			}
			
			// Clean up finished transfers
			int messagesLeft = -1;
			CURLMsg* info;
			while ( (info=curl_multi_info_read( multiHandle, &messagesLeft) ) != NULL )
			{
				if (info->msg == CURLMSG_DONE) // Currently, curl_multi_info_read will only return CURLMSG_DONE.
				{
					//printf_console("Got CURLMSG_DONE for curl handle 0x%08x, status '%s'\n", info->easy_handle, curl_easy_strerror(info->data.result) );
					
					
					CurlHandle* pHandle = CurlHandle::GetHandleFromInternalHandle(info->easy_handle);
					CURLcode result = info->data.result;
				
					curl_multi_remove_handle(multiHandle, info->easy_handle);
					info = NULL; // Defensive coding: info becomes garbage after calling curl_multi_remove_handle

					AssertIf(pHandle == NULL);
					if ( pHandle != NULL)
					{
						if ( result != CURLE_OK && result != CURLE_ABORTED_BY_CALLBACK && pHandle->GetErrorMessage() != "Callback aborted")
							AtomicIncrement(&(groupFailedCount[pHandle->GetGroup()]));
						
						pHandle->CompleteTransfer(result);
					}
				}
				else {
					printf_console("Unexpected message id %02x returned from curl_multi_info_read\n",info->msg);
				}
			}
			
			if (runningRequests == 0 && thread.IsQuitSignaled())
				break;
		}
		
		curl_multi_cleanup( multiHandle );
		
		return 0;
	}
	
	static bool Failed(int group)
	{
		if ( pCurlRequest ) 
		{
			return pCurlRequest->groupMaxFailedCount[group]>0 && pCurlRequest->groupFailedCount[group] >= pCurlRequest->groupMaxFailedCount[group];
		}
		return false;
	}
	
	static CurlRequest &Instance()
	{
		if ( pCurlRequest == NULL ) 
		{
			pCurlRequest = UNITY_NEW(CurlRequest(),kMemCurl);
		}
		return *pCurlRequest;
	}
	
	static void Cleanup()
	{
		if ( pCurlRequest != NULL ) 
		{
			UNITY_DELETE( pCurlRequest, kMemCurl);
			pCurlRequest = NULL;
		}
	}
	
	void Check()
	{
		CurlRequestMessage *pMessage = NULL;
		CurlRequestMessage::State currentState;
		int fetched, total;
		for (int group = 0; group < kCurlRequestGroupCount; ++group)
		{
			int doneCount = 0;
			int maxDone = groupMaxDonePerTick[group];
			while ( (pMessage = PopEvent(group, currentState, fetched, total )) != NULL ) 
			{
				switch ( currentState )
				{
					case CurlRequestMessage::kStateConnecting:
						pMessage->Connecting();
						break;
					case CurlRequestMessage::kStateProgress:
						pMessage->Progress(pMessage->m_BytesFetched, pMessage->m_BytesTotal);
						break;
					case CurlRequestMessage::kStateDone:
						pMessage->Progress(pMessage->m_BytesFetched, pMessage->m_BytesTotal);
						if (pMessage->m_ShouldAbort) {
							pMessage->m_Result="Aborted Request";
							pMessage->m_Success=false;
							pMessage->m_ResponseCode=299;
						}
						pMessage->Done();
						if ( pMessage->m_Success ) doneCount++; // Don't count unsuccessful requests toward the max done per tick counter
						UNITY_DELETE( pMessage,kMemCurl);
						break;
					default:
						FatalErrorString ("Unexpected CurlRequestMessage state.");
						break;
				}
				if ( maxDone > 0 && doneCount >= maxDone )
					break;
			}
		}
	}
	
private:
	Thread thread;
	typedef UNITY_LIST(kMemCurl, CurlRequestMessage *) CurlMessageList;
	typedef UNITY_SET(kMemCurl, CurlRequestMessage *) CurlMessageSet;
	typedef UNITY_LIST(kMemTempAlloc, CurlRequestMessage *) CurlMessageTempList;
	CurlMessageList newMessageQueue[kCurlRequestGroupCount]; // there's one queue per request group
	CurlMessageList eventMessageQueue[kCurlRequestGroupCount];
	CurlMessageSet activeRequests[kCurlRequestGroupCount];
	UNITY_VECTOR(kMemCurl,int) groupRequestsMax;
	UNITY_VECTOR(kMemCurl,int) groupRequestsInProgress;
	UNITY_VECTOR(kMemCurl,int) groupFailedCount;
	UNITY_VECTOR(kMemCurl,int) groupMaxFailedCount;
	UNITY_VECTOR(kMemCurl,int) groupMaxDonePerTick;
	Mutex mutex; // Lock this mutex before accessing newMessageQueue, eventMessageQueue or activeRequests
	
	int runningRequests ;
								   
	UNITY_MAP(kMemCurl,CURL*,CurlHandle*) m_InProgress;
	
	string userAgent;
	static CurlRequest* pCurlRequest;
};

CurlRequest *CurlRequest::pCurlRequest = NULL;

void CurlHandle::CompleteTransfer(CURLcode res)
{
	if ( !m_Message->m_PathName.empty() )
		m_WriteHandle.Close();
	if ( !m_Message->m_UploadPathName.empty() )
		m_ReadHandle.Close();
	curl_easy_getinfo (m_Curl, CURLINFO_RESPONSE_CODE, &(m_Message->m_ResponseCode));
     
    m_Message->m_ResponseHeaders = m_ResponseHeaders;
    m_Message->m_ResponseString = m_ResponseString;

	if ( res == CURLE_OK )
	{
		m_Message->m_Result = m_ResponseString;
		m_Message->m_Success = true;
		if ( m_Message->m_Cache ) {
			if ( m_Message->m_ResponseCode == 200 )
			{
				m_Message->m_Cache->SetResponseHeaders(m_ResponseHeaders);
				m_Message->m_Cache->Store(m_ResponseString);
			}
			else if ( m_Message->m_ResponseCode == 304 ) // not modified
			{
				m_Message->m_ResponseCode = 200;
				m_Message->m_Result = m_Message->m_Cache->GetResponseData();
				m_Message->m_Cache->Touch(); // Touch cache timestamp
			}
		}
	}
	else
	{
		m_Message->m_Result =  m_CurlErrorBuffer;
		m_Message->m_Success = false;
	}
	CurlRequest::Instance().PushDone(m_Message, m_Group);
	CurlHandle* pThis = this;
	UNITY_DELETE(pThis, kMemCurl);
}

size_t CurlHandle::HeaderCallback(void *data, size_t size, size_t elements, void *stream)
{
	CurlHandle *pThis = static_cast<CurlHandle *>(stream);
	std::string header = "";
	header.append((char *)data, size*elements);
	header = Trim(header,"\r\n\t ");
	
	if ( ! header.empty() )
		pThis->m_ResponseHeaders.push_back(header);
	return size*elements;
}

size_t CurlHandle::WriteMemoryCallback(void *data, size_t size, size_t elements, void *stream)
{
	CurlHandle *pThis = static_cast<CurlHandle *>(stream);
	pThis->m_ResponseString.append((char *)data, size*elements);
	return size*elements;
}

size_t CurlHandle::WriteFileCallback(void *data, size_t size, size_t elements, void *stream)
{
	CurlHandle *pThis = static_cast<CurlHandle *>(stream);
	pThis->m_WriteHandle.Write(data, size*elements);
	return size*elements;
}

size_t CurlHandle::ReadFileCallback(void *data, size_t size, size_t elements, void *stream)
{
	CurlHandle *pThis = static_cast<CurlHandle *>(stream);
	
	int readSize = pThis->m_ReadHandle.Read(data, size*elements);
	return readSize;
}

int CurlHandle::ProgressCallback(void *clientp, double dltotal, double dlnow,  double ultotal, double ulnow)
{
	CurlHandle *pThis = static_cast<CurlHandle *>(clientp);
	if (!pThis->m_IsWritingToFile && dltotal > 0 && dltotal > pThis->m_ResponseString.capacity())
		pThis->m_ResponseString.reserve(dltotal+1);
	if ( !pThis->m_Message->m_UploadPathName.empty() )
		return (int)CurlRequest::Instance().PushProgress(pThis->m_Message, pThis->m_Group, (size_t)ulnow, (size_t)ultotal);
	else
		return (int)CurlRequest::Instance().PushProgress(pThis->m_Message, pThis->m_Group, (size_t)dlnow, (size_t)dltotal);
}
	
void CurlRequestMessage::Abort()
{
	AtomicIncrement(&m_ShouldAbort);
}

CurlRequestMessage::~CurlRequestMessage()
{
	UNITY_DELETE(m_Cache, kMemCurl);
}

void CurlRequestAbortTag(const string& tag, int group)
{
	CurlRequest::Instance().AbortTag(tag, group);
}

void CurlRequestAbort(CurlRequestMessage *pMessage, int group)
{
	CurlRequest::Instance().AbortRequestMessage(pMessage, group);
}


void CurlRequestGet(CurlRequestMessage *pMessage, int group)
{
	if ( CurlRequest::Failed(group) ) 
	{
		// Make sure Done is called even if the maximum fail limit was reached
		pMessage->m_Success = false;
		pMessage->m_Result = "HTTP requests disabled because it reached the maximum number of failing requests";
		pMessage->Done ();
		UNITY_DELETE( pMessage, kMemCurl);
	}
	else 
	{
		CurlRequest::Instance().PushNew(pMessage, group);
	}
}

const int kCacheCleanupTicks = 13000;

// This should be called regularly on the main thread
void CurlRequestCheck()
{
	static int ticksUnitlCacheCleanup = kCacheCleanupTicks;
	CurlRequest::Instance().Check();
	
	if ( --ticksUnitlCacheCleanup == 0 )
	{
		ticksUnitlCacheCleanup = kCacheCleanupTicks;
		CurlFileCacheCleanup();
	}
}

bool CurlRequestFailed(int group)
{
	return CurlRequest::Failed(group);
}

void CurlRequestInitialize()
{
	#define CURL_GLOBAL_SSL (1<<0)
	#define CURL_GLOBAL_WIN32 (1<<1)
	#define CURL_GLOBAL_ALL (CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32)
	#define CURL_GLOBAL_DEFAULT CURL_GLOBAL_ALL
	curl_global_init(CURL_GLOBAL_DEFAULT);
	CurlFileCacheCleanup();
}


void CurlRequestCleanup()
{
	CurlFileCacheCleanup();
	CurlRequest::Cleanup();
	curl_global_cleanup();
}

string CurlUrlEncode (const string &s)
{
	char *escaped = curl_escape (s.c_str (), s.length ()); 
	string r = escaped;
	curl_free (escaped);
	return r;
}

string CurlUrlDecode (const string &s)
{
	char *unescaped = curl_unescape (s.c_str (), s.length ()); 
	string r = unescaped;
	curl_free (unescaped);
	return r;
}
