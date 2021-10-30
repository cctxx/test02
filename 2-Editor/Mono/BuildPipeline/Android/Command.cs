using UnityEditor.Utils;

using System;
using System.Diagnostics;

namespace UnityEditor.Android
{

internal class CommandInvokationFailure : Exception
{
	public readonly string   HighLevelMessage;
	public readonly string   Command;
	public readonly int      ExitCode;
	public readonly string   Args;
	public readonly string[] StdOut;
	public readonly string[] StdErr;
	public override string   Message { get { return string.Format("{0}\n{1}\n\nstderr[\n{2}\n]\nstdout[\n{3}\n]", HighLevelMessage, CommandString(), StdErrString(), StdOutString()); } }
	public          string[]  Errors  { get; set; }

	public CommandInvokationFailure(string message, Program p)
	{
		HighLevelMessage = message;
		Command  = p.GetProcessStartInfo().FileName;
		Args     = p.GetProcessStartInfo().Arguments;
		ExitCode = p.ExitCode;
		StdOut   = p.GetStandardOutput();
		StdErr   = p.GetErrorOutput();
		Errors   = new string[]{};
	}

	public string CommandString()
	{
		return Command + " " + Args;
	}

	public string StdOutString()
	{
		return string.Join("\n", StdOut);
	}

	public string StdErrString()
	{
		return string.Join("\n", StdErr);
	}

	override public string ToString()
	{
		string userReadableMessage = HighLevelMessage;
		if (Errors != null)
		foreach (var error in Errors)
			userReadableMessage += "\n" + error;
		userReadableMessage += "\n\n" + base.ToString();
		return userReadableMessage;
	}
};

internal class Command
{
	public delegate void WaitingForProcessToExit(Program program);

	public static string Run(ProcessStartInfo psi, WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		UnityEditor.Utils.Program p = new UnityEditor.Utils.Program(psi);
		try
		{
			p.Start();
			do
			{
				if (waitingForProcessToExit != null)
					waitingForProcessToExit(p);
			}
			while (!p.WaitForExit(100));

			if (p.ExitCode != 0)
				throw new CommandInvokationFailure(errorMsg, p);

			var output = new System.Text.StringBuilder("");
			foreach (var line in p.GetStandardOutput())
				output.Append(line + Environment.NewLine);
			foreach (var line in p.GetErrorOutput())
				output.Append(line + Environment.NewLine);

			return output.ToString();
		}
		finally
		{
			p.Dispose();
		}
	}
}

}