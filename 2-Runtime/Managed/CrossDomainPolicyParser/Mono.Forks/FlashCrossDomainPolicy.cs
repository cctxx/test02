//
// FlashCrossDomainPolicy.cs
//
// Author:
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
using MonoForks.System;
using System.Collections.Generic;
using System.IO;
using MonoForks.System.Net;
using UnityEngine;

namespace MonoForks.System.Windows.Browser.Net {

	partial class FlashCrossDomainPolicy : BaseDomainPolicy {

		private string site_control;
		public int PolicyPort { get; set; }

		public FlashCrossDomainPolicy ()
		{
			AllowedAccesses = new List<AllowAccessFrom> ();
			AllowedHttpRequestHeaders = new List<AllowHttpRequestHeadersFrom> ();
			PolicyPort = 843;
		}
		
		public static FlashCrossDomainPolicy DenyPolicy = new FlashCrossDomainPolicy();

		public List<AllowAccessFrom> AllowedAccesses { get; private set; }
		public List<AllowHttpRequestHeadersFrom> AllowedHttpRequestHeaders { get; private set; }

		public string SiteControl {
			get { return String.IsNullOrEmpty (site_control) ? "all" : site_control; }
			set { site_control = value; }
		}

		public bool IsSocketConnectionAllowed(int port)
		{
			foreach(var allowed in AllowedAccesses)
			{
				if (allowed.IsSocketConnectionAllowed (port, PolicyPort))
                        return true;
			}
			return false;
		}

		public override bool IsAllowed (Uri uri, string [] headerKeys)
		{
            switch (SiteControl) {
			case "all":
			case "master-only":
			case "by-ftp-filename":
				break;
			default:
				// others, e.g. 'none', are not supported/accepted
				Log.Msg("rejected because SiteControl does not have a valid value");
				return false;
			}
            bool any = false;
            if (AllowedAccesses.Count > 0)
            {
				foreach (var a in AllowedAccesses)
				{
					if (a.IsAllowed(uri, headerKeys))
					{
						any = true;
					}
				}
            }
            if (!any)
            {
				Log.Msg("Rejected because there was no AllowedAcces entry in the crossdomain file allowing this request.");
                return false;
            }

			if (AllowedHttpRequestHeaders.Count > 0)
                foreach(var h in AllowedHttpRequestHeaders)
                    if (h.IsRejected(uri,headerKeys)) return false;

			return true;
		}

		public class AllowAccessFrom {

			public AllowAccessFrom ()
			{
				Secure = true;	// true by default
			}

			public string Domain { get; set; }
			public bool AllowAnyPort { get; set; }
			public int [] ToPorts { get; set; }
			public bool Secure { get; set; }

			public bool IsAllowed (Uri uri, string [] headerKeys)
			{
				Log.Msg("Checking if "+uri+" is a valid domain");
                if (!CheckDomain(uri)) return false;

                if (!AllowAnyPort && ToPorts != null && Array.IndexOf(ToPorts, uri.Port) < 0)
                {
					Log.Msg("requested port: "+uri.Port+" is not allowed by specified portrange");
                	return false;
                }

				// if Secure is false then it allows applications from HTTP to download data from HTTPS servers
				if (!Secure)
					return true;
				// if Secure is true then only application on HTTPS servers can access data on HTTPS servers
				if (ApplicationUri.Scheme == Uri.UriSchemeHttps)
					return (uri.Scheme == Uri.UriSchemeHttps);
				// otherwise FILE/HTTP applications can access HTTP uris

				Log.Msg("All requirements met, the request is approved");
				return true;
			}

			public bool IsSocketConnectionAllowed(int port, int policyport)
			{
				if (policyport>1024 && port<1024) return false;

				bool portok = false;
				
				if (AllowAnyPort) portok = true;
				if (ToPorts != null)
				{
					foreach (int allowedport in ToPorts)
					{
						if (allowedport == port)
							portok = true;
					}
					if (!portok) return false;
				}
				//for now we only support socket policies that say all domains are fine.
                return (Domain == "*");
			}
			
            bool CheckDomain(Uri uri)
			{
				Log.Msg("Checking request-host: "+uri.Host+" against valid domain: "+Domain);
                if (Domain == "*") return true;
                if (ApplicationUri.Host == Domain) return true;

                if (Domain[0] != '*') return false;
                string match = Domain.Substring(1, Domain.Length - 1);
                if (uri.Host.EndsWith(match)) return true;
                
                return false;
            }
		}

		public class AllowHttpRequestHeadersFrom {

			public AllowHttpRequestHeadersFrom ()
			{
				Headers = new Headers ();
			}

			public string Domain { get; set; }
			public bool AllowAllHeaders { get; set; }
			public Headers Headers { get; private set; }
			public bool Secure { get; set; }

			public bool IsRejected (Uri uri, string [] headerKeys)
			{
				// "A Flash policy file must allow access to all domains to be used by the Silverlight runtime."
				// http://msdn.microsoft.com/en-us/library/cc645032(VS.95).aspx
				//if (Domain != "*")
				//	return false;

				if (Headers.IsAllowed (headerKeys))
					return false;

				// if Secure is false then it allows applications from HTTP to download data from HTTPS servers
				if (!Secure)
					return true;
				// if Secure is true then only application on HTTPS servers can access data on HTTPS servers
				if (ApplicationUri.Scheme == Uri.UriSchemeHttps)
					return (uri.Scheme == Uri.UriSchemeHttps);
				// otherwise FILE/HTTP applications can access HTTP uris
				return true;
			}
		}
	}
}

#endif
