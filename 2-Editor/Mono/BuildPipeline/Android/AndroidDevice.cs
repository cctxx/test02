using UnityEngine;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Text.RegularExpressions;

namespace UnityEditor.Android
{


// TODO: Parse result strings - ADB normally exits cleanly as long as no communication error has occured.
internal class AndroidDevice
{
	private const string ConsoleMessage   = "See the Console for more details. ";
	private const string SDKMessage       = "Please make sure the Android SDK is installed and is properly configured in the Editor. ";

	private const string PropertiesError  = "Unable to retrieve device properties. " + SDKMessage + ConsoleMessage;
	private const string MemInfoError     = "Unable to retrieve /proc/meminfo from device. " + SDKMessage + ConsoleMessage;
	private const string InstallError     = "Unable to install APK to device. " + SDKMessage + ConsoleMessage;
	private const string UninstallError   = "Unable to uninstall APK from device. " + SDKMessage + ConsoleMessage;
	private const string ForwardError     = "Unable to forward network traffic to device. " + SDKMessage + ConsoleMessage;
	private const string LaunchError      = "Unable to launch application. " + SDKMessage + ConsoleMessage;
	private const string PushError        = "Unable to push local file to device. " + SDKMessage + ConsoleMessage;
	private const string SetPropertyError = "Unable to set device property. " + SDKMessage + ConsoleMessage;

	private readonly string m_DeviceId;
	private readonly string m_ProductModel;
	private Dictionary<string, string> m_Properties;

	public string Id
	{
		get	{ return m_DeviceId; }
	}

	public string ProductModel
	{
		get { return m_ProductModel; }
	}

	public Dictionary<string,ulong> MemInfo
	{
		get
		{
			Dictionary<string, ulong> properties = new Dictionary<string, ulong>();
			Regex propertyMatcher = new Regex(@"^(.+?):\s*?(\d+).*$", RegexOptions.Multiline);
			foreach (Match match in propertyMatcher.Matches(Exec(new string[] {"shell", "cat", "/proc/meminfo"}, null, MemInfoError)))
				properties[match.Groups[1].Value] = ulong.Parse(match.Groups[2].Value) * 1024;
			return properties;
		}
	}

	public Dictionary<string,string> Properties
	{
		get
		{
			if (m_Properties != null)
				return m_Properties;

			Dictionary<string, string> properties = new Dictionary<string, string>();
			Regex propertyMatcher = new Regex(@"^\[(.+?)\]:\s*?\[(.*?)\].*$", RegexOptions.Multiline);
			foreach (Match match in propertyMatcher.Matches(Exec(new string[] {"shell", "getprop"}, null, PropertiesError)))
				properties[match.Groups[1].Value] = match.Groups[2].Value;
			return m_Properties = properties;
		}
	}

	public AndroidDevice(string deviceId)
	{
		m_DeviceId = deviceId;
		m_ProductModel = Properties["ro.product.model"];
	}

	public string Describe()
	{
		return string.Format("{0} [{1}]", Id, ProductModel);
	}

	public string Install(string apkfile, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return Exec(new string[] {"install", "-r", Quote(Path.GetFullPath(apkfile))}, waitingForProcessToExit, InstallError);
	}

	public string Uninstall(string package, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return Exec(new string[] {"uninstall", Quote(package)}, waitingForProcessToExit, UninstallError);
	}

	public string Forward(string pc, string device, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return Exec(new string[] {"forward", Quote(pc), Quote(device)}, waitingForProcessToExit, ForwardError);
	}

	public string Launch(string package, string activity, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return Exec(new string[] {"shell", "am", "start",
			"-a", "android.intent.action.MAIN",
			"-c", "android.intent.category.LAUNCHER",
			"-f", "0x10200000",
			"-n", Quote(string.Format("{0}/{1}", package, activity))},
			waitingForProcessToExit, LaunchError);
	}

	public string Push(string src, string dst, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return Exec(new string[] {"push", Quote(Path.GetFullPath(src)), Quote(dst)}, waitingForProcessToExit, PushError);
	}

	public string SetProperty(string key, string val, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		m_Properties = null;
		return Exec(new string[] {"shell", "setprop", Quote(key), Quote(val)}, waitingForProcessToExit, SetPropertyError);
	}

	private string Exec(string[] command, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		try
		{
			string[] adbCommand = new string[2 + command.Length];
			new string[]{"-s", m_DeviceId}.CopyTo(adbCommand, 0);
			command.CopyTo(adbCommand, 2);
			return ADB.Run(adbCommand, (program) =>
				{
					if (waitingForProcessToExit != null)
						waitingForProcessToExit(program);

					if (program == null)
						return;

					foreach (string stderr in program.GetErrorOutput())
					{
						if (!stderr.StartsWith("- waiting for device -"))
							continue;

						if (!EditorUtility.DisplayDialog("Communication lost", Describe() + ".\nTry reconnecting the USB cable.", "Retry", "Cancel"))
							throw new ProcessAbortedException("Devices connection lost");
						throw new RetryInvokationException();
					}
				}, errorMsg);
		}
	    catch (RetryInvokationException)
	    {
			return Exec(command, waitingForProcessToExit, errorMsg);
	    }
	}

	/*
	public static AndroidDevice First(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return AssertAnyDeviceReady(waitingForProcessToExit)[0];
	}

	public static AndroidDevice[] List(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		List<AndroidDevice> devices = new List<AndroidDevice>();
		foreach (string deviceId in ADB.Devices(waitingForProcessToExit))
			devices.Add(new AndroidDevice(deviceId));
		return devices.ToArray();
	}

	private static AndroidDevice[] AssertAnyDeviceReady(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		AndroidDevice[] devices = List(waitingForProcessToExit);
		if (devices.Length == 0)
			throw new Exception();
		return devices;
	}
	*/

	private class RetryInvokationException : Exception {}

	private static string Quote(string val)
	{
		return string.Format("\"{0}\"", val);
	}
}

}