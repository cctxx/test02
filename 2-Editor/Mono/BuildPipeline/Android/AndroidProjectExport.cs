using System;
using System.IO;
using System.Text;
using System.Xml;

using UnityEditor;

namespace UnityEditor.Android
{

internal class AndroidProjectExport
{
	private static readonly string DefaultUnityPackage = "com.unity3d.player";
	private static readonly string[] UnityActivities = new string[]
	{
		"UnityPlayerNativeActivity",
	};

	public static void Export(string playerPackage, string stagingArea,
								AndroidLibraries androidLibraries,
								string targetPath,
								string packageName, string productName,
								int platformApiLevel,
								bool useObb)
	{

		// Generate main project
		string mainProjectPath = Path.Combine(targetPath, productName);
		var playerJar	= Path.Combine(Path.Combine(playerPackage, "bin"), "classes.jar");
		var exportedJar	= Path.Combine(Path.Combine(mainProjectPath, "libs"), "unity-classes.jar");
		CopyFile(playerJar, exportedJar);

		CopyDir(Path.Combine(stagingArea, "res"),	Path.Combine(mainProjectPath, "res"));
		CopyDir(Path.Combine(stagingArea, "plugins"),	Path.Combine(mainProjectPath, "libs"));
		CopyDir(Path.Combine(stagingArea, "libs"),	Path.Combine(mainProjectPath, "libs"));
		CopyDir(Path.Combine(stagingArea, "assets"),Path.Combine(mainProjectPath, "assets"));

		GenerateProjectProperties(mainProjectPath, platformApiLevel, androidLibraries);
		GenerateAndroidManifest(mainProjectPath, stagingArea, packageName);
		CopyAndPatchJavaSources(mainProjectPath, playerPackage, packageName);

		// Generate (copy) library projects
		foreach (string libraryPath in androidLibraries)
			CopyDir(libraryPath, Path.Combine(targetPath, Path.GetFileName(libraryPath)));

		if (useObb)
		{
			// Copy OBB
			CopyFile(Path.Combine(stagingArea, "main.obb"), Path.Combine(targetPath, String.Format("{0}.main.obb", productName)));
		}
		else
		{
			// Add any streaming resources to the /assets folder
			// bug: there is currently no generic way to enforce uncompressed assets
			CopyDir(Path.Combine(stagingArea, "raw"), Path.Combine(mainProjectPath, "assets"));
		}
	}

	private static void GenerateProjectProperties(string targetPath, int platformApiLevel, AndroidLibraries androidLibraries)
	{
		int i = 1;
		string projectProperties = "target=android-" + platformApiLevel + "\n";
		foreach (string libraryPath in androidLibraries)
			projectProperties += "android.library.reference." + (i++) + "=../" + Path.GetFileName(libraryPath) + "\n";;
		File.WriteAllText(Path.Combine (targetPath, "project.properties"), projectProperties);
	}

	private static void GenerateAndroidManifest(string targetPath, string stagingArea, string packageName)
	{
		AndroidManifest manifestXML = new AndroidManifest(Path.Combine(stagingArea, AndroidManifest.AndroidManifestFile));
		foreach (string activityClassName in UnityActivities)
			manifestXML.RenameActivity(DefaultUnityPackage + "." + activityClassName, packageName + "." + activityClassName);
		manifestXML.SaveAs(Path.Combine(targetPath, AndroidManifest.AndroidManifestFile));
	}

	private static void CopyAndPatchJavaSources(string targetPath, string playerPackage, string packageName)
	{
		string sourcePackageDir = Path.Combine(Path.Combine(playerPackage, "src"), DefaultUnityPackage.Replace('.', '/'));
		string targetPackageDir = Path.Combine(Path.Combine(targetPath, "src"), packageName.Replace('.', '/'));
		Directory.CreateDirectory(targetPackageDir);
		foreach (string activityClassName in UnityActivities)
		{
			string targetFile = Path.Combine(targetPackageDir, activityClassName + ".java");
			string sourceFile = Path.Combine(sourcePackageDir, activityClassName + ".java");
			if (File.Exists(targetFile) || !File.Exists(sourceFile))
				continue;
			File.WriteAllText(targetFile, PatchJavaSource(File.ReadAllText(sourceFile), packageName));
		}
	}

	private static string PatchJavaSource(string javaSource, string packageName)
	{
		javaSource = javaSource.Replace("package " + DefaultUnityPackage, "package " + packageName);
		foreach (string activityClassName in UnityActivities)
			javaSource = javaSource.Replace(DefaultUnityPackage + "." + activityClassName, packageName + "." + activityClassName);
		javaSource = javaSource.Insert(javaSource.IndexOf("import"), "import com.unity3d.player.*;\n");
		return javaSource;
	}

	private static void CopyDir (string source, string target)
	{
		if (Directory.Exists(source))
		{
			Directory.CreateDirectory(Path.GetDirectoryName(target));
			FileUtil.CopyDirectoryRecursive(source, target, true);
		}
	}

	private static void CopyFile (string source, string target)
	{
		if (File.Exists(source))
		{
			Directory.CreateDirectory(Path.GetDirectoryName(target));
			FileUtil.UnityFileCopy(source, target, true);
		}
	}
}

}