#include "UnityPrefix.h"

#include "Analytics.h"

#include <queue>
#include <time.h>
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Application.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Configuration/UnityConfigureVersion.h"

using namespace std;


#define GA_VERSION "1.3" // Tracker version

#define GA_ACCOUNT "UA-16068464-6"
#define GA_WEBSITE "editor.analytics.unity3d.com"
#define GA_ACCOUNT_DEV "UA-16068464-5"
#define GA_WEBSITE_DEV "editordev.analytics.unity3d.com"

#define GA_GIF "http://www.google-analytics.com/__utm.gif"
#define GA_VISITOR_PREF_NAME "GA_VISITOR_ID"
#define GA_COOKIE_PREF_NAME "GA_COOKIE"

#define USE_KONTAGENT 0


enum
{
	kMaxAnalyticsRequestsPerSession = 490
};

const double kMinimumRequestInterval = 0.1; // Minimum time between two identical analytics requests 


static UInt32 GetRandom ()
{
	static Rand r (time (NULL));
	return r.Get ();
}

static const string &GetAccount ()
{
	static string account = IsDeveloperBuild () || UNITY_IS_BETA ? GA_ACCOUNT_DEV : GA_ACCOUNT;
	return account;
}

static const string &GetWebSite ()
{
	static string site = IsDeveloperBuild () || UNITY_IS_BETA ? GA_WEBSITE_DEV : GA_WEBSITE;
	return site;
}

static const string &GetSystemLanguage ()
{
	static string language = "";
	if ( language.empty () ) 
	{	
		int sl = systeminfo::GetSystemLanguage();
		if ( sl == SystemLanguageChinese ) language = "zh";
		else if ( sl == SystemLanguageEnglish ) language = "en";
		else if ( sl == SystemLanguageFrench ) language = "fr";
		else if ( sl == SystemLanguageGerman ) language = "de";
		else if ( sl == SystemLanguageRussian ) language = "ru";
		else if ( sl == SystemLanguageSpanish ) language = "es";
		else language = "-";
	}
	return language;
}

static UInt32 GetUniqueVisitorID ()
{
	// Create a new id
	string license = GetApplication ().GetLicenseInfoText ();
	string s = Format ("%s-%u", license.c_str (), (unsigned int)GetRandom ());
	
	UInt8 hash[16];
	ComputeMD5Hash ((const UInt8*)s.c_str (), s.size (), hash);
	return *(UInt32*)hash; 	
}

static void GetCookie (UInt32 c[6])
{
	// <ignored_number>.<visitorId>.<visitorsFirstHitTime>.<lastSessionTimeOfSessionFirstHit>.<TimeOfSessionFirstHit>.<sessionCount>
	//__utmz=<ignored_number>.<TimeOfSessionFirstHit>.<sessionCount>.1.<campValues>;
	//		where <campValues> = <valueName>=<value>|<valueName>=<value>|...
	

	static UInt32 cookie[6] = {-1};
	if ( cookie[0] == -1 ) 
	{
		if ( EditorPrefs::HasKey (GA_COOKIE_PREF_NAME) ) 
		{
			// Try to read the cookie 
			string c = EditorPrefs::GetString (GA_COOKIE_PREF_NAME);
			if ( sscanf(c.c_str (), "%u.%u.%u.%u.%u.%u", &cookie[0], &cookie[1], &cookie[2], &cookie[3], &cookie[4], &cookie[5]) == 6 ) 
			{
				// The cookie was read
				cookie[3] = cookie[4];
				cookie[4] = time (NULL);
				cookie[5] += 1;
			}
			else
			{
				// cookie not read correctly
				cookie[0] = -1;
			}
		}
		if ( cookie[0] == -1 ) 
		{
			// Create a new cookie
			cookie[0] = 1;
			cookie[1] = GetUniqueVisitorID ();
			cookie[2] = time (NULL);
			cookie[3] = cookie[2];
			cookie[4] = cookie[2];
			cookie[5] = 1;
		}
		EditorPrefs::SetString (GA_COOKIE_PREF_NAME, Format ("%u.%u.%u.%u.%u.%u", cookie[0], cookie[1], cookie[2], cookie[3], cookie[4], cookie[5]));
	}
	c[0] = cookie[0];
	c[1] = cookie[1];
	c[2] = cookie[2];
	c[3] = cookie[3];
	c[4] = cookie[4];
	c[5] = cookie[5];
}

// Build URL for the Google Analytics gif request
static string BuildGoogleAnalyticsURL (string referer, string query, string path, string event)
{
    // Construct the gif hit url.
	string utmUrl = GA_GIF;
	utmUrl += "?";
	utmUrl += string ("utmwv=") + GA_VERSION;
	utmUrl += Format ("&utmn=%u", GetRandom ());
	utmUrl += "&utmcs=ISO-8859-1";
	if (GetScreenManagerPtr())
		utmUrl += Format ("&utmsr=%dx%d", GetScreenManager ().GetCurrentResolution ().width, GetScreenManager ().GetCurrentResolution ().height);
	utmUrl += "&utmsc=24-bit";
	utmUrl += "&utmul=" + GetSystemLanguage ();
	utmUrl += "&utmje=0";
	utmUrl += "&utmfl=-";
	utmUrl += "&utmdt=" + CurlUrlEncode ("Unity Editor");
	utmUrl += "&utmhn=" + CurlUrlEncode (GetWebSite ());

	// utmr: Referral, complete URL. Ex: utmr=http://www.example.com/aboutUs/index.php?var=selected
	if ( referer.empty ()) 
		referer = "-";	
	utmUrl += "&utmr=" + CurlUrlEncode (referer);
	
	// utmp: Page request of the current page. Ex: utmp=/testDirectory/myPage.html
	if ( !path.empty () ) 
	{
		if ( !query.empty () ) 
		{
			path += "?" + query;
		}
		utmUrl += "&utmp=" + CurlUrlEncode (path); 
	}

	// utme: Event 
	if ( !event.empty () ) 
	{		
		utmUrl += "&utmt=event&utme=" + CurlUrlEncode (event);
	}
	
	utmUrl += string ("&utmac=") + GetAccount ();
	
	UInt32 cookie[6];
	GetCookie (cookie);
	utmUrl += "&utmcc=" +
		Format ("__utma%%3D%u.%u.%u.%u.%u.%u%%3B", cookie[0], cookie[1], cookie[2], cookie[3], cookie[4], cookie[5]) +
		Format ("%%2B__utmz%%3D1.%u.%u.1.utmccn%%3D(direct)%%7Cutmcsr%%3D(direct)%%7Cutmcmd%%3D(none)%%3B%%2B", cookie[4], cookie[5]);	
	
    return utmUrl;	
}


static void SendAnalyticsRequest (string path, string event)
{
	// Compose the HTTP GET request
	CurlRequestMessage *pMessage = UNITY_NEW(CurlRequestMessage, kMemCurl);
	pMessage->m_Uri = BuildGoogleAnalyticsURL ("", "", path, event); 
	CurlRequestGet (pMessage);
}

// AnalyticsProcessTracker
AnalyticsProcessTracker::AnalyticsProcessTracker (const std::string &category, const std::string &action, const std::string &label)
{
	m_Category = category;
	m_Action = action;
	m_Label = label;
	m_TimeStart = GetTimeSinceStartup ();
	m_Succeeded = false;
	AnalyticsTrackPageView (m_Category + "/" + m_Action + "/Start");
}

AnalyticsProcessTracker::~AnalyticsProcessTracker () 
{ 
	int seconds = RoundfToInt(GetTimeSinceStartup () - m_TimeStart);
	if ( m_Succeeded ) 
	{
		AnalyticsTrackPageView (m_Category + "/" + m_Action + "/Done");
		AnalyticsTrackEvent (m_Category, m_Action, m_Label, seconds);
		
		printf_console("\n*** Completed '%s.%s.%s' in %d seconds\n\n", m_Category.c_str(), m_Action.c_str(), m_Label.c_str(), seconds);
	}
	else
	{
		AnalyticsTrackPageView (m_Category + "/" + m_Action + "/Cancel");
		printf_console("\n*** Cancelled '%s.%s.%s' in %d seconds\n\n", m_Category.c_str(), m_Action.c_str(), m_Label.c_str(), seconds);
	}
}

static bool sAnalyticsTrackingEnabled = true;
static bool sAnalyticsTrackingOverflow = false;
static bool sCheckForBuildMachineOrBatchMode = true;
static int sAnalyticsRequestCount = 0;
static string sLastRequestValue;
static double sLastRequestTime = 0;

static void Track (const std::string &value, bool pageview, bool debug, bool forceRequest)
{
	// Do not collect analytics on a build machine or when running in batch mode
	if ( sCheckForBuildMachineOrBatchMode ) 
	{
		sCheckForBuildMachineOrBatchMode = false;
		sAnalyticsTrackingEnabled = IsHumanControllingUs ();
		sAnalyticsTrackingEnabled &= EditorPrefs::GetBool ("EnableEditorAnalytics", true);

		const char* s = getenv("UNITY_ENABLE_ANALYTICS");
		if (s!=0) if (!strcmp(s,"1")) sAnalyticsTrackingEnabled = true;
	}
	
	// If analytics is overflowed (kMaxAnalyticsRequestsPerSession is exeded) only add requests if forceRequest is true
	if ( sAnalyticsTrackingEnabled && (!sAnalyticsTrackingOverflow || forceRequest) ) 
	{
		if ( !CurlRequestFailed () ) 
		{
			bool error = false;
			
			// Sanity check the requests. Don't allow the same request to be repeated in a very short interval
			string requestValue = value + (pageview ? "P" : "E");
			double requestTime = GetTimeSinceStartup ();
			if ( requestTime - sLastRequestTime < kMinimumRequestInterval && requestValue == sLastRequestValue ) 
			{
				printf_console ("Error: Analytics %s: %s skipped because it was sent more than once in %.2f seconds\n", (pageview? "PageView": "Event"), value.c_str (), kMinimumRequestInterval);
				error = true;
			}
			sLastRequestTime = requestTime;
			sLastRequestValue = requestValue;
			
			// There is a limit on the number of requests you can send to GA in a session. This is only send once per session
			if ( !sAnalyticsTrackingOverflow && (++sAnalyticsRequestCount > kMaxAnalyticsRequestsPerSession) )
			{
				printf_console ("Error: Analytics tracking has been disabled because more than %d analytics requests was send\n", kMaxAnalyticsRequestsPerSession);
				sAnalyticsTrackingOverflow = true;
				SendAnalyticsRequest ("/Analytics/MaxRequestsReached", ""); // Track that we ran out of requests
			}
			
			if ( !error ) 
			{
				SendAnalyticsRequest (pageview ? value : "", pageview ? "" : value);
			}
		}
		else
		{
			printf_console ("Error: Analytics tracking has been disabled because too many server communication errors have occurred\n");
			sAnalyticsTrackingEnabled = false;
		}
	}
}

#if USE_KONTAGENT
//#define KONTAGENT_API_SERVER "http://test-server.kontagent.net"
#define KONTAGENT_API_SERVER "http://api.geo.kontagent.net"
#define KONTAGENT_API_KEY "864b325cd3d34dc08784f26ce2f54d1d"

static const string &GetUniqueVisitorKontagentID ()
{
	static string uid;
	if ( uid.empty () ) 
	{
		// Create a new id
		string license = GetApplication ().GetLicenseInfoText ();
		
		UInt8 hash[16];
		ComputeMD5Hash ((const UInt8*)license.c_str (), license.size (), hash);
		uid = MD5ToString (hash);
	}
	return uid;
}


static void TrackKontagent (const std::string &message_type, const std::string &parameters)
{
	// Do not collect analytics on a build machine or when running in batch mode
	if ( sCheckForBuildMachineOrBatchMode ) 
	{
		sCheckForBuildMachineOrBatchMode = false;
		sAnalyticsTrackingEnabled = IsHumanControllingUs ();
		sAnalyticsTrackingEnabled &= EditorPrefs::GetBool ("EnableEditorAnalytics", true);
		
		const char* s = getenv("UNITY_ENABLE_ANALYTICS");
		if (s!=0) if (!strcmp(s,"1")) sAnalyticsTrackingEnabled = true;
	}
	
	// If analytics is overflowed (kMaxAnalyticsRequestsPerSession is exeded) only add requests if forceRequest is true
	if ( sAnalyticsTrackingEnabled ) 
	{
		if ( !CurlRequestFailed () ) 
		{
			bool error = false;
			
			// Sanity check the requests. Don't allow the same request to be repeated in a very short interval
			string requestValue = message_type + ":" + parameters;
			double requestTime = GetTimeSinceStartup ();
			if ( requestTime - sLastRequestTime < kMinimumRequestInterval && requestValue == sLastRequestValue ) 
			{
				printf_console ("Error: Analytics %s: %s skipped because it was sent more than once in %.2f seconds\n", message_type.c_str (), parameters.c_str (), kMinimumRequestInterval);
				error = true;
			}
			sLastRequestTime = requestTime;
			sLastRequestValue = requestValue;
			
			if ( !error ) 
			{
				// Compose the HTTP GET request
				CurlRequestMessage *pMessage = UNITY_NEW(CurlRequestMessage, kMemCurl);
				
				string url = string(KONTAGENT_API_SERVER) + "/api/v1/" + KONTAGENT_API_KEY + "/" + message_type + "/";
				GetUniqueVisitorID ();

				if ( !parameters.empty() )
					url += "?" + parameters;
					
				pMessage->m_Uri = url; 
				CurlRequestGet (pMessage);
			}
		}
		else
		{
			printf_console ("Error: Analytics tracking has been disabled because too many server communication errors have occurred\n");
			sAnalyticsTrackingEnabled = false;
		}
	}
}
#endif

static void SplitString (const std::string s, std::vector<std::string> &parts)
{
	Split (s, '/', parts);
}

void AnalyticsTrackPageView (const std::string &page, bool forceRequest)
{
	if (sAnalyticsTrackingEnabled)
	{
		
		Track (page, true, false, forceRequest);

#if USE_KONTAGENT
		std::vector<std::string> parts;
		SplitString (page, parts);
		if ( parts.size () > 3 ) 
		{
			printf_console ("Error: Analytics %s more than 3 parts in the pageview\n", page.c_str ());
		}
		std::string s1 = parts.size () > 0 ? parts[0]: ""; 
		std::string s2 = parts.size () > 1 ? parts[1]: ""; 
		std::string s3 = parts.size () > 2 ? parts[2]: ""; 
		string parameters = Format ("st1=%s&st2=%s&st3=%s", CurlUrlEncode (s1).c_str (), CurlUrlEncode (s2).c_str (), CurlUrlEncode (s3).c_str ());
		TrackKontagent ("evt", Format("s=%s&n=%s&%s&v=%d", GetUniqueVisitorKontagentID ().c_str (),  "page", parameters.c_str (), 0));
#endif
	}
}

void AnalyticsTrackEvent (const std::string &category, const std::string &action, const std::string &label, int value, bool forceRequest)
{
	if (sAnalyticsTrackingEnabled)
	{
		string event = Format ("5(%s*%s*%s)(%d):", category.c_str (), action.c_str (), label.c_str (), value);
		Track (event, false, false, forceRequest);

#if USE_KONTAGENT
		string parameters = Format ("st1=%s&st2=%s&st3=%s", CurlUrlEncode (category).c_str (), CurlUrlEncode (action).c_str (), CurlUrlEncode (label).c_str ());
		TrackKontagent ("evt", Format("s=%s&n=%s&%s&v=%d", GetUniqueVisitorKontagentID ().c_str (),  "event", parameters.c_str (), value));
#endif
	}
}
