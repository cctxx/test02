using UnityEngine;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor.VersionControl;

namespace UnityEditor
{
	internal class BuildPlayerWindow : EditorWindow
	{
        public class SceneSorter : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(System.Object x, System.Object y)
            {
                return ((new CaseInsensitiveComparer()).Compare(y, x));
            }

        }
        
        // All settings for a build platform.
		public class BuildPlatform
		{
			// short name used for texture settings, etc. 
			public string name;
			public GUIContent title;
			public Texture2D smallIcon;
			public BuildTargetGroup targetGroup;
			public bool forceShowTarget;
			
			public BuildPlatform(string locTitle, BuildTargetGroup targetGroup, bool forceShowTarget)
			{
				this.targetGroup = targetGroup;
				name = BuildPipeline.GetBuildTargetGroupName(DefaultTarget);
				title = EditorGUIUtility.TextContent (locTitle);
				smallIcon = EditorGUIUtility.IconContent(locTitle + ".Small").image as Texture2D;
					
				this.forceShowTarget = forceShowTarget;
			}

			public BuildTarget DefaultTarget
			{
				get
				{
					switch (targetGroup)
					{
						case BuildTargetGroup.Standalone:
							return BuildTarget.StandaloneWindows;
						case BuildTargetGroup.WebPlayer:
							return BuildTarget.WebPlayer;
						case BuildTargetGroup.iPhone:
							return BuildTarget.iPhone;
						case BuildTargetGroup.Android:
							return BuildTarget.Android;
						case BuildTargetGroup.BB10:
							return BuildTarget.BB10;
						case BuildTargetGroup.Tizen:
							return BuildTarget.Tizen;
                        case BuildTargetGroup.GLESEmu:
                            return BuildTarget.StandaloneGLESEmu;
                        case BuildTargetGroup.NaCl:
                            return BuildTarget.NaCl;
                        case BuildTargetGroup.Metro:
                            return BuildTarget.MetroPlayer;
						default:
							return (BuildTarget)(-1);
					}
				}
			}
		};
	
		class BuildPlatforms
		{
			internal BuildPlatforms()
			{
				// This is pretty brittle, notLicensedMessages and buildTargetNotInstalled below must match the order here
				// and since NaCl isn't listed in the build settings like the other platforms you must not add anything after it, if it
				// must also be added in the license/notinstalled arrays.
				List <BuildPlatform> buildPlatformsList = new List<BuildPlatform>();
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.Web", BuildTargetGroup.WebPlayer, true));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.Standalone", BuildTargetGroup.Standalone, true));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.iPhone", BuildTargetGroup.iPhone, true));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.Android", BuildTargetGroup.Android, true));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.BlackBerry", BuildTargetGroup.BB10, true));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.Tizen", BuildTargetGroup.Tizen, false));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.StandaloneGLESEmu", BuildTargetGroup.GLESEmu, false));
				buildPlatformsList.Add(new BuildPlatform ("BuildSettings.NaCl", BuildTargetGroup.NaCl, false));
                buildPlatforms = buildPlatformsList.ToArray();

                List<BuildTarget> standaloneSubtargetsList = new List<BuildTarget>();
				standaloneSubtargetsList.Add(BuildTarget.StandaloneWindows);
				standaloneSubtargetsList.Add(BuildTarget.StandaloneOSXIntel);
				standaloneSubtargetsList.Add (BuildTarget.StandaloneLinux);

				List<GUIContent> standaloneSubtargetStringsList = new List<GUIContent>();
				standaloneSubtargetStringsList.Add(EditorGUIUtility.TextContent ("BuildSettings.StandaloneWindows"));
				standaloneSubtargetStringsList.Add(EditorGUIUtility.TextContent ("BuildSettings.StandaloneOSXIntel"));
				standaloneSubtargetStringsList.Add(EditorGUIUtility.TextContent ("BuildSettings.StandaloneLinux"));

				standaloneSubtargets = standaloneSubtargetsList.ToArray();
				standaloneSubtargetStrings = standaloneSubtargetStringsList.ToArray();
			}

			public BuildPlatform[] buildPlatforms;

			public BuildTarget[] standaloneSubtargets;
			public GUIContent[] standaloneSubtargetStrings;


			public string GetBuildTargetDisplayName (BuildTarget target)
			{
				foreach (BuildPlatform cur in buildPlatforms)	
				{
					if (cur.DefaultTarget == target)
						return cur.title.text;
				}

				if (target == BuildTarget.WebPlayerStreamed)
					return BuildPlatformFromTargetGroup(BuildTargetGroup.WebPlayer).title.text;

				for (int i=0;i<standaloneSubtargets.Length;i++)
				{
					if (standaloneSubtargets[i] == DefaultTargetForPlatform (target))
						return standaloneSubtargetStrings[i].text;
				}
				
				return "Unsupported Target";
			}	
			
			public static Dictionary<GUIContent,BuildTarget> GetArchitecturesForPlatform (BuildTarget target)
			{
				switch (target) {
				case BuildTarget.StandaloneWindows:
				case BuildTarget.StandaloneWindows64:
					return new Dictionary<GUIContent,BuildTarget> () {
						{ EditorGUIUtility.TextContent ("x86"), BuildTarget.StandaloneWindows },
						{ EditorGUIUtility.TextContent ("x86_64"), BuildTarget.StandaloneWindows64 },
					};
				case BuildTarget.StandaloneLinux:
				case BuildTarget.StandaloneLinux64:
				case BuildTarget.StandaloneLinuxUniversal:
					return new Dictionary<GUIContent,BuildTarget> () {
						{ EditorGUIUtility.TextContent ("x86"), BuildTarget.StandaloneLinux },
						{ EditorGUIUtility.TextContent ("x86_64"), BuildTarget.StandaloneLinux64 },
						{ EditorGUIUtility.TextContent ("x86 + x86_64 (Universal)"), BuildTarget.StandaloneLinuxUniversal },
					};
				case BuildTarget.StandaloneOSXIntel:
				case BuildTarget.StandaloneOSXIntel64:
				case BuildTarget.StandaloneOSXUniversal:
					return new Dictionary<GUIContent,BuildTarget> () {
						{ EditorGUIUtility.TextContent ("x86"), BuildTarget.StandaloneOSXIntel },
						{ EditorGUIUtility.TextContent ("x86_64"), BuildTarget.StandaloneOSXIntel64 },
						{ EditorGUIUtility.TextContent ("Universal"), BuildTarget.StandaloneOSXUniversal },
					};
				default:
					return null;
				}
			}
			
			public static BuildTarget DefaultTargetForPlatform (BuildTarget target) {
				switch (target) {
				case BuildTarget.StandaloneWindows:
				case BuildTarget.StandaloneWindows64:
					return BuildTarget.StandaloneWindows;
				case BuildTarget.StandaloneLinux:
				case BuildTarget.StandaloneLinux64:
				case BuildTarget.StandaloneLinuxUniversal:
					return BuildTarget.StandaloneLinux;
				case BuildTarget.StandaloneOSXIntel:
				case BuildTarget.StandaloneOSXIntel64:
				case BuildTarget.StandaloneOSXUniversal:
					return BuildTarget.StandaloneOSXIntel;
				case BuildTarget.MetroPlayer:
					return BuildTarget.MetroPlayer;
				default:
					return target;
				}
			}

            public PS3BuildSubtarget[] ps3Subtargets = 
			{
				PS3BuildSubtarget.PCHosted, PS3BuildSubtarget.HddTitle, PS3BuildSubtarget.BluRayTitle
			};

            public GUIContent[] ps3SubtargetStrings = 
			{
				EditorGUIUtility.TextContent ("BuildSettings.PS3BuildSubtargetPCHosted"),
				EditorGUIUtility.TextContent ("BuildSettings.PS3BuildSubtargetHddTitle"),
				EditorGUIUtility.TextContent ("BuildSettings.PS3BuildSubtargetBluRayTitle"),
			};
		
			public WiiBuildSubtarget[] wiiBuildSubtargets =
			{
				WiiBuildSubtarget.DVD, WiiBuildSubtarget.WiiWare, WiiBuildSubtarget.DVDLibrary, WiiBuildSubtarget.WiiWareLibrary
			};
			public GUIContent[] wiiBuildSubtargetStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.WiiBuildSubtargetDVD"), 
				EditorGUIUtility.TextContent ("BuildSettings.WiiBuildSubtargetWiiWare"), 
				EditorGUIUtility.TextContent ("BuildSettings.WiiBuildSubtargetDVDLibrary"), 
				EditorGUIUtility.TextContent ("BuildSettings.WiiBuildSubtargetWiiWareLibrary"), 
			};

            public WiiBuildDebugLevel[] wiiBuildDebugLevels =
            {
                WiiBuildDebugLevel.Full, WiiBuildDebugLevel.Minimal, WiiBuildDebugLevel.None
            };
            public GUIContent[] wiiBuildDebugLevelStrings =
            {
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelFull"), 
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelMinimal"),
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelNone")
            };
            public GUIContent[] wiiBuildDebugLevelDescStrings =
            {
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelFullDesc"), 
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelMinimalDesc"),
                EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevelNoneDesc")
            };

			public FlashBuildSubtarget[] flashBuildSubtargets =
			{ 
				FlashBuildSubtarget.Flash11dot2, 
				FlashBuildSubtarget.Flash11dot3,
				FlashBuildSubtarget.Flash11dot4,
				FlashBuildSubtarget.Flash11dot5,
				FlashBuildSubtarget.Flash11dot6,
				FlashBuildSubtarget.Flash11dot7,
				FlashBuildSubtarget.Flash11dot8
			};

			public GUIContent[] flashBuildSubtargetString =
			{
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot2"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot3"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot4"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot5"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot6"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot7"),
				EditorGUIUtility.TextContent("BuildSettings.FlashSubtarget11dot8"),
			};
            public MetroBuildType[] metroBuildTypes =
            {
			    MetroBuildType.VisualStudioCppDX,
                MetroBuildType.VisualStudioCSharpDX,
				MetroBuildType.VisualStudioCpp,
				MetroBuildType.VisualStudioCSharp,
            };
            public GUIContent[] metroBuildTypeStrings =
            {
			    EditorGUIUtility.TextContent("BuildSettings.MetroBuildTypeVisualStudioCppDX"),
                EditorGUIUtility.TextContent("BuildSettings.MetroBuildTypeVisualStudioCSharpDX"),
                EditorGUIUtility.TextContent("BuildSettings.MetroBuildTypeVisualStudioCpp"),
				EditorGUIUtility.TextContent("BuildSettings.MetroBuildTypeVisualStudioCSharp")
            };

            public MetroSDK[] metroSDKs =
            {
			    MetroSDK.SDK80,
                MetroSDK.SDK81
            };
            public GUIContent[] metroSDKStrings =
            {
			    EditorGUIUtility.TextContent("BuildSettings.MetroSDK80"),
                EditorGUIUtility.TextContent("BuildSettings.MetroSDK81")
            };


			public XboxBuildSubtarget[] xboxBuildSubtargets =
			{
				XboxBuildSubtarget.Development, XboxBuildSubtarget.Master, XboxBuildSubtarget.Debug
			};
			public GUIContent[] xboxBuildSubtargetStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.XboxBuildSubtargetDevelopment"), 
				EditorGUIUtility.TextContent ("BuildSettings.XboxBuildSubtargetMaster"), 
				EditorGUIUtility.TextContent ("BuildSettings.XboxBuildSubtargetDebug"), 
			};
			public XboxRunMethod[] xboxRunMethods =
			{
				XboxRunMethod.HDD, XboxRunMethod.DiscEmuFast, XboxRunMethod.DiscEmuAccurate
			};
            public GUIContent[] xboxRunMethodStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.XboxRunMethodHDD"),
                EditorGUIUtility.TextContent ("BuildSettings.XboxRunMethodDiscEmuFast"),
                EditorGUIUtility.TextContent ("BuildSettings.XboxRunMethodDiscEmuAccurate"),
			};
			
			public AndroidBuildSubtarget[] androidBuildSubtargets =
			{
				AndroidBuildSubtarget.Generic,
                AndroidBuildSubtarget.DXT,
                AndroidBuildSubtarget.PVRTC,
                AndroidBuildSubtarget.ATC,
                AndroidBuildSubtarget.ETC,
                AndroidBuildSubtarget.ETC2,
                AndroidBuildSubtarget.ASTC
			};
			public GUIContent[] androidBuildSubtargetStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetGeneric"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetDXT"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetPVRTC"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetATC"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetETC"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetETC2"),
				EditorGUIUtility.TextContent ("BuildSettings.AndroidBuildSubtargetASTC"),
			};
			
			public BlackBerryBuildSubtarget[] blackberryBuildSubtargets =
			{
				BlackBerryBuildSubtarget.Generic, BlackBerryBuildSubtarget.PVRTC, BlackBerryBuildSubtarget.ATC, BlackBerryBuildSubtarget.ETC
			};
			public GUIContent[] blackberryBuildSubtargetStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildSubtargetGeneric"),
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildSubtargetPVRTC"),
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildSubtargetATC"),
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildSubtargetETC"),
			};
			
			public BlackBerryBuildType[] blackberryBuildTypes =
			{
				BlackBerryBuildType.Debug, BlackBerryBuildType.Submission
			};
			
			public GUIContent[] blackberryBuildTypeStrings =
			{
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildTypeDebug"),
				EditorGUIUtility.TextContent ("BuildSettings.BlackBerryBuildTypeSubmission"),
			};

			public int BuildPlatformIndexFromTargetGroup(BuildTargetGroup group)
			{
				for (int i = 0; i < buildPlatforms.Length; i++)
					if (group == buildPlatforms[i].targetGroup)
						return i;
				return -1;
			}

			public BuildPlatform BuildPlatformFromTargetGroup(BuildTargetGroup group)
			{
				int index = BuildPlatformIndexFromTargetGroup(group);
				return index != -1 ? buildPlatforms[index] : null;
			}
		};
		static BuildPlatforms s_BuildPlatforms;
		
		class Styles 
		{
			public GUIStyle selected = "ServerUpdateChangesetOn";
			public GUIStyle box = "OL Box";
			public GUIStyle title = "OL title";
			public GUIStyle evenRow = "CN EntryBackEven";
			public GUIStyle oddRow = "CN EntryBackOdd";
			public GUIStyle platformSelector = "PlayerSettingsPlatform";
			public GUIStyle toggle = "Toggle";
			public GUIStyle levelString = "PlayerSettingsLevel";
			public Vector2 toggleSize;
			
			public GUIContent noSessionDialogText = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.NoSessionDialogText");
			public GUIContent platformTitle = EditorGUIUtility.TextContent ("BuildSettings.PlatformTitle");
			public GUIContent switchPlatform = EditorGUIUtility.TextContent ("BuildSettings.SwitchPlatform");
			public GUIContent build = EditorGUIUtility.TextContent ("BuildSettings.Build");
			public GUIContent export = EditorGUIUtility.TextContent ("BuildSettings.Export");
			public GUIContent buildAndRun = EditorGUIUtility.TextContent ("BuildSettings.BuildAndRun");
			public GUIContent scenesInBuild = EditorGUIUtility.TextContent ("BuildSettings.ScenesInBuild");
	
			public Texture2D activePlatformIcon = EditorGUIUtility.IconContent("BuildSettings.SelectedIcon").image as Texture2D;

			public const float kButtonWidth = 110;

			// List of platforms that appear in the window. To add one, add it here.
			// Later on, we'll let the users add their own.
			const string kShopURL = "https://store.unity3d.com/shop/";
			const string kDownloadURL = "http://unity3d.com/unity/download/";
			const string kMailURL = "mailto:sales@unity3d.com";
			public GUIContent[,] notLicensedMessages = 
			{
				{ EditorGUIUtility.TextContent ("BuildSettings.NoWeb"), EditorGUIUtility.TextContent ("BuildSettings.NoWebButton"), new GUIContent (kShopURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.NoStandalone"), EditorGUIUtility.TextContent ("BuildSettings.NoWebButton"), new GUIContent (kShopURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.NoiPhone"), EditorGUIUtility.TextContent ("BuildSettings.NoiPhoneButton"), new GUIContent (kShopURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.NoAndroid"), EditorGUIUtility.TextContent ("BuildSettings.NoAndroidButton"), new GUIContent (kShopURL) },
				{ EditorGUIUtility.TextContent ("BuildSettings.NoBB10"), EditorGUIUtility.TextContent ("BuildSettings.NoBB10Button"), new GUIContent (kShopURL) },
				{ EditorGUIUtility.TextContent ("BuildSettings.NoTizen"), EditorGUIUtility.TextContent ("BuildSettings.NoTizenButton"), new GUIContent (kShopURL) },
				{ EditorGUIUtility.TextContent ("BuildSettings.NoGLESEmu"), EditorGUIUtility.TextContent ("BuildSettings.NoGLESEmuButton"), new GUIContent (kMailURL) },
			};
			
			private GUIContent[,] buildTargetNotInstalled = 
			{
				{ EditorGUIUtility.TextContent ("BuildSettings.WebNotInstalled"), null, new GUIContent (kDownloadURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.StandaloneNotInstalled"), null, new GUIContent (kDownloadURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.iPhoneNotInstalled"), null, new GUIContent (kDownloadURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.AndroidNotInstalled"), null, new GUIContent (kDownloadURL) },
				{ EditorGUIUtility.TextContent ("BuildSettings.BlackBerryNotInstalled"), null, new GUIContent (kDownloadURL) }, 
				{ EditorGUIUtility.TextContent ("BuildSettings.TizenNotInstalled"), null, new GUIContent (kDownloadURL) }, 
                { EditorGUIUtility.TextContent ("BuildSettings.GLESEmuNotInstalled"),  null, new GUIContent (kDownloadURL) },
			};
            public GUIContent GetTargetNotInstalled(int index, int item)
		    {
		        if (index >= buildTargetNotInstalled.GetLength (0)) index = 0;
                return buildTargetNotInstalled[index, item];
		    }
			
			public GUIContent GetDownloadErrorForTarget (BuildTarget target) {
				return null;			
			}

			// string and matching enum values for standalone subtarget dropdowm
            public GUIContent ps3Target = EditorGUIUtility.TextContent("BuildSettings.PS3BuildSubtarget");
			public GUIContent flashTarget = EditorGUIUtility.TextContent("BuildSettings.FlashBuildSubtarget");
            public GUIContent metroBuildType = EditorGUIUtility.TextContent("BuildSettings.MetroBuildType");
            public GUIContent metroSDK= EditorGUIUtility.TextContent("BuildSettings.MetroSDK");
			public GUIContent standaloneTarget = EditorGUIUtility.TextContent ("BuildSettings.StandaloneTarget");
			public GUIContent architecture = EditorGUIUtility.TextContent ("BuildSettings.Architecture");
			public GUIContent webPlayerStreamed = EditorGUIUtility.TextContent ("BuildSettings.WebPlayerStreamed");
            public GUIContent webPlayerOfflineDeployment = EditorGUIUtility.TextContent("BuildSettings.WebPlayerOfflineDeployment");
			public GUIContent debugBuild = EditorGUIUtility.TextContent("BuildSettings.DebugBuild");
			public GUIContent profileBuild = EditorGUIUtility.TextContent("BuildSettings.ConnectProfiler");
			public GUIContent allowDebugging = EditorGUIUtility.TextContent("BuildSettings.AllowDebugging");
			public GUIContent createProject = EditorGUIUtility.TextContent("BuildSettings.ExportAndroidProject");
			public GUIContent symlinkiOSLibraries = EditorGUIUtility.TextContent("BuildSettings.SymlinkiOSLibraries");
			public GUIContent wiiBuildSubtarget = EditorGUIUtility.TextContent("BuildSettings.WiiBuildSubtarget");
            public GUIContent wiiBuildDebugLevel = EditorGUIUtility.TextContent("BuildSettings.WiiBuildDebugLevel");
			public GUIContent xboxBuildSubtarget = EditorGUIUtility.TextContent("BuildSettings.XboxBuildSubtarget");
            public GUIContent xboxRunMethod = EditorGUIUtility.TextContent("BuildSettings.XboxRunMethod");
			public GUIContent androidBuildSubtarget = EditorGUIUtility.TextContent("BuildSettings.AndroidBuildSubtarget");
            public GUIContent explicitNullChecks = EditorGUIUtility.TextContent("BuildSettings.ExplicitNullChecks");
            public GUIContent enableHeadlessMode = EditorGUIUtility.TextContent("BuildSettings.EnableHeadlessMode");
			public GUIContent blackberryBuildSubtarget = EditorGUIUtility.TextContent("BuildSettings.BlackBerryBuildSubtarget");
			public GUIContent blackberryBuildType = EditorGUIUtility.TextContent("BuildSettings.BlackBerryBuildType");
		}
		
		class ScenePostprocessor : AssetPostprocessor
		{
			static void OnPostprocessAllAssets (string[] importedAssets, string[] deletedAssets, string[] movedAssets, string[] movedFromPath)
			{
				EditorBuildSettingsScene[] scenes = EditorBuildSettings.scenes;
				for (int i = 0; i < movedAssets.Length; i++)
				{
					string movedAsset = movedAssets[i];
					if (Path.GetExtension(movedAsset) == ".unity")
					{
						foreach (EditorBuildSettingsScene scene in scenes)
						{
							if (scene.path.ToLower() == movedFromPath[i].ToLower())
							{
								scene.path = movedAsset;
							}
						}
					}
				}
				EditorBuildSettings.scenes = scenes;
			}
		}

		ListViewState lv = new ListViewState();
		bool[] selectedLVItems = new bool[]{};
		bool[] selectedBeforeDrag;
		int initialSelectedLVItem = -1;

		Vector2 scrollPosition = new Vector2 (0,0);

		private const string kAssetsFolder = "Assets/";
		
		private const string kEditorBuildSettingsPath = "ProjectSettings/EditorBuildSettings.asset";

		static Styles styles = null;
		
		static void ShowBuildPlayerWindow ()
		{
            // Resetting the selected build target so it matches the active target when the build window is opened.
            // Helps to communicate which platform will be built (as we build the current selected platform).
            // Fixes bug 387526
            EditorUserBuildSettings.selectedBuildTargetGroup = BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget);
			EditorWindow.GetWindow<BuildPlayerWindow>(true, "Build Settings");
		}

		static void BuildPlayerAndRun ()
		{
			if (!BuildPlayerWithDefaultSettings (false, BuildOptions.AutoRunPlayer))
			{
				ShowBuildPlayerWindow ();
			}
		}

		static void BuildPlayerAndSelect ()
		{
			if (!BuildPlayerWithDefaultSettings (false, BuildOptions.ShowBuiltPlayer))
			{
				ShowBuildPlayerWindow ();
			}
		}

		public BuildPlayerWindow()
		{
			position = new Rect(50,50,540,530);
			minSize = new Vector2 (550,580);
			title = "Build Settings";
		}

		/**
		 * Make sure we're logged into the asset store and the issue a build with default settings.
		 */
		static bool EnsureLoggedInAndBuild(bool askForBuildLocation, BuildOptions forceOptions)
		{
			if (string.IsNullOrEmpty(UploadingBuildsMonitor.GetActiveSessionID()))
			{				
				AssetStoreLoginWindow.Login (
					styles.noSessionDialogText.text,
					(string errorMessage) => {
						if (string.IsNullOrEmpty (errorMessage))
						{
							BuildPlayerWithDefaultSettings(askForBuildLocation, forceOptions, false);
						}
						else
						{
							Debug.LogError (errorMessage);
						}
					}
				);
				return false;		
			}
			return true;
		}
		
		static bool BuildPlayerWithDefaultSettings (bool askForBuildLocation, BuildOptions forceOptions)
		{
			return BuildPlayerWithDefaultSettings(askForBuildLocation, forceOptions, true);
		}
        static bool IsMetroPlayer(BuildTarget target)
        {
            return target == BuildTarget.MetroPlayer;
        }
        static bool IsWP8Player(BuildTarget target)
        {
            return false;
        }

        static bool BuildPlayerWithDefaultSettings (bool askForBuildLocation, BuildOptions forceOptions, bool first)
		{			
			// If building and deploying a web player online then make sure we're logged in.
			if (first && EditorUserBuildSettings.webPlayerDeployOnline && 
				!EnsureLoggedInAndBuild(askForBuildLocation, forceOptions))
				return false;

			InitBuildPlatforms();

			BuildTarget buildTarget = CalculateSelectedBuildTarget();
			if (!BuildPipeline.IsBuildTargetSupported (buildTarget))
				return false;
				
			if (Unsupported.IsBleedingEdgeBuild())
			{
				var sb = new System.Text.StringBuilder();
				sb.AppendLine("This version of Unity is a BleedingEdge build that has not seen any manual testing.");
				sb.AppendLine("You should consider this build unstable.");
				sb.AppendLine("We strongly recommend that you use a normal version of Unity instead.");

				if (EditorUtility.DisplayDialog ("BleedingEdge Build", sb.ToString(), "Cancel","OK"))
					return false;

			}
			
			// Do some early validation if user intends to run
			if( BuildTarget.BB10 == buildTarget && ((forceOptions & BuildOptions.AutoRunPlayer) != 0))
			{
				if( string.IsNullOrEmpty(PlayerSettings.BlackBerry.authorId) ||
					string.IsNullOrEmpty(PlayerSettings.BlackBerry.deviceAddress) ||
					string.IsNullOrEmpty(PlayerSettings.BlackBerry.devicePassword) )
				{
					Debug.LogError (EditorGUIUtility.TextContent("BuildSettings.BlackBerryValidationFailed").text);
					return false;
				}
			}

			// Pick location for the build
			string newLocation = "";
			bool installInBuildFolder = EditorUserBuildSettings.installInBuildFolder && PostprocessBuildPlayer.SupportsInstallInBuildFolder (buildTarget) && (Unsupported.IsDeveloperBuild()
                || IsMetroPlayer(buildTarget) || IsWP8Player(buildTarget));
			if (!installInBuildFolder)
			{			
				if (EditorUserBuildSettings.selectedBuildTargetGroup == BuildTargetGroup.WebPlayer && EditorUserBuildSettings.webPlayerDeployOnline)
				{
					newLocation = Path.Combine (Path.Combine (Path.GetTempPath (), "Unity"), "UDNBuild");
					
					try
					{
						Directory.CreateDirectory (newLocation);
					}
					catch (System.Exception) {}
					
					if (!Directory.Exists (newLocation))
					{
						Debug.LogError (string.Format ("Failed to create temporary build directory at {0}", newLocation));
						return false;
					}
					
					newLocation = newLocation.Replace ('\\', '/');
				}
				else
				{
					if (askForBuildLocation && !PickBuildLocation (buildTarget))
						return false;
						
					newLocation = EditorUserBuildSettings.GetBuildLocation (buildTarget);					
				}

				if (newLocation.Length == 0)
				{
					return false;
				}
											
				if (!askForBuildLocation)
				{
					switch (UnityEditorInternal.InternalEditorUtility.BuildCanBeAppended(buildTarget, newLocation))
					{
						case CanAppendBuild.Unsupported:
							break;
						case CanAppendBuild.Yes:
							EditorUserBuildSettings.appendProject = true;
							break;
						case CanAppendBuild.No:
							if (!PickBuildLocation (buildTarget))
								return false;

							newLocation = EditorUserBuildSettings.GetBuildLocation (buildTarget);
							if (newLocation.Length == 0 || !System.IO.Directory.Exists (FileUtil.DeleteLastPathNameComponent (newLocation)))
								return false;

							break;
					}
				}
			}
			
			// Build a list of scenes that are enabled
			ArrayList scenesList = new ArrayList ();
			EditorBuildSettingsScene[] editorScenes = EditorBuildSettings.scenes;
			foreach (EditorBuildSettingsScene scene in editorScenes)
			{
				if (scene.enabled)
					scenesList.Add(scene.path);	
			}

			string[] scenes = scenesList.ToArray(typeof(string)) as string[];

			BuildOptions options = forceOptions;

			if (EditorUserBuildSettings.development)
				options |= BuildOptions.Development;
            if (EditorUserBuildSettings.connectProfiler && (EditorUserBuildSettings.development || buildTarget == BuildTarget.MetroPlayer))
				options |= BuildOptions.ConnectWithProfiler;
			if (EditorUserBuildSettings.allowDebugging && EditorUserBuildSettings.development)
				options |= BuildOptions.AllowDebugging;
			if (EditorUserBuildSettings.symlinkLibraries)
				options |= BuildOptions.SymlinkLibraries;
			if (EditorUserBuildSettings.appendProject)
				options |= BuildOptions.AcceptExternalModificationsToPlayer;
			if (EditorUserBuildSettings.webPlayerDeployOnline)
				options |= BuildOptions.DeployOnline;
            if (EditorUserBuildSettings.webPlayerOfflineDeployment)
                options |= BuildOptions.WebPlayerOfflineDeployment;
            if (EditorUserBuildSettings.enableHeadlessMode)
                options |= BuildOptions.EnableHeadlessMode;
			if (installInBuildFolder)
			{
				options |= BuildOptions.InstallInBuildFolder;
			}

			// See if we need to switch platforms and delay the build.  We do this whenever
			// we're trying to build for a target different from the active one so as to ensure
			// that the compiled script code we have loaded is built for the same platform we
			// are building for.  As we can't reload while our editor stuff is still executing,
			// we need to defer to after the next script reload then.
			bool delayToAfterScriptReload = false;
			if (EditorUserBuildSettings.activeBuildTarget != buildTarget)
			{
				if (!EditorUserBuildSettings.SwitchActiveBuildTarget(buildTarget))
				{
					// Switching the build target failed.  No point in trying to continue
					// with a build.
					Debug.LogError(string.Format("Could not switch to build target '{0}'.",
					                             s_BuildPlatforms.GetBuildTargetDisplayName(buildTarget)));
					return false;
				}

				if (EditorApplication.isCompiling)
					delayToAfterScriptReload = true;
			}

			// Trigger build.
			uint crc = 0;
			string error = BuildPipeline.BuildPlayerInternalNoCheck(scenes, newLocation, buildTarget, options, delayToAfterScriptReload, out crc);

			return error.Length == 0;
		}

        void ActiveScenesGUI()
		{
			int i, index;
			int enabledCounter = 0;
			int prevSelectedRow = lv.row;
			bool shiftIsDown = Event.current.shift;
			bool ctrlIsDown = EditorGUI.actionKey;

			Event evt = Event.current;

			Rect scenesInBuildRect = GUILayoutUtility.GetRect(styles.scenesInBuild, styles.title);
			ArrayList scenes = new ArrayList(EditorBuildSettings.scenes);
		
			lv.totalRows = scenes.Count;
			
			if (selectedLVItems.Length != scenes.Count)
			{
				System.Array.Resize(ref selectedLVItems, scenes.Count);
			}

			int[] enabledCount = new int[scenes.Count];
			for (i=0;i<enabledCount.Length;i++)
			{
				EditorBuildSettingsScene scene = (EditorBuildSettingsScene)scenes[i];
				enabledCount[i] = enabledCounter;
				if (scene.enabled)
					enabledCounter++;
			}

			foreach (ListViewElement el in ListViewGUILayout.ListView (lv, 
				ListViewOptions.wantsReordering | ListViewOptions.wantsExternalFiles, styles.box))
			{
				EditorBuildSettingsScene scene = (EditorBuildSettingsScene)scenes[el.row];
				
				var sceneExists = File.Exists(scene.path);
				EditorGUI.BeginDisabledGroup (!sceneExists);

				bool selected = selectedLVItems[el.row];
				if (selected && evt.type == EventType.Repaint)
					styles.selected.Draw(el.position, false, false, false, false);

				if (!sceneExists)
					scene.enabled = false;
				Rect toggleRect = new Rect(el.position.x + 4, el.position.y, styles.toggleSize.x, styles.toggleSize.y);
				EditorGUI.BeginChangeCheck ();
				scene.enabled = GUI.Toggle(toggleRect, scene.enabled, "");
				if (EditorGUI.EndChangeCheck () && selected)
				{
					// Set all selected scenes to the same state as current scene
					for (int j=0; j<scenes.Count; ++j)
						if (selectedLVItems[j])
							((EditorBuildSettingsScene)scenes[j]).enabled = scene.enabled;
				}

				GUILayout.Space(styles.toggleSize.x);

				string nicePath = scene.path;
				if (nicePath.StartsWith (kAssetsFolder))
					nicePath = nicePath.Substring (kAssetsFolder.Length);
	
				Rect r = GUILayoutUtility.GetRect (EditorGUIUtility.TempContent (nicePath), styles.levelString);
				if (Event.current.type == EventType.Repaint)
					styles.levelString.Draw (r, EditorGUIUtility.TempContent (nicePath), false, false, selected, false);
				
				if (scene.enabled)
					GUI.Label(new Rect(r.xMax - 18, r.y, 18, r.height), enabledCount[el.row].ToString());

				EditorGUI.EndDisabledGroup ();

				if (ListViewGUILayout.HasMouseUp(el.position) && !shiftIsDown && !ctrlIsDown)
				{
					if (!shiftIsDown && !ctrlIsDown)
						ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref initialSelectedLVItem, ref selectedLVItems);
				}
				else if (ListViewGUILayout.HasMouseDown(el.position))
				{
					if (!selectedLVItems[el.row] || shiftIsDown || ctrlIsDown)
						ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref initialSelectedLVItem, ref selectedLVItems);

					lv.row = el.row;

					selectedBeforeDrag = new bool[selectedLVItems.Length];
					selectedLVItems.CopyTo(selectedBeforeDrag, 0);
					selectedBeforeDrag[lv.row] = true;
				}
			}

			GUI.Label(scenesInBuildRect, styles.scenesInBuild, styles.title);

			// "Select All"
			if (GUIUtility.keyboardControl == lv.ID)
			{
				if (Event.current.type == EventType.ValidateCommand && Event.current.commandName == "SelectAll")
				{
					Event.current.Use();
				}
				else
					if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "SelectAll")
					{
						for (i = 0; i < selectedLVItems.Length; i++)
							selectedLVItems[i] = true;

						lv.selectionChanged = true;

						Event.current.Use();
						GUIUtility.ExitGUI();
					}
			}

			if (lv.selectionChanged)
			{
				ListViewGUILayout.MultiSelection(prevSelectedRow, lv.row, ref initialSelectedLVItem, ref selectedLVItems);
			}

			// external file(s) is dragged in
			if (lv.fileNames != null)
			{
				System.Array.Sort (lv.fileNames);
				int k = 0;
				for (i = 0; i < lv.fileNames.Length; i++)
				{
					if (lv.fileNames[i].EndsWith("unity"))
					{
						EditorBuildSettingsScene newScene = new EditorBuildSettingsScene();
						newScene.path = FileUtil.GetProjectRelativePath(lv.fileNames[i]);
					
						if (newScene.path == string.Empty) // it was relative already
							newScene.path = lv.fileNames[i];
						
						newScene.enabled = true;
						scenes.Insert(lv.draggedTo + (k++), newScene);
					}
				}
                

				if (k != 0)
				{
					System.Array.Resize(ref selectedLVItems, scenes.Count);

					for (i = 0; i < selectedLVItems.Length; i++)
						selectedLVItems[i] = (i >= lv.draggedTo) && (i < lv.draggedTo + k);
				}

				lv.draggedTo = -1;
			}

			if (lv.draggedTo != -1)
			{
				ArrayList selectedScenes = new ArrayList();

				// First pick out selected items from array
				index = 0;
				for (i = 0; i < selectedLVItems.Length; i++, index++)
				{
					if (selectedBeforeDrag[i])
					{
						selectedScenes.Add(scenes[index]);
						scenes.RemoveAt(index);
						index--;

						if (lv.draggedTo >= i)
							lv.draggedTo--;
					}
				}

				lv.draggedTo = (lv.draggedTo > scenes.Count) || (lv.draggedTo < 0) ? scenes.Count : lv.draggedTo;

				// Add selected items into dragged position
				scenes.InsertRange(lv.draggedTo, selectedScenes);

				for (i = 0; i < selectedLVItems.Length; i++)
					selectedLVItems[i] = (i >= lv.draggedTo) && (i < lv.draggedTo + selectedScenes.Count);
			}

			if (evt.type == EventType.KeyDown && (evt.keyCode == KeyCode.Backspace || evt.keyCode == KeyCode.Delete))
			{
				index = 0;
				for (i = 0; i < selectedLVItems.Length; i++, index++)
				{
					if (selectedLVItems[i])
					{
						scenes.RemoveAt(index);
						index--;
					}

					selectedLVItems[i] = false;
				}

				lv.row = -1;

				evt.Use();
			}

			EditorBuildSettings.scenes = scenes.ToArray(typeof(EditorBuildSettingsScene)) as EditorBuildSettingsScene[];
		}

		void AddCurrentScene ()
		{
			string currentScene = EditorApplication.currentScene;
			if (currentScene.Length == 0)
			{
				EditorApplication.SaveCurrentSceneIfUserWantsToForce ();
				currentScene = EditorApplication.currentScene;
			}
		
			if (currentScene.Length != 0)
			{
				ArrayList list = new ArrayList (EditorBuildSettings.scenes);

				EditorBuildSettingsScene scene = new EditorBuildSettingsScene ();
				scene.path = currentScene;
				
				scene.enabled = true;
				list.Add(scene);
				
			
				EditorBuildSettings.scenes = list.ToArray(typeof(EditorBuildSettingsScene)) as EditorBuildSettingsScene[];
			}

			Repaint();
			GUIUtility.ExitGUI();
		}

		static BuildTarget CalculateSelectedBuildTarget()
		{
			BuildTargetGroup targetGroup = EditorUserBuildSettings.selectedBuildTargetGroup;
			switch (targetGroup)
			{
				case BuildTargetGroup.WebPlayer:
						return EditorUserBuildSettings.webPlayerStreamed ? BuildTarget.WebPlayerStreamed : BuildTarget.WebPlayer;
				case BuildTargetGroup.Standalone:
					return EditorUserBuildSettings.selectedStandaloneTarget;
				default:
					if (s_BuildPlatforms == null)
						throw new System.Exception("Build platforms are not initialized.");
					BuildPlatform platform = s_BuildPlatforms.BuildPlatformFromTargetGroup(targetGroup);
					if (platform == null)
						throw new System.Exception("Could not find build platform for target group " + targetGroup);
					return platform.DefaultTarget;
			}		
		}

		private void ActiveBuildTargetsGUI ()
		{
			GUILayout.BeginVertical ();
			GUILayout.BeginVertical (GUILayout.Width(255));
			GUILayout.Label (styles.platformTitle, styles.title);
			scrollPosition = GUILayout.BeginScrollView(scrollPosition, "OL Box");
			
			// Draw enabled build targets first, then draw disabled build targets
			for (int requireEnabled = 0; requireEnabled < 2; requireEnabled++) 
			{
				bool showRequired = requireEnabled == 0;
				bool even = false;
				foreach (BuildPlatform gt in s_BuildPlatforms.buildPlatforms)
				{
					if (IsBuildTargetGroupSupported (gt.DefaultTarget) != showRequired)
						continue;
					
					// Some build targets are not publicly available, show them only when they are actually in use
					if (!IsBuildTargetGroupSupported (gt.DefaultTarget) && !gt.forceShowTarget)
						continue;
						
					ShowOption (gt, gt.title, even ? styles.evenRow : styles.oddRow);
					even = !even;
				}
				GUI.contentColor = Color.white;
			}
			
			GUILayout.EndScrollView();
			GUILayout.EndVertical ();
			GUILayout.Space (10);

			// Switching build target in the editor
			BuildTarget selectedTarget = CalculateSelectedBuildTarget();
			
			
			GUILayout.BeginHorizontal ();
			
			GUI.enabled = BuildPipeline.IsBuildTargetSupported(selectedTarget) && BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget) != BuildPipeline.GetBuildTargetGroup(selectedTarget);
			if (GUILayout.Button (styles.switchPlatform, GUILayout.Width (Styles.kButtonWidth)))
			{
				EditorUserBuildSettings.SwitchActiveBuildTarget (selectedTarget);
			}
			
			GUI.enabled = BuildPipeline.IsBuildTargetSupported(selectedTarget);
			if (GUILayout.Button (new GUIContent("Player Settings..."), GUILayout.Width (Styles.kButtonWidth)))
			{
				Selection.activeObject = Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings");
			}
			
			GUILayout.EndHorizontal ();
			
			GUI.enabled = true;
			
			GUILayout.EndVertical();
		}

		void ShowOption (BuildPlatform bp, GUIContent title, GUIStyle background)
		{	
			Rect r = GUILayoutUtility.GetRect (50, 36);
			r.x += 1;
			r.y += 1;
			bool valid = BuildPipeline.LicenseCheck(bp.DefaultTarget);
			GUI.contentColor = new Color (1,1,1, valid ? 1 : .7f);
			bool enabled = EditorUserBuildSettings.selectedBuildTargetGroup == bp.targetGroup;
			if (Event.current.type == EventType.Repaint)
			{
				background.Draw (r, GUIContent.none, false, false, enabled, false);
				GUI.Label (new Rect (r.x + 3, r.y + 3, 32, 32), title.image, GUIStyle.none);

				if (BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget) == bp.targetGroup)
					GUI.Label(new Rect(r.xMax - styles.activePlatformIcon.width - 8, r.y + 3 + (32 - styles.activePlatformIcon.height) / 2,
						styles.activePlatformIcon.width, styles.activePlatformIcon.height), 
						styles.activePlatformIcon, GUIStyle.none);
			}

			if (GUI.Toggle(r, enabled, title.text, styles.platformSelector))
			{
				if (EditorUserBuildSettings.selectedBuildTargetGroup != bp.targetGroup)
				{
					EditorUserBuildSettings.selectedBuildTargetGroup = bp.targetGroup;
					
					// Repaint inspectors, as they may be showing platform target specific things.
					Object[] inspectors = Resources.FindObjectsOfTypeAll(typeof(InspectorWindow));
					for (int i=0; i<inspectors.Length; i++)
					{
						InspectorWindow inspector = inspectors[i] as InspectorWindow;
						if (inspector != null)
							inspector.Repaint();
					}
				}
			}
		}

		void OnGUI () 
		{
			if (styles == null)
			{
				styles = new Styles();
				styles.toggleSize = styles.toggle.CalcSize(new GUIContent("X"));
				lv.rowHeight = (int)styles.levelString.CalcHeight(new GUIContent("X"), 100);
			}

			InitBuildPlatforms();

			GUILayout.BeginHorizontal ();
			GUILayout.Space (10);
			GUILayout.BeginVertical ();
			
			string message = "";
			var buildSettingsLocked = !AssetDatabase.IsOpenForEdit (kEditorBuildSettingsPath, out message);
			
			EditorGUI.BeginDisabledGroup (buildSettingsLocked); {
			
				ActiveScenesGUI ();
				// Clear all and Add Current Scene
				GUILayout.BeginHorizontal ();
				if (buildSettingsLocked)
				{
					GUI.enabled = true;

					if (Provider.enabled && GUILayout.Button ("Checkout"))
					{
						Asset asset = Provider.GetAssetByPath (kEditorBuildSettingsPath);
						var assetList = new AssetList ();
						assetList.Add (asset);
						Task checkoutTask = Provider.Checkout (assetList, CheckoutMode.Both);
						checkoutTask.SetCompletionAction(CompletionAction.UpdatePendingWindow);
					}
					GUILayout.Label (message);
					GUI.enabled = false;
				}
				GUILayout.FlexibleSpace ();
				if (GUILayout.Button ("Add Current"))
					AddCurrentScene ();
				GUILayout.EndHorizontal ();
			
			} EditorGUI.EndDisabledGroup ();

			GUILayout.Space (10);

			GUILayout.BeginHorizontal (GUILayout.Height (301));
			ActiveBuildTargetsGUI ();
			GUILayout.Space(10);
			GUILayout.BeginVertical ();
			ShowBuildTargetSettings ();
			GUILayout.EndVertical ();
			GUILayout.EndHorizontal ();
			
			GUILayout.Space (10);
			GUILayout.EndVertical();
			GUILayout.Space (10);
			GUILayout.EndHorizontal ();
		}

		static BuildTarget RestoreLastKnownPlatformsBuildTarget(BuildPlatform bp) 
		{
			switch (bp.targetGroup)
			{
			case BuildTargetGroup.WebPlayer:
					return EditorUserBuildSettings.webPlayerStreamed ? BuildTarget.WebPlayerStreamed : BuildTarget.WebPlayer;
			case BuildTargetGroup.Standalone:
				return EditorUserBuildSettings.selectedStandaloneTarget;
			default:
				return bp.DefaultTarget;
			}		
		}

		static void InitBuildPlatforms()
		{
			if (s_BuildPlatforms == null)
			{
				s_BuildPlatforms = new BuildPlatforms();
				RepairSelectedBuildTargetGroup();
			}
		}

		internal static List<BuildPlatform> GetValidPlatforms () 
		{
			InitBuildPlatforms();

			List<BuildPlatform> platforms = new List<BuildPlatform>();
			foreach (BuildPlatform bp in s_BuildPlatforms.buildPlatforms)
				if (bp.targetGroup == BuildTargetGroup.Standalone || BuildPipeline.IsBuildTargetSupported (bp.DefaultTarget))
					platforms.Add (bp);
					
			return platforms;
		}

		
		internal static bool IsBuildTargetGroupSupported (BuildTarget target)
		{
			if (target == BuildTarget.StandaloneWindows)
				return true;
			else
				return BuildPipeline.IsBuildTargetSupported(target);
		}

		internal static bool IsXboxBuildSubtargetDevelopment(XboxBuildSubtarget target)
		{
			return (target != XboxBuildSubtarget.Master);
		}

		static void RepairSelectedBuildTargetGroup ()
		{
			BuildTargetGroup group = EditorUserBuildSettings.selectedBuildTargetGroup;
			if ((int)group == 0 || s_BuildPlatforms == null || s_BuildPlatforms.BuildPlatformIndexFromTargetGroup(group) < 0)
				EditorUserBuildSettings.selectedBuildTargetGroup = BuildTargetGroup.WebPlayer;

		}
		
		void ShowBuildTargetSettings () 
		{
			EditorGUIUtility.labelWidth = Mathf.Min (180, (position.width - 265) * 0.47f);
			
			BuildTarget buildTarget = CalculateSelectedBuildTarget();
			BuildPlatform platform = s_BuildPlatforms.BuildPlatformFromTargetGroup(EditorUserBuildSettings.selectedBuildTargetGroup);

			// Draw the group name
			GUILayout.Space (18);
			
			// Draw icon and text of title separately so we can control the space between them
			Rect r = GUILayoutUtility.GetRect (50, 36);
			r.x += 1;
			GUI.Label (new Rect (r.x + 3, r.y + 3, 32, 32), platform.title.image, GUIStyle.none);
			GUI.Toggle(r, false, platform.title.text, styles.platformSelector);

			GUILayout.Space (10);

			GUIContent error = styles.GetDownloadErrorForTarget(buildTarget);
			if (error != null)
			{
				GUILayout.Label (error, EditorStyles.wordWrappedLabel);
				GUIBuildButtons (false, false, false, platform);
				return;
			}

			// Draw not licensed buy now UI
			if (!BuildPipeline.LicenseCheck(buildTarget))
			{
				int targetGroup = s_BuildPlatforms.BuildPlatformIndexFromTargetGroup(platform.targetGroup);

				GUILayout.Label(styles.notLicensedMessages[targetGroup, 0], EditorStyles.wordWrappedLabel);
				GUILayout.Space(5);
				GUILayout.BeginHorizontal();
				GUILayout.FlexibleSpace();
				if (styles.notLicensedMessages[targetGroup, 1].text.Length != 0)
				{
					if (GUILayout.Button(styles.notLicensedMessages[targetGroup, 1]))
					{
						Application.OpenURL(styles.notLicensedMessages[targetGroup, 2].text);
					}
				}
				GUILayout.EndHorizontal();
				GUIBuildButtons (false, false, false, platform);
				return;
			}
			
			// Draw the side bar to the right. Different options like streaming web player, Specific Standalone player to build etc.
			GUI.changed = false;
			switch (platform.targetGroup)
			{
			case BuildTargetGroup.WebPlayer:
				// Publish online?
				EditorUserBuildSettings.webPlayerDeployOnline = false; // EditorGUILayout.Toggle ("Push Live", EditorUserBuildSettings.webPlayerDeployOnline);
					// TODO: Move string to text files for translation support

				// Is webplayer streamed?
				GUI.enabled = BuildPipeline.LicenseCheck (BuildTarget.WebPlayerStreamed);
				bool streamed = EditorGUILayout.Toggle (styles.webPlayerStreamed, EditorUserBuildSettings.webPlayerStreamed);
				if (GUI.changed)
					EditorUserBuildSettings.webPlayerStreamed = streamed;

				// Use offline resources?
				bool orgGUIEnabled = GUI.enabled;
				if (EditorUserBuildSettings.webPlayerDeployOnline)
					GUI.enabled = false;
				EditorUserBuildSettings.webPlayerOfflineDeployment = EditorGUILayout.Toggle(styles.webPlayerOfflineDeployment, EditorUserBuildSettings.webPlayerOfflineDeployment);
				GUI.enabled = orgGUIEnabled;
				break;
				
			case BuildTargetGroup.NaCl:
				// Build NaCl
				break;
			case BuildTargetGroup.Standalone:
				{
					BuildTarget selectedTarget = EditorUserBuildSettings.selectedStandaloneTarget,
					            newTarget;
					
					int selectedIndex = System.Math.Max (0, System.Array.IndexOf (s_BuildPlatforms.standaloneSubtargets,
					                    BuildPlatforms.DefaultTargetForPlatform (selectedTarget)));
					int newIndex = EditorGUILayout.Popup (styles.standaloneTarget, selectedIndex, s_BuildPlatforms.standaloneSubtargetStrings);
					Dictionary <GUIContent,BuildTarget> architectures = BuildPlatforms.GetArchitecturesForPlatform (selectedTarget);
					
					if (newIndex == selectedIndex && null != architectures) {
						// Display architectures for the current target platform
						GUIContent[] architectureNames = new List<GUIContent> (architectures.Keys).ToArray ();
						int selectedArchitecture = 0;
						
						if (newIndex == selectedIndex) {
							// Grab architecture index for currently selected target
							foreach (var architecture in architectures) {
								if (architecture.Value == selectedTarget) {
									selectedArchitecture = System.Math.Max (0, System.Array.IndexOf (architectureNames, architecture.Key));
									break;
								}
							}
						}
						
						selectedArchitecture = EditorGUILayout.Popup (styles.architecture, selectedArchitecture, architectureNames);
						newTarget = architectures[architectureNames[selectedArchitecture]];
					} else {
						newTarget = s_BuildPlatforms.standaloneSubtargets[newIndex];
						
						// Force target update when switching between standalone platforms
						if (BuildTargetGroup.Standalone == BuildPipeline.GetBuildTargetGroup (EditorUserBuildSettings.activeBuildTarget))
							EditorUserBuildSettings.SwitchActiveBuildTarget (newTarget);
					}
					EditorUserBuildSettings.selectedStandaloneTarget = newTarget;
                }
				break;
			case BuildTargetGroup.iPhone:
				{
					if (Application.platform == RuntimePlatform.OSXEditor)
					EditorUserBuildSettings.symlinkLibraries = EditorGUILayout.Toggle(styles.symlinkiOSLibraries, EditorUserBuildSettings.symlinkLibraries);
				}
				break;
			case BuildTargetGroup.Android:
				{
					int selIdx = System.Array.IndexOf(s_BuildPlatforms.androidBuildSubtargets, EditorUserBuildSettings.androidBuildSubtarget);
					if (selIdx == -1)
						selIdx = 0;
					selIdx = EditorGUILayout.Popup(styles.androidBuildSubtarget, selIdx, s_BuildPlatforms.androidBuildSubtargetStrings);
					EditorUserBuildSettings.androidBuildSubtarget = s_BuildPlatforms.androidBuildSubtargets[selIdx];

					bool installInBuildFolder = EditorUserBuildSettings.installInBuildFolder && PostprocessBuildPlayer.SupportsInstallInBuildFolder (buildTarget);
					EditorUserBuildSettings.appendProject &= (GUI.enabled = !installInBuildFolder);
					EditorUserBuildSettings.appendProject = EditorGUILayout.Toggle(styles.createProject, EditorUserBuildSettings.appendProject);
					GUI.enabled = true;
				}
				break;
			case BuildTargetGroup.FlashPlayer:
				{		
					//Select the FlashPlayer
					var selIdx = System.Array.IndexOf(s_BuildPlatforms.flashBuildSubtargets, EditorUserBuildSettings.flashBuildSubtarget);
					if (selIdx == -1)
						selIdx = 0;
					selIdx = EditorGUILayout.Popup(styles.flashTarget, selIdx, s_BuildPlatforms.flashBuildSubtargetString);
					EditorUserBuildSettings.flashBuildSubtarget = s_BuildPlatforms.flashBuildSubtargets[selIdx];
				}
				break;
            case BuildTargetGroup.Metro:
                {
					int selectedIndex = System.Math.Max (0, System.Array.IndexOf(s_BuildPlatforms.metroBuildTypes, EditorUserBuildSettings.metroBuildType));
					selectedIndex = EditorGUILayout.Popup(styles.metroBuildType, selectedIndex, s_BuildPlatforms.metroBuildTypeStrings);
					EditorUserBuildSettings.metroBuildType = s_BuildPlatforms.metroBuildTypes[selectedIndex];

                    selectedIndex = System.Math.Max(0, System.Array.IndexOf(s_BuildPlatforms.metroSDKs, EditorUserBuildSettings.metroSDK));
                    selectedIndex = EditorGUILayout.Popup(styles.metroSDK, selectedIndex, s_BuildPlatforms.metroSDKStrings);
                    EditorUserBuildSettings.metroSDK = s_BuildPlatforms.metroSDKs[selectedIndex];
                }
				break;
			case BuildTargetGroup.BB10:
				{
					int selIdx = System.Array.IndexOf(s_BuildPlatforms.blackberryBuildSubtargets, EditorUserBuildSettings.blackberryBuildSubtarget);
					if (selIdx == -1)
						selIdx = 0;
					selIdx = EditorGUILayout.Popup(styles.blackberryBuildSubtarget, selIdx, s_BuildPlatforms.blackberryBuildSubtargetStrings);
					EditorUserBuildSettings.blackberryBuildSubtarget = s_BuildPlatforms.blackberryBuildSubtargets[selIdx];
				
					selIdx = System.Array.IndexOf(s_BuildPlatforms.blackberryBuildTypes, EditorUserBuildSettings.blackberryBuildType);
					if (selIdx == -1)
						selIdx = 0;
					selIdx = EditorGUILayout.Popup(styles.blackberryBuildType, selIdx, s_BuildPlatforms.blackberryBuildTypeStrings);
					EditorUserBuildSettings.blackberryBuildType = s_BuildPlatforms.blackberryBuildTypes[selIdx];
		
					GUI.enabled = true;
				}
				break;
			}

			GUI.enabled = true;

			// Are we building a development player
			// Tell user that he needs to grab a different version of Unity that supports the build target.
			bool enableBuildButton = false;
			bool enableBuildAndRunButton = false;
            bool enableConnectProfilerToggle = (buildTarget != BuildTarget.Wii && buildTarget != BuildTarget.FlashPlayer && buildTarget != BuildTarget.NaCl
                && !IsWP8Player(buildTarget));
            bool enableAllowDebuggingToggle = (buildTarget != BuildTarget.Wii && buildTarget != BuildTarget.PS3 && buildTarget != BuildTarget.FlashPlayer && buildTarget != BuildTarget.NaCl && !IsMetroPlayer(buildTarget)
                && !IsWP8Player(buildTarget));
            bool enableExplicitNullChecksToggle = (buildTarget == BuildTarget.XBOX360 || buildTarget == BuildTarget.PS3);
			bool enableHeadlessModeToggle = (buildTarget == BuildTarget.StandaloneLinux || buildTarget == BuildTarget.StandaloneLinux64 || buildTarget == BuildTarget.StandaloneLinuxUniversal) && UnityEngine.Application.HasProLicense();
			bool canInstallInBuildFolder = false;

			bool buildTargetPossible = true;
			if ((IsMetroPlayer(buildTarget) || IsWP8Player(buildTarget)) && !InternalEditorUtility.RunningUnderWindows8())
			{
				buildTargetPossible = false;
				GUILayout.Label(EditorGUIUtility.TextContent("BuildSettings.NoWindows8"), EditorStyles.wordWrappedLabel);
			}

			if (BuildPipeline.IsBuildTargetSupported(buildTarget))
			{
                bool isSubmissionBuild = (BuildTarget.PS3 == buildTarget) && (EditorUserBuildSettings.ps3BuildSubtarget != PS3BuildSubtarget.PCHosted);

                switch (platform.targetGroup)
                {
                    default:
						if (IsMetroPlayer(buildTarget))
							EditorUserBuildSettings.development = false;
						else
							EditorUserBuildSettings.development = EditorGUILayout.Toggle(styles.debugBuild, EditorUserBuildSettings.development);
                        break;
                }

                GUI.enabled = EditorUserBuildSettings.development || IsMetroPlayer(buildTarget);
				// Disable 'Autoconnect profiler' if using a non-Pro license.
				GUI.enabled = GUI.enabled && InternalEditorUtility.HasAdvancedLicenseOnBuildTarget(buildTarget);
                if (enableConnectProfilerToggle)
				    EditorUserBuildSettings.connectProfiler = EditorGUILayout.Toggle(styles.profileBuild, EditorUserBuildSettings.connectProfiler);
				GUI.enabled = EditorUserBuildSettings.development;
				if (enableAllowDebuggingToggle)
					EditorUserBuildSettings.allowDebugging = EditorGUILayout.Toggle(styles.allowDebugging, EditorUserBuildSettings.allowDebugging);
				if (enableExplicitNullChecksToggle)
                    EditorUserBuildSettings.explicitNullChecks = EditorGUILayout.Toggle(styles.explicitNullChecks, EditorUserBuildSettings.explicitNullChecks);
                GUI.enabled = !EditorUserBuildSettings.development;
                if (enableHeadlessModeToggle)
                    EditorUserBuildSettings.enableHeadlessMode = EditorGUILayout.Toggle(styles.enableHeadlessMode, EditorUserBuildSettings.enableHeadlessMode && !EditorUserBuildSettings.development); 

				GUI.enabled = true;

				GUILayout.FlexibleSpace();

				canInstallInBuildFolder = !isSubmissionBuild && Unsupported.IsDeveloperBuild() &&
											PostprocessBuildPlayer.SupportsInstallInBuildFolder(buildTarget);

				bool extensionsLoaded = buildTargetPossible;
				if (!string.IsNullOrEmpty(Modules.ModuleManager.GetTargetStringFromBuildTarget(buildTarget)) &&
					Modules.ModuleManager.GetBuildPostProcessor(buildTarget) == null)
				{
					extensionsLoaded = false;
					GUILayout.Label("Extensions not loaded for " + s_BuildPlatforms.GetBuildTargetDisplayName(buildTarget) + ".");
				}
	

				if (extensionsLoaded)
				{
					enableBuildButton = true;
					enableBuildAndRunButton = !(isSubmissionBuild || EditorUserBuildSettings.installInBuildFolder);
				}
			}
			else
			{
				GUILayout.BeginHorizontal(GUILayout.ExpandWidth (true));
				
				GUILayout.BeginVertical(GUILayout.ExpandWidth (true));

				int targetGroup = s_BuildPlatforms.BuildPlatformIndexFromTargetGroup(platform.targetGroup);

                GUILayout.Label(styles.GetTargetNotInstalled(targetGroup, 0));
                if (styles.GetTargetNotInstalled(targetGroup, 1) != null)
                    if (GUILayout.Button(styles.GetTargetNotInstalled(targetGroup, 1)))
                        Application.OpenURL(styles.GetTargetNotInstalled(targetGroup, 2).text);

				GUILayout.EndVertical();
				GUILayout.FlexibleSpace ();
				GUILayout.EndHorizontal();
			}

			GUIBuildButtons (enableBuildButton, enableBuildAndRunButton,
			                 canInstallInBuildFolder, platform);
		}

		private static void GUIBuildButtons (bool enableBuildButton,
		                                     bool enableBuildAndRunButton,
		                                     bool canInstallInBuildFolder,
		                                     BuildPlatform platform)
		{
			GUILayout.FlexibleSpace();

			if (canInstallInBuildFolder)
				EditorUserBuildSettings.installInBuildFolder = GUILayout.Toggle(EditorUserBuildSettings.installInBuildFolder, "Install in Builds folder\n(for debugging with source code)");
			else
				EditorUserBuildSettings.installInBuildFolder = false;

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();

			if (platform.targetGroup == BuildTargetGroup.Android
			    && EditorUserBuildSettings.appendProject)
				enableBuildAndRunButton = false;

			GUIContent buildButton = styles.build;
			if (platform.targetGroup == BuildTargetGroup.Android
			    && EditorUserBuildSettings.appendProject)
				buildButton = styles.export;

			if (platform.targetGroup == BuildTargetGroup.iPhone)
				if (Application.platform != RuntimePlatform.OSXEditor)
					enableBuildAndRunButton = false;

			// Build Button
			GUI.enabled = enableBuildButton;
			if (GUILayout.Button(buildButton, GUILayout.Width(Styles.kButtonWidth)))
			{
				BuildPlayerWithDefaultSettings(true, BuildOptions.ShowBuiltPlayer);
				GUIUtility.ExitGUI();
			}
			// Build and Run button
			GUI.enabled = enableBuildAndRunButton;
			if (GUILayout.Button(styles.buildAndRun, GUILayout.Width(Styles.kButtonWidth)))
			{
				BuildPlayerWithDefaultSettings(true, BuildOptions.AutoRunPlayer);
				GUIUtility.ExitGUI();
			}

			GUILayout.EndHorizontal();
		}

		private static bool PickBuildLocation (BuildTarget target)
		{
			var previousPath = EditorUserBuildSettings.GetBuildLocation (target);

			// When exporting Eclipse project, we're saving a folder, not file,
			// deal with it separately:
			if (target == BuildTarget.Android
			    && EditorUserBuildSettings.appendProject)
			{
				var exportProjectTitle  = "Export Google Android Project";
				var exportProjectFolder = EditorUtility.SaveFolderPanel (exportProjectTitle, previousPath, "");
				EditorUserBuildSettings.SetBuildLocation (target, exportProjectFolder);
				return true;
			}

			string extension = PostprocessBuildPlayer.GetExtensionForBuildTarget (target);

			string defaultFolder = FileUtil.DeleteLastPathNameComponent (previousPath);
			string defaultName = FileUtil.GetLastPathNameComponent (previousPath);
            string title = "Build " + s_BuildPlatforms.GetBuildTargetDisplayName(target);
			
            string path = EditorUtility.SaveBuildPanel(target, title, defaultFolder, defaultName, extension);
            
            if (path == string.Empty)
				return false;

			// Enforce extension if needed
			if (extension != string.Empty && FileUtil.GetPathExtension (path).ToLower () != extension)
				path += '.' + extension;

            // A path may not be empty initially, but it could contain, e.g., a drive letter (as in Windows),
            // so even appending an extention will work fine, but in reality the name will be, for example,
            // G:/
            //Debug.Log(path);
		    
            string currentlyChosenName = FileUtil.GetLastPathNameComponent(path);
            if (currentlyChosenName == string.Empty)
                return false; // No nameless projects, please

            // We don't want to re-create a directory that already exists, this may
            // result in access-denials that will make users unhappy.
            string check_dir = extension!=string.Empty ? FileUtil.DeleteLastPathNameComponent(path) : path;
            if (!Directory.Exists(check_dir))
                Directory.CreateDirectory(check_dir);

            EditorUserBuildSettings.SetBuildLocation(target, path);
            return true;
		}

        private static bool MetroAndWP8BuildRequirementsMet(out GUIContent msg)
        {
            if (!InternalEditorUtility.RunningUnderWindows8())
            {
                msg = EditorGUIUtility.TextContent("BuildSettings.NoWindows8");
                return false;
            }
            /* temporary disable, until TeamCity is updated
            if (!SyncVS.CheckVisualStudioVersion(11, 0, 60610)) // VS 2012 Update 3
            {
                msg = EditorGUIUtility.TextContent("BuildSettings.NoVS2012U3");
                return false;
            }*/

            msg = null;
            return true;
        }
	}
}
