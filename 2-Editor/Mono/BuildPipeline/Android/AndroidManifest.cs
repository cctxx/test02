using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;


/*
using System.Collections.Generic;
using System.Text;
using System.Xml;
*/
namespace UnityEditor
{
internal class AndroidManifest : AndroidXmlDocument
{
	public const string AndroidXmlNamespace = "http://schemas.android.com/apk/res/android";
	public const string AndroidManifestFile = "AndroidManifest.xml";

	public readonly XmlElement ApplicationElement;

	public AndroidManifest(string path) : base(path)
	{
		ApplicationElement = (XmlElement)(GetElementsByTagName("application")[0]);
	}

	public void SetPackageName(string packageName)
	{
		DocumentElement.SetAttribute("package", packageName);
	}

	public string GetPackageName()
	{
		return DocumentElement.GetAttribute("package");
	}

	public void SetVersion(string versionName, int versionCode)
	{
		DocumentElement.Attributes.Append(CreateAndroidAttribute("versionName", versionName));
		DocumentElement.Attributes.Append(CreateAndroidAttribute("versionCode", versionCode.ToString()));
	}

	public void SetInstallLocation(string location)
	{
		DocumentElement.Attributes.Append(CreateAndroidAttribute("installLocation", location));
	}

	public void SetDebuggable(bool debuggable)
	{
		ApplicationElement.Attributes.Append(CreateAndroidAttribute("debuggable", debuggable ? "true" : "false"));
	}

	public void AddGLESVersion(string glEsVersion)
	{
		var element = AppendElement(DocumentElement, "uses-feature", "android:glEsVersion");
		if (element != null)
			element.Attributes.Append(CreateAndroidAttribute("glEsVersion", glEsVersion));
	}

	public bool SetOrientation(string activity, string orientation)
	{
		XmlElement activityElement = GetActivity(activity);
		if (activityElement == null)
			return false;
		activityElement.Attributes.Append(CreateAndroidAttribute("screenOrientation", orientation));
		return true;
	}

	public bool RenameActivity(string src, string dst)
	{
		XmlElement activityElement = GetActivity(src);
		if (activityElement == null)
			return false;
		activityElement.Attributes.Append(CreateAndroidAttribute("name", dst));
		return true;
	}

	public XmlElement GetActivity(string name)
	{
		return GetElementWithAttribute(ApplicationElement, "activity", "android:name", name);
	}

	// http://developer.android.com/guide/topics/manifest/uses-feature-element.html
	public void AddUsesFeature(string feature, bool required)
	{
		var usesfeatElem = AppendTopAndroidNameTag("uses-feature", feature);
		// The default value for android:required if not declared is "true".
		if (usesfeatElem != null && !required)
			usesfeatElem.Attributes.Append(CreateAndroidAttribute("required", "false"));
	}

	public void AddUsesPermission(string permission)
	{
		AppendTopAndroidNameTag("uses-permission", permission);
	}

	public void AddSupportsGLTexture(string format)
	{
		AppendTopAndroidNameTag("supports-gl-texture", format);
	}

	public void AddUsesSDK(int minSdkVersion, int targetSdkVersion)
	{
		XmlElement sdkElement = null;
		XmlElement minSdkElement = GetElementWithAttribute(DocumentElement, "uses-sdk", "android:minSdkVersion");
		XmlElement targetSdkElement = GetElementWithAttribute(DocumentElement, "uses-sdk", "android:targetSdkVersion");
		if (minSdkElement != null && targetSdkElement != null)
			return;

		if (minSdkElement != null)
			sdkElement = minSdkElement;
		else if (targetSdkElement != null)
			sdkElement = targetSdkElement;
		else
			sdkElement = (XmlElement)DocumentElement.AppendChild(CreateElement("uses-sdk"));

		if (minSdkElement == null)
			sdkElement.Attributes.Append(CreateAndroidAttribute("minSdkVersion", minSdkVersion.ToString()));
		if (targetSdkElement == null)
			sdkElement.Attributes.Append(CreateAndroidAttribute("targetSdkVersion", targetSdkVersion.ToString()));
	}

	private XmlAttribute CreateAndroidAttribute(string key, string value)
	{
		XmlAttribute attr = CreateAttribute("android", key, AndroidXmlNamespace);
		attr.Value = value;
		return attr;
	}

	private XmlElement AppendTopAndroidNameTag(string tag, string value)
	{
		XmlElement elem = AppendElement(DocumentElement, tag, "android:name", value);
		if (elem == null)
			return elem;
		elem.Attributes.Append(CreateAndroidAttribute("name", value));
		return elem;
	}
}
}