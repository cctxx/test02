//
// BaseDomainPolicy.cs
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
using MonoForks.System;
using System.Collections.Generic;
using System.IO;
using MonoForks.System.Net;
#if !TEST
using MonoForks.System.Windows.Interop;
#endif

// Base class for shared stuff between the Silverlight and Flash policies
// e.g. Headers and Domain comparison

namespace MonoForks.System.Windows.Browser.Net {

	abstract class BaseDomainPolicy : ICrossDomainPolicy {
#if TEST
		static public Uri ApplicationUri { get; set; }
#else
		static public Uri ApplicationUri {
			get { return PluginHost.RootUri; }
		}
#endif
		static string root;

		static public string ApplicationRoot {
			get {
				if (root == null)
					root = CrossDomainPolicyManager.GetRoot (ApplicationUri);
				return root;
			}
		}

		public class Headers {

			class PrefixComparer : IEqualityComparer<string> {

				public bool Equals (string x, string y)
				{
					int check_length = x.Length - 1;
					if ((x.Length > 0) && (x [check_length] == '*'))
					return (String.Compare (x, 0, y, 0, check_length, StringComparison.OrdinalIgnoreCase) == 0);

					return (String.Compare (x, y, StringComparison.OrdinalIgnoreCase) == 0);
				}

				public int GetHashCode (string obj)
				{
					return (obj == null) ? 0 : obj.GetHashCode ();
				}
			}

			static PrefixComparer pc = new PrefixComparer ();

			private List<string> list;

			public Headers ()
			{
			}

			public bool AllowAllHeaders { get; private set; }

			public bool IsAllowed (string[] headers)
			{
				if (AllowAllHeaders)
					return true;

				if (headers == null || headers.Length == 0)
					return true;

                foreach(var h in headers)
                {
                    bool found = false;
                    foreach(var item in list)
                        if (pc.Equals(item,h)) found = true;
                    if (!found) return false;
                }
				return true;
			}

			public void SetHeaders (string raw)
			{
				if (raw == "*") {
					AllowAllHeaders = true;
					list = null;
				} else if (raw != null) {
					string [] headers = raw.Split (',');
					list = new List<string> (headers.Length + 1);
					list.Add ("Content-Type");
					for (int i = 0; i < headers.Length; i++) {
						string s = headers [i].Trim ();
						if (!String.IsNullOrEmpty (s))
							list.Add (s);
					}
				} else {
					// without a specified 'http-request-headers' no header, expect Content-Type, is allowed
					AllowAllHeaders = false;
					list = new List<string> (1);
					list.Add ("Content-Type");
				}
			}
		}

		public bool IsAllowed (WebRequest request)
		{
   			var keys = request.Headers.Keys;
                        string[] AllKeys = new string[keys.Count];
                        keys.CopyTo(AllKeys,0);
                        return IsAllowed (request.RequestUri, AllKeys);
		}

		abstract public bool IsAllowed (Uri uri, params string [] headerKeys);
	}
}

#endif

