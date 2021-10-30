using UnityEditor;

internal class PostProcessDashboardWidget
{
	public static void PostProcess(BuildTarget target, string installPath, string stagingArea, string playerPackage, string companyName, string productName, int width, int height)
	{
		string buildingContentsFolder = stagingArea + "/DashboardBuild";

		// Replace with new dashboard widget!
		FileUtil.DeleteFileOrDirectory(buildingContentsFolder);

		FileUtil.CopyFileOrDirectory(playerPackage, buildingContentsFolder);

		// Move data file
		FileUtil.MoveFileOrDirectory("Temp/unitystream.unity3d", buildingContentsFolder + "/widget.unity3d");

		// Install plugins	
		PostprocessBuildPlayer.InstallPlugins(buildingContentsFolder + "/Plugins", target);

		// Post process widget info plist and html site	
		string identifierName = PostprocessBuildPlayer.GenerateBundleIdentifier(companyName, productName) + ".widget";

		int widthPlus32 = width + 32;
		int heightPlus32 = height + 32;

		string[] fields = { "UNITY_WIDTH_PLUS32", widthPlus32.ToString(), "UNITY_HEIGHT_PLUS32", heightPlus32.ToString(), "UNITY_WIDTH", width.ToString(), "UNITY_HEIGHT", height.ToString(), "UNITY_BUNDLE_IDENTIFIER", identifierName, "UNITY_BUNDLE_NAME", productName };

		FileUtil.ReplaceText(buildingContentsFolder + "/UnityWidget.html", fields);
		FileUtil.ReplaceText(buildingContentsFolder + "/Info.plist", fields);

		FileUtil.DeleteFileOrDirectory(installPath);
		FileUtil.MoveFileOrDirectory(buildingContentsFolder, installPath);
	}

}