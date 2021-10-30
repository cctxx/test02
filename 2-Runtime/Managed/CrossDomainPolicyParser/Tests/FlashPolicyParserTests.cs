using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using MonoForks.Mono.Xml;
using MonoForks.System.Net;
using NUnit.Framework;
using MonoForks.System.Windows.Browser.Net;
using UnityEngine;
using Uri = MonoForks.System.Uri;

namespace CrossDomainPolicyParserTests
{
	[TestFixture]
	public class FlashPolicyParserTests
	{
		static string XDomainGlobal =
@"<?xml version=""1.0""?>
<!DOCTYPE cross-domain-policy SYSTEM ""http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd"">
<cross-domain-policy>
	<allow-access-from domain=""*"" />
</cross-domain-policy>";

		string http_hosted = "http://www.host.com/coolgame.unity3d";
		string https_hosted = "https://secure.host.net/coolgame.unity3d";
		string file_hosted = "file:///coolgame.unity3";

		[Test]
		public void GlobalXDomainAcceptsRequestOnSameDomain()
		{
			string requesturl = "http://www.mach8.nl/index.html";

			Assert.IsTrue(RequestAllowed(XDomainGlobal, requesturl, http_hosted));
		}
		[Test]
		public void GlobalXDomainAcceptsRequestOnSubDomain()
		{
			string requesturl = "http://subdomain.mach8.nl/index.html";

			Assert.IsTrue(RequestAllowed(XDomainGlobal, requesturl, http_hosted));
		}

		[Test]
		public void GlobalXDomainAllowsSecureRequestWhenHostedNonSecure()
		{
			string requesturl = "https://www.mach8.nl/index.html";

			Assert.IsTrue(RequestAllowed(XDomainGlobal, requesturl, http_hosted));
		}
		[Test]
		public void GlobalXDomainAcceptsSecureRequestWhenHostedSecure()
		{
			string requesturl = "https://www.mach8.nl/index.html";
			
			Assert.IsTrue(RequestAllowed(XDomainGlobal, requesturl, https_hosted));
		}
		[Test]
		public void GlobalXDomainDeniesNonSecureRequestWhenHostedSecure()
		{
			string requesturl = "http://www.mach8.nl/index.html";
			Assert.IsFalse(RequestAllowed(XDomainGlobal, requesturl, https_hosted));
		}

		[Test]
		public void AllDomain_Secure()
		{
			string policy = @"<?xml version=""1.0""?>
<!DOCTYPE cross-domain-policy SYSTEM ""http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd"">
<cross-domain-policy>
	<allow-access-from domain=""*"" secure=""true""/>
</cross-domain-policy>";

			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void WhenRequestURLMatchesWildCardAccessIsAllowed()
		{
			string policy = @"<?xml version=""1.0""?>
<cross-domain-policy>
	<allow-access-from domain=""*.mydomain.nl"" />
</cross-domain-policy>";

			Assert.IsTrue(RequestAllowed(policy, "http://subdomain.mydomain.nl", http_hosted));
		}

		[Test]
		public void WhenRequestURLDoesNotMatchWildCardAccessIsDisallowed()
		{
			string policy = @"<?xml version=""1.0""?>
<cross-domain-policy>
	<allow-access-from domain=""*.mydomain.nl"" />
</cross-domain-policy>";

			Assert.IsFalse(RequestAllowed(policy, "http://subdomain.myotherdomain.nl", http_hosted));
		}


		[Test]
		public void AllDomains_NoDTD()
		{
			string policy = @"<?xml version='1.0'?><cross-domain-policy><allow-access-from domain='*'/></cross-domain-policy>";

			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_NoXmlHeader()
		{
			string policy = @"<cross-domain-policy> 
	<allow-access-from domain=""*"" to-ports=""*""/> 
</cross-domain-policy> ";
			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_PermittedCrossDomainPolicies_All()
		{
			// 'all' is the default value
			// http://www.adobe.com/devnet/articles/crossdomain_policy_file_spec.html#site-control-permitted-cross-domain-policies
			string policy = @"<?xml version='1.0'?>
<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>
<cross-domain-policy>
	<site-control permitted-cross-domain-policies='all' />
	<allow-access-from domain='*' />
</cross-domain-policy>";

			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_PermittedCrossDomainPolicies_MasterOnly()
		{
			string policy = @"<?xml version='1.0'?>
<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>
<cross-domain-policy>
	<site-control permitted-cross-domain-policies='master-only' />
	<allow-access-from domain='*' />
</cross-domain-policy>";

			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_PermittedCrossDomainPolicies_None()
		{
			string policy = @"<?xml version='1.0'?>
<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>
<cross-domain-policy>
	<site-control permitted-cross-domain-policies='none' />
	<allow-access-from domain='*' />
</cross-domain-policy>";
			Assert.IsFalse(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_PermittedCrossDomainPolicies_ByContentType()
		{
			string policy = @"<?xml version='1.0'?>
<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>
<cross-domain-policy>
	<site-control permitted-cross-domain-policies='by-content-type' />
	<allow-access-from domain='*' />
</cross-domain-policy>";
			Assert.IsFalse(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		public void AllDomains_PermittedCrossDomainPolicies_ByFtpFilename()
		{
			string policy = @"<?xml version='1.0'?>
<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>
<cross-domain-policy>
	<site-control permitted-cross-domain-policies='by-ftp-filename' />
	<allow-access-from domain='*' />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, "http://www.host.com", http_hosted));
		}

		[Test]
		[ExpectedException(typeof(MiniParser.XMLError))]
		public void IllformedPolicyIsRejected()
		{
			FlashCrossDomainPolicyFromString("bogus", "http://www.host.com");
		}

		[Test]
		[ExpectedException(typeof(ArgumentException))]
		public void EmptyPolicyStringIsRejected()
		{
			FlashCrossDomainPolicyFromString("", "http://www.host.com");
		}

		private bool RequestAllowed(string xdomain, string requesturl, string hosturl)
		{
			FlashCrossDomainPolicy policy = FlashCrossDomainPolicyFromString(xdomain, hosturl);
			var wr = new WebRequest(new Uri(requesturl), new Dictionary<string, string>());
			return policy.IsAllowed(wr);
		}

		private FlashCrossDomainPolicy FlashCrossDomainPolicyFromString(string xdomain, string hosturl)
		{
			UnityCrossDomainHelper.SetWebSecurityHostUriDelegate(() => hosturl);

			var ms = new MemoryStream(Encoding.UTF8.GetBytes(xdomain));
			return FlashCrossDomainPolicy.FromStream(ms);
		}
	}
}
