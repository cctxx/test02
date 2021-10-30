//
// CrossDomainPolicyManager.cs
//
// Authors:
//	Atsushi Enomoto <atsushi@ximian.com>
//	Moonlight List (moonlight-list@lists.ximian.com)
//
// Copyright (C) 2009 Novell, Inc.  http://www.novell.com
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#define NET_2_1
#if NET_2_1

using System;
using System.Collections.Generic;
using System.IO;
using MonoForks.System.Windows.Interop;
using System.Security;
using System.Reflection;

namespace MonoForks.System.Windows.Browser.Net
{
	internal static class CrossDomainPolicyManager
	{

		public static string GetRoot(Uri uri)
		{
			if ((uri.Scheme == "http" && uri.Port == 80) || (uri.Scheme == "https" && uri.Port == 443) || (uri.Port == -1))
				return String.Format("{0}://{1}/", uri.Scheme, uri.DnsSafeHost);
			else
				return String.Format("{0}://{1}:{2}/", uri.Scheme, uri.DnsSafeHost, uri.Port);
		}

		public const string ClientAccessPolicyFile = "/clientaccesspolicy.xml";
		public const string CrossDomainFile = "/crossdomain.xml";

		const int Timeout = 10000;

		// Web Access Policy

		static Dictionary<string, ICrossDomainPolicy> policies = new Dictionary<string, ICrossDomainPolicy>();

		static internal ICrossDomainPolicy PolicyDownloadPolicy = new PolicyDownloadPolicy();
		static ICrossDomainPolicy site_of_origin_policy = new SiteOfOriginPolicy();
		static ICrossDomainPolicy no_access_policy = new NoAccessPolicy();

		static Uri GetRootUri(Uri uri)
		{
			return new Uri(GetRoot(uri));
		}

		public static Uri GetSilverlightPolicyUri(Uri uri)
		{
			return new Uri(GetRootUri(uri), CrossDomainPolicyManager.ClientAccessPolicyFile);
		}

		public static Uri GetFlashPolicyUri(Uri uri)
		{
			return new Uri(GetRootUri(uri), CrossDomainPolicyManager.CrossDomainFile);
		}

		public static ICrossDomainPolicy GetCachedWebPolicy(Uri uri)
		{
			//Debug.Log("Got to GetCachedWebPolicy");
			//Debug.Log("debug1. uri: " + uri.ToString() + " pluginhost.sourceuri: " + PluginHost.SourceUri);
			//Debug.Log("debug1.1 pluginhost.rooturi: " + PluginHost.RootUri);
			// if we request an Uri from the same site then we return an "always positive" policy
			if (SiteOfOriginPolicy.HasSameOrigin(uri, PluginHost.SourceUri))
				return site_of_origin_policy;

			//Debug.Log("debug2");

			//if this is a request for a crossdomainfile, then we allow it.
			//TODO: Make this more secure.
			string postfix = "";
			if (!uri.IsDefaultPort) postfix = ":" + uri.Port;

			if (uri.ToString() == uri.Scheme + "://" + uri.Host + postfix + "/crossdomain.xml") return PolicyDownloadPolicy;

			//Debug.Log("debug3");
			// otherwise we search for an already downloaded policy for the web site
			string root = GetRoot(uri);
			ICrossDomainPolicy policy = null;
			policies.TryGetValue(root, out policy);
			// and we return it (if we have it) or null (if we dont)
			//Debug.Log("debug4: " + policy);
			return policy;
		}

		private static void AddPolicy(Uri responseUri, ICrossDomainPolicy policy)
		{
			string root = GetRoot(responseUri);
			try
			{
				policies.Add(root, policy);
			}
			catch (ArgumentException)
			{
				// it's possible another request already added this root
			}
		}

		/*
				public static ICrossDomainPolicy BuildSilverlightPolicy (HttpWebResponse response)
				{
					// return null if no Silverlight policy was found, since we offer a second chance with a flash policy
					if (response.StatusCode != HttpStatusCode.OK)
						return null;

					ICrossDomainPolicy policy = null;
					try {
						policy = ClientAccessPolicy.FromStream (response.GetResponseStream ());
						if (policy != null)
							policies.Add (GetRoot (response.ResponseUri), policy);
					} catch (Exception ex) {
						Console.WriteLine (String.Format ("CrossDomainAccessManager caught an exception while reading {0}: {1}", 
							response.ResponseUri, ex.Message));
						// and ignore.
					}
					return policy;
				}

				public static ICrossDomainPolicy BuildFlashPolicy (HttpWebResponse response)
				{
					bool ok = response.StatusCode == HttpStatusCode.OK;
					return BuildFlashPolicy(ok, response.ResponseUri, response.GetResponseStream(), response.Headers);
				}
		*/
		public static ICrossDomainPolicy BuildFlashPolicy(bool statuscodeOK, Uri uri, Stream responsestream, Dictionary<string, string> responseheaders)
		{
			ICrossDomainPolicy policy = null;
			if (statuscodeOK)
			{
				try
				{
					policy = FlashCrossDomainPolicy.FromStream(responsestream);
				}
				catch (Exception ex)
				{
					Log.Msg(String.Format("BuildFlashPolicy caught an exception while parsing {0}: {1}",
						uri, ex.Message));
					throw;
				}
				if (policy != null)
				{
					// see DRT# 864 and 865
					Log.Msg("crossdomain.xml was succesfully parsed");
					string site_control = null;
					responseheaders.TryGetValue("X-Permitted-Cross-Domain-Policies", out site_control);
					if (!String.IsNullOrEmpty(site_control))
						(policy as FlashCrossDomainPolicy).SiteControl = site_control;
				}
			}

			// the flash policy was the last chance, keep a NoAccess into the cache
			if (policy == null)
				policy = no_access_policy;

			AddPolicy(uri, policy);
			return policy;
		}

		public static void ClearCache()
		{
			policies.Clear();
		}

		// Socket Policy
		//
		// - we connect once to a site for the entire application life time
		// - this returns us a policy file (silverlight format only) or else no access is granted
		// - this policy file
		// 	- can contain multiple policies
		// 	- can apply to multiple domains
		//	- can grant access to several resources

		static readonly Dictionary<string, FlashCrossDomainPolicy> SocketPoliciesByIp = new Dictionary<string, FlashCrossDomainPolicy>();
		const int PolicyPort = 843;

		static public bool CheckSocketEndPoint(string connecting_to_ip, int port)
		{
			return CheckSocketEndPoint(connecting_to_ip,port,PolicyPort);
		}
		
		static public bool CheckSocketEndPoint(string connecting_to_ip, int port, int policyport)
		{
			var policy = FlashCrossDomainPolicyFor(connecting_to_ip, policyport, 3000);
			return policy.IsSocketConnectionAllowed(port);
		}

		public static FlashCrossDomainPolicy FlashCrossDomainPolicyFor(string connecting_to_ip, int policyport, int timeout)
		{
			FlashCrossDomainPolicy cachedPolicy;
			if (SocketPoliciesByIp.TryGetValue(connecting_to_ip, out cachedPolicy))
			{
				Log.Msg(String.Format("Policy for host {0} found in the cache.", connecting_to_ip));
				return cachedPolicy;
			}

			try
			{
				FlashCrossDomainPolicy policy = RetrieveFlashCrossDomainPolicyFrom(connecting_to_ip, policyport,timeout);
				policy.PolicyPort = policyport;
				SocketPoliciesByIp.Add(connecting_to_ip, policy);
				return policy;
			}
			catch (Exception ex)
			{
				Log.Msg(String.Format("{0} caught an exception while checking endpoint {1}: {2}", typeof(CrossDomainPolicyManager).Name, connecting_to_ip, ex));
				return FlashCrossDomainPolicy.DenyPolicy;
			}
		}

		[SecuritySafeCritical]
		private static FlashCrossDomainPolicy RetrieveFlashCrossDomainPolicyFrom(string host, int port, int timeout)
		{
			var type = Type.GetType("System.Net.Sockets.SocketPolicyClient, System");
			var stream = (Stream)type.InvokeMember("GetPolicyStreamForIP", BindingFlags.InvokeMethod | BindingFlags.Static | BindingFlags.NonPublic, null, null, new object[] { host, port, timeout });

            if (stream == null) throw new Exception("got back null stream from getpolicystream");
			return FlashCrossDomainPolicy.FromStream(stream);
		}
	}
}

#endif

