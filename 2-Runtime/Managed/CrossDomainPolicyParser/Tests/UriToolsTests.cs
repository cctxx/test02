using CrossDomainPolicyParser;
using MonoForks.System;
using NUnit.Framework;

namespace CrossDomainPolicyParserTests
{
	[TestFixture]
	public class UriToolsTests
	{
		[Test]
		public void MakeUriWorksForRelativeUri()
		{
			Uri uri = UriTools.MakeUri("http://mydomain.com/mygame.unity3d", "test.png");
			Assert.AreEqual("mydomain.com",uri.Host);
		}
		[Test]
		public void MakeUriWorksForAbsoluteUri()
		{
			Uri uri = UriTools.MakeUri("http://mydomain.com/mygame.unity3d", "http://www.google.com/test.png");
			Assert.AreEqual("www.google.com", uri.Host);
		}
	}
}
