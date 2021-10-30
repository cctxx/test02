using UnityEngine;

using System;
using System.Collections.Generic;
using System.Threading;
using System.Text;

namespace UnityEditor.Android
{

internal class ADB
{
	private const string ConsoleMessage		= "See the Console for more details. ";
	private const string SDKMessage			= "Please make sure the Android SDK is installed and is properly configured in the Editor. ";

	private const string DevicesError		= "Unable to list connected devices. " + SDKMessage + ConsoleMessage;
	private const string KillServerError	= "Unable to kill the adb server. " + SDKMessage + ConsoleMessage;
	private const string StartServerError	= "Unable to start the adb server. " + SDKMessage + ConsoleMessage;

	public static List<string> Devices(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		int tries = 2;
		string status = "";
		List<string> devices = new List<string>();
		while (true)
		{
			status = Run(new []{"devices"}, waitingForProcessToExit, DevicesError);
			if(status.Contains("error")) // retry
				status = Run(new []{"devices"}, waitingForProcessToExit, DevicesError);

			foreach (string line in status.Split(new char[] {'\r', '\n'}))
				if (line.Trim().EndsWith("device") || line.Trim().EndsWith("offline"))
					devices.Add(line.Substring(0, line.IndexOf('\t')));

			if (devices.Count > 0)
				return devices;

			KillServer(waitingForProcessToExit);

			if (--tries == 0)
				break;

			Console.WriteLine(String.Format("ADB - No device found, will retry {0} time(s).", tries));
		}
		Debug.LogWarning("ADB - No device found - output:\n" + status);
		return devices;
	}

	public static void KillServer(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		RunInternal(new []{"kill-server"}, waitingForProcessToExit, KillServerError);
		for (int i = 0; i < 50; ++i) // wait 50*100 ms
		{
			waitingForProcessToExit(null);
			Thread.Sleep(100);
		}
	}

	public static void StartServer(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		AndroidSDKTools sdkTools = AndroidSDKTools.GetInstance();

		var p = new System.Diagnostics.Process
			{
				StartInfo =
				{
					FileName = sdkTools.ADB,
					Arguments = "start-server",
					WorkingDirectory = sdkTools.SDKRootDir,
					CreateNoWindow = true,
					StandardOutputEncoding = Encoding.UTF8,
					RedirectStandardInput = true,
					RedirectStandardError = true,
					RedirectStandardOutput = true,
					UseShellExecute = false
				}
			};
		try
		{
			p.Start();
			do
			{
				if (waitingForProcessToExit != null)
					waitingForProcessToExit(null);
			}
			while (!p.WaitForExit(100));
		}
		finally
		{
			p.Dispose();
		}
	}

	public static string Run(string[] command, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		// this is critical since the streams will hang forever is ADB server is
		// started as a side-effect of another command
		StartServer(waitingForProcessToExit);
		return RunInternal(command, waitingForProcessToExit, errorMsg);
	}

	private static string RunInternal(string[] command, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		AndroidSDKTools sdkTools = AndroidSDKTools.GetInstance();

		var psi = new System.Diagnostics.ProcessStartInfo();
		psi.FileName = sdkTools.ADB;
		psi.Arguments = string.Join(" ", command);
		psi.WorkingDirectory = sdkTools.SDKRootDir;
		psi.CreateNoWindow = true;
		psi.StandardOutputEncoding = Encoding.UTF8;
		return Command.Run(psi, waitingForProcessToExit, errorMsg);
	}
}

}