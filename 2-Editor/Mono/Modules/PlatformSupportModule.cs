using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEditor;
using UnityEngine;

namespace UnityEditor.Modules
{
	internal interface IPlatformSupportModule
	{
		/// Returns name identifying a target, for ex., Metro, note this name should match prefix for extension module UnityEditor.Metro.Extensions.dll, UnityEditor.Metro.Extensions.Native.dll
		string TargetName { get; }

		/// Returns the filename of jam which should be executed when you're recompiling extensions from Editor using CTRL + L shortcut, for ex., WP8EditorExtensions, MetroEditorExtensions, etc
		string JamTarget { get; }

		IBuildPostprocessor CreateBuildPostprocessor();

		// Return an instance of ISettingEditorExtension or null if not used
		// See DefaultPlayerSettingsEditorExtension.cs for abstract implementation
		ISettingEditorExtension CreateSettingsEditorExtension();

		// Return an instance of IPreferenceWindowExtension or null if not used
		IPreferenceWindowExtension CreatePreferenceWindowExtension();
	}

	// IMPORTANT: If you add new fields or properties in the interfaces below, always increment PlatformSupportModuleManager.API_VERSION !
	//            It's your duty to fix platform specific module with newly added properties/functions, even if those functions will be empty, 
	//            and don't forget to fix APIVersion.Version to match PlatformSupportModuleManager.API_VERSION
	internal interface IBuildPostprocessor
	{
		void LaunchPlayer(BuildLaunchPlayerArgs args);
		void PostProcess(BuildPostProcessArgs args);
		bool SupportsInstallInBuildFolder();

		// Return string.Empty if targeting a folder.
		string GetExtension();

		string[] FindPluginFilesToCopy(string basePluginFolder, out bool shouldRetainStructure);
	}


	// Extension point to add/alter the SettingsEditorWindow class
	internal interface ISettingEditorExtension
	{
		void OnEnable( PlayerSettingsEditor settingsEditor );

		bool HasPublishSection();

		// Leave blank if no contribution
		void PublishSectionGUI(float h, float midWidth, float maxWidth); 

		bool HasIdentificationGUI();

		// Leave blank if no contribution
		void IdentificationSectionGUI(); 
		
		// Leave blank if no contribution
		void ConfigurationSectionGUI();
			
		bool SupportsOrientation();

		void SplashSectionGUI();
	}


	// Extension point to add preferences to the PreferenceWindow class
	internal interface IPreferenceWindowExtension
	{
		// Called from PreferenceWindow whenever preferences should be read
		void ReadPreferences();
		
		// Called from PreferenceWindow whenever preferences should be written
		void WritePreferences();

		// True is this extension contributes an external application/tool preference(s)
		bool HasExternalApplications();

		// Called from OnGui - this function should draw any contributing UI components
		void ShowExternalApplications();
	}

	internal struct BuildLaunchPlayerArgs
	{
		public BuildTarget target;
		public string playerPackage;
		public string installPath;
		public string productName;
		public BuildOptions options;
	}

	internal struct BuildPostProcessArgs
	{
		public BuildTarget target;
		public string stagingArea;
		public string stagingAreaData;
		public string stagingAreaDataManaged;
		public string playerPackage;
		public string installPath;
		public string companyName;
		public string productName;
		public Guid productGUID;
		public BuildOptions options;
		internal RuntimeClassRegistry usedClassRegistry;
	}
}
