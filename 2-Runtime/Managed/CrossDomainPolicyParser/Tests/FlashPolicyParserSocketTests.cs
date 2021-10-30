using System.IO;
using System.Text;
using MonoForks.System.Windows.Browser.Net;
using NUnit.Framework;

namespace CrossDomainPolicyParserTests
{
	[TestFixture]
	public class FlashPolicyParserSocketTests
	{
		[Test]
		public void AllDomains_AllPorts_IsAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""*"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 123));
		}

		[Test]
		public void AllDomains_AllPorts_Trailing0_IsAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""*"" />
</cross-domain-policy>" + "\0";
			Assert.IsTrue(RequestAllowed(policy, 123));
		}
		
		[Test]
		public void AllDomains_UsingSpecificPorts_IsAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1010,1020"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 1020));
		}
		
		[Test]
		public void AllDomains_OutsideSpecificPorts_IsDisAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1010,1030"" />
</cross-domain-policy>";
			Assert.IsFalse(RequestAllowed(policy, 1020));
		}
		
		[Test]
		public void AllDomains_OutsidePortRange_IsDisAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1030-1040"" />
</cross-domain-policy>";
			Assert.IsFalse(RequestAllowed(policy, 1020));
		}		
		
		[Test]
		public void AllDomains_InsidePortRange_IsAllowed()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1030-1040"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 1035));
		}

		[Test]
		public void PolicyReceivedFromHigherThan1024_DisallowsAccessToBelow1024Ports()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1000-1040"" />
</cross-domain-policy>";
			Assert.IsFalse(RequestAllowed(policy, 1010, 1300));
		}

		[Test]
		public void PolicyReceivedFromHigherThan1024_AllowsAccessToAbove1024Ports()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1000-1040"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 1035, 1300));
		}

		[Test]
		public void PolicyReceivedFromLowerThan1024_AllowsAccessToBelow1024Ports()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1000-1040"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 1010, 1000));
		}
		[Test]
		public void PolicyReceivedFromLowerThan1024_AllowsAccessToAbove1024Ports()
		{
			string policy = @"<?xml version='1.0'?>
<cross-domain-policy>
	<allow-access-from domain=""*"" to-ports=""1000-1040"" />
</cross-domain-policy>";
			Assert.IsTrue(RequestAllowed(policy, 1030, 1000));
		}

		private bool RequestAllowed(string xdomain, int port)
		{
			return RequestAllowed(xdomain, port, 843);
		}

		private bool RequestAllowed(string xdomain, int port, int policyport)
		{
			var ms = new MemoryStream(Encoding.UTF8.GetBytes(xdomain));
			var policy = FlashCrossDomainPolicy.FromStream(ms);
			policy.PolicyPort = policyport;
			return policy.IsSocketConnectionAllowed(port);
		}	
		
	}
}