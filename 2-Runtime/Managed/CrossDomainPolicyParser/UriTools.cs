using System;
using Uri = MonoForks.System.Uri;

namespace CrossDomainPolicyParser
{
	class UriTools
	{
		public static Uri MakeUri(string gameurl, string url)
		{
			if ((!url.ToLower().StartsWith("http://")) && (!url.ToLower().StartsWith("https://")) && (!url.ToLower().StartsWith("file://")))
				url = GetBaseUrl(gameurl) + "/" +url;
			Log.Msg("About to parse url: " + url);
			return new Uri(url);
		}
		static string GetBaseUrl(string url)
		{
			return url.Substring(0, url.LastIndexOf('/'));
		}
	}
}
