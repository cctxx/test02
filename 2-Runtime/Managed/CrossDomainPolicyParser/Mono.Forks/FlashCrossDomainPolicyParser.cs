//
// FlashCrossDomainPolicyParser.cs
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
using System.IO;
using System.Collections;
using System.Text;
using MonoForks.Mono.Xml;

/*

Specification: http://www.adobe.com/devnet/articles/crossdomain_policy_file_spec.html

# This grammar is based on the xsd from Adobe, but the schema is wrong.
# It should have used interleave (all). Some crossdomain.xml are invalidated.
# (For example, try mono-xmltool --validate-xsd http://www.adobe.com/xml/schemas/PolicyFile.xsd http://twitter.com/crossdomain.xml)

default namespace = ""

grammar {

start = cross-domain-policy

cross-domain-policy = element cross-domain-policy {
  element site-control {
    attribute permitted-cross-domain-policies {
      "all" | "by-contract-type" | "by-ftp-filename" | "master-only" | "none"
    }
  }?,
  element allow-access-from {
    attribute domain { text },
    attribute to-ports { text }?,
    attribute secure { xs:boolean }?
  }*,
  element allow-http-request-headers-from {
    attribute domain { text },
    attribute headers { text },
    attribute secure { xs:boolean }?
  }*,
  element allow-access-from-identity {
    element signatory {
      element certificate {
        attribute fingerprint { text },
        attribute fingerprint-algorithm { text }
      }
    }
  }*
}

}

*/

namespace MonoForks.System.Windows.Browser.Net
{

	partial class FlashCrossDomainPolicy {

		static public bool ReadBooleanAttribute(MiniParser.IAttrList attrs, string attribute)
		{
			switch (attrs.GetValue(attribute))
			{
				case null:
				case "true":
					return true;
				case "false":
					return false;
				default:
					throw new Exception("Invalid boolean attribute: " + attribute);
			}
		}
		class Reader : MiniParser.IReader
		{
			public Reader(Stream stream)
			{
				this.stream = stream;
			}
			public int Read()
			{
				return stream.ReadByte();
			}

			Stream stream;
		}

		static public FlashCrossDomainPolicy FromStream(Stream originalStream)
		{
			Log.Msg("received policy");

			var stream = StripTrailing0(originalStream);
			if (stream.Length == 0)
				throw new ArgumentException("Policy can't be constructed from empty stream.");

			FlashCrossDomainPolicy cdp = new FlashCrossDomainPolicy ();
			Handler handler = new Handler(cdp);
			Reader r = new Reader(stream);
			MiniParser p = new MiniParser();
			p.Parse(r, handler);
			
			// if none supplied set a default for headers
			if (cdp.AllowedHttpRequestHeaders.Count == 0)
			{
				var h = new AllowHttpRequestHeadersFrom() { Domain = "*", Secure = true };
				h.Headers.SetHeaders(null); // defaults
				cdp.AllowedHttpRequestHeaders.Add(h);
			}
			Log.Msg("done parsing policy");
			return cdp;
		}

		private static MemoryStream StripTrailing0(Stream stream)
		{
			//crazy inefficient, but only happens on very small streams.
			var dupe = new MemoryStream();
			while(true)
			{
				var b = stream.ReadByte();
				if (b==0 || b==-1) break;
				dupe.WriteByte((byte)b);
			}
			dupe.Seek(0, SeekOrigin.Begin);
			return dupe;
		}

		class Handler : MiniParser.IHandler
		{
			public Handler(FlashCrossDomainPolicy cdp)
			{
				this.cdp = cdp;
			}


			// IContentHandler

			private string current = null;
			private Stack stack = new Stack();
			FlashCrossDomainPolicy cdp;

			public void OnStartElement(string name, MiniParser.IAttrList attrs)
			{
				Log.Msg("Parsing: "+name);
				if (current == null)
				{
					if (name != "cross-domain-policy") throw new Exception("Expected root element to be <cross-domain-policy>. found "+name+" instead");
				}
				else
				{
					if (current == "cross-domain-policy")
					{
						switch (name)
						{
							case "site-control":
								cdp.SiteControl = attrs.GetValue("permitted-cross-domain-policies");
								break;
							case "allow-access-from":
								var a = new AllowAccessFrom()
								{
									Domain = attrs.GetValue("domain"),
									Secure = ReadBooleanAttribute(attrs, "secure")
								};
								
								var p = attrs.GetValue ("to-ports");
								if (p == "*")
									a.AllowAnyPort = true;
								else if (p != null)
								{
									var ports = new ArrayList();
									string[] ports_string =  p.Split (',');
									foreach(string portstring in ports_string)
									{
										if (portstring.Contains("-"))
										{
											string[] s = portstring.Split('-');
											if (s.Length != 2) continue;
											int startport;
											int endport;
											if (!Int32.TryParse(s[0], out startport)) continue;
											if (!Int32.TryParse(s[1], out endport)) continue;
											for (int i = startport; i != endport; i++)
											{
												ports.Add(i);
											}
										}
										else
										{
											int port;
											if (Int32.TryParse(portstring, out port))
												ports.Add(port);
										}
									}
									a.ToPorts = (int[]) ports.ToArray(typeof(int));
								}
								cdp.AllowedAccesses.Add(a);
								break;
							case "allow-http-request-headers-from":
								var h = new AllowHttpRequestHeadersFrom()
								{
									Domain = attrs.GetValue("domain"),
									Secure = ReadBooleanAttribute(attrs, "secure")
								};
								h.Headers.SetHeaders(attrs.GetValue("headers"));
								cdp.AllowedHttpRequestHeaders.Add(h);
								break;
							default:
								break;
						}
					}
					else
					{
						throw new Exception("Invalid element " + name);
					}
				}
				stack.Push(name);
				current = name;
				Log.Msg(name);
				// attributes
				int n = attrs.Length;
				for (int i = 0; i < n; i++)
					Log.Msg("  " + attrs.GetName(i) + ": " + attrs.GetValue(i));
			}

			public void OnEndElement(string name)
			{
				stack.Pop();
				current = stack.Count>0 ? (string)stack.Peek() : null;
			}

			public void OnStartParsing(MiniParser parser) { }
			public void OnChars(string ch) { }
			public void OnEndParsing(MiniParser parser) { }
		}
	}
}

#endif

