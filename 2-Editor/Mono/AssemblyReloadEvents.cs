using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Mono.Cecil;
using UnityEngine;
using System.IO;

namespace UnityEditor
{

// Add your static data here


internal class AssemblyReloadEvents
{
	// Called from C++
	public static void OnBeforeAssemblyReload ()
	{
		Security.ClearVerifiedAssemblies ();
	}

	// Called from C++
	public static void OnAfterAssemblyReload ()
	{
		// Repaint to ensure UI is initialized. We could have a situation where we wanted to create
		// a new script from the app menu bar while the object browser ui was not initialized.
		foreach (ProjectBrowser pb in ProjectBrowser.GetAllProjectBrowsers ())
			pb.Repaint ();
	}
}

}
