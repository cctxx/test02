using System;
using System.Reflection;
using System.Security;
using System.Text;
using CrossDomainPolicyParser;
using System.IO;
using System.Collections.Generic;
using MonoForks.System.Windows.Browser.Net;

internal class Log
{
	public delegate void LogDelegate(string msg);
	
	static LogDelegate logger;
	
	static public void SetLog(LogDelegate ld)
	{
		logger = ld;
	}
	
	static public void Msg(string msg)
	{
		if (logger != null) logger(msg);
	}
}

namespace UnityEngine
{
    public class UnityCrossDomainHelper
    {
        public enum SecurityPolicy
        {
            DontKnowYet = 0,
            AllowAccess = 1,
            DenyAccess = 2
        }

    	public delegate string GetWebSecurityHostUriDelegate();
    	private static GetWebSecurityHostUriDelegate getWebSecurityHostUriDelegate = DefaultGetWebSecurityHostUri;
    	static WWWPolicyProvider wwwPolicyProvider = new WWWPolicyProvider();

		static public string GetWebSecurityHostUri()
		{
			return getWebSecurityHostUriDelegate();	
		}

		static internal void SetWebSecurityHostUriDelegate(GetWebSecurityHostUriDelegate d)
		{
			getWebSecurityHostUriDelegate = d;	
		}

		static string DefaultGetWebSecurityHostUri()
		{
			return Application.webSecurityHostUrl;
		}

        public static void ClearCache()
        {
            wwwPolicyProvider.ClearCache();
            CrossDomainPolicyManager.ClearCache();
        }

		interface IPolicyProvider
		{
			Stream GetPolicy(string url);
		}

		class WWWPolicyProvider : IPolicyProvider
		{
			Dictionary<string,WWW> policyDownloads = new Dictionary<string, WWW>();

			public void ClearCache()
			{
				policyDownloads.Clear();
			}

			public Stream GetPolicy(string policyurl)
			{
				WWW downloadInProgress = null;
				policyDownloads.TryGetValue(policyurl, out downloadInProgress);

				if (downloadInProgress != null)
				{
					if (!downloadInProgress.isDone) return null;

					bool statuscodeOK = downloadInProgress.error == null;
					
					if (!statuscodeOK) throw new InvalidOperationException("Unable to download policy");
					if (statuscodeOK)
					{
						Log.Msg("Download had OK statuscode");
						Log.Msg("Received the following crossdomain.xml");
						Log.Msg("----------");
						Log.Msg(downloadInProgress.text);
						Log.Msg("----------");
						return new MemoryStream(downloadInProgress.bytes);
					}

				}

				//okay, we hadn't started downloading the policy, lets start the policy download.
				var www = new WWW(policyurl);
				policyDownloads.Add(policyurl, www);
				return null;
			}
		}

		class WebRequestPolicyProvider : IPolicyProvider
		{
			private MethodInfo methodinfo;

			public WebRequestPolicyProvider(MethodInfo mi)
			{
				methodinfo = mi;
			}

			[System.Security.SecuritySafeCritical]
			public Stream GetPolicy(string policy_url)
			{
				var proxy = System.Environment.GetEnvironmentVariable("UNITY_PROXYSERVER");
				if (string.IsNullOrEmpty(proxy))
					proxy = null;
				object result = methodinfo.Invoke(null, new object[] {policy_url, proxy});
				return (Stream) result;
			}
		}

		public static SecurityPolicy GetSecurityPolicy(string requesturi_string)
		{
			return GetSecurityPolicy(requesturi_string, wwwPolicyProvider);
		}

		public static bool GetSecurityPolicyForDotNetWebRequest(string requesturi_string, MethodInfo policyProvidingMethod)
		{
			var provider = new WebRequestPolicyProvider(policyProvidingMethod);

			return GetSecurityPolicy(requesturi_string, provider) == SecurityPolicy.AllowAccess;
		}

		static SecurityPolicy GetSecurityPolicy(string requesturi_string, IPolicyProvider policyProvider)
        {
			var requesturi = UriTools.MakeUri(Application.webSecurityHostUrl, requesturi_string);
			if (requesturi.Scheme=="file")
			{
				//Editor-In-WebMode is allowed to read files from file://
				if (Application.isEditor) return SecurityPolicy.AllowAccess;

				//Webplayer itself is allowed to read files from file:// as long as it is hosted on file:// itself as well.
				var hostedat = new MonoForks.System.Uri(Application.webSecurityHostUrl);
				if (Application.isWebPlayer && hostedat.Scheme == "file") return SecurityPolicy.AllowAccess;

				//other scenarios of accessing file:// are not allowed
				return SecurityPolicy.DenyAccess;
			}
			
            //todo: force absolute
            ICrossDomainPolicy policy = CrossDomainPolicyManager.GetCachedWebPolicy(requesturi);
			if (policy != null)
            {
                var request = new MonoForks.System.Net.WebRequest(requesturi, new Dictionary<string, string>());
                SecurityPolicy allowed = policy.IsAllowed(request) ? SecurityPolicy.AllowAccess : SecurityPolicy.DenyAccess;
                return allowed;
			}
        	
            if (ShouldEnableLogging())
    			Log.SetLog(Console.WriteLine);
        	
        	Log.Msg("Determining crossdomain.xml location for request: " + requesturi);
			var policyURI = CrossDomainPolicyManager.GetFlashPolicyUri(requesturi);

        	Stream s;
			try
        	{
				s = policyProvider.GetPolicy(policyURI.ToString());
				if (s == null) return SecurityPolicy.DontKnowYet;
				CrossDomainPolicyManager.BuildFlashPolicy(true, policyURI, s, new Dictionary<string, string>());
			} catch (InvalidOperationException)
			{
				return SecurityPolicy.DenyAccess;				
			}
			catch (MonoForks.Mono.Xml.MiniParser.XMLError xe)
			{
				Debug.Log (string.Format ("Error reading crossdomain policy: {0}", xe.Message));
				return SecurityPolicy.DenyAccess;				
			}
			return GetSecurityPolicy(requesturi_string, policyProvider);
        }

		[SecuritySafeCritical]
    	private static bool ShouldEnableLogging()
    	{
    		return Environment.GetEnvironmentVariable("ENABLE_CROSSDOMAIN_LOGGING")=="1";
    	}

    	static public bool CheckSocketEndPoint(string connecting_to_ip, int port)
		{
			Log.Msg("CheckSocketEndpoint called for "+connecting_to_ip+" with port: "+port);

			if (!Application.webSecurityEnabled) return true;
			
			bool result = CrossDomainPolicyManager.CheckSocketEndPoint(connecting_to_ip,port);
			Log.Msg("CheckSocketENdpoint returns :"+result);
			return result;
		}
		static public bool PrefetchSocketPolicy(string ip, int policyport, int timeout)
		{
			if (!Application.webSecurityEnabled) return false;
            var policy = CrossDomainPolicyManager.FlashCrossDomainPolicyFor(ip, policyport, timeout);
            return policy != FlashCrossDomainPolicy.DenyPolicy;
		}

    }
}
