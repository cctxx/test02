using System;
using System.Collections.Generic;
using MonoForks.System.Net;
using MonoForks.System;

namespace MonoForks.System.Windows.Interop
{
	public class PluginHost
	{
		static public Uri SourceUri
		{
			get
			{
				return new Uri(UnityEngine.UnityCrossDomainHelper.GetWebSecurityHostUri());
			}
		}
		static public Uri RootUri
		{
			get
			{
				return new Uri(GetRoot(SourceUri));
			}
		}

		private static string GetRoot(Uri uri)
		{
			if ((uri.Scheme == "http" && uri.Port == 80) || (uri.Scheme == "https" && uri.Port == 443) || (uri.Port == -1))
				return String.Format("{0}://{1}", uri.Scheme, uri.DnsSafeHost);
			else
				return String.Format("{0}://{1}:{2}", uri.Scheme, uri.DnsSafeHost, uri.Port);
		}

	}
}

namespace MonoForks.System.Net
{
	internal class WebRequest
	{
		public WebRequest(MonoForks.System.Uri requesturi, Dictionary<string,string> headers)
		{
			this.RequestUri = requesturi;
			this.Headers = headers;
		}
		public MonoForks.System.Uri RequestUri { get; set; } 
		public Dictionary<string,string> Headers { get; set; } 
	}
}
