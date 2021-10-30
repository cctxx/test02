using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;

namespace UnityEditor
{
	[CustomEditor (typeof (TextureImporter))]
	[CanEditMultipleObjects]
	internal class TextureImporterInspector : AssetImporterInspector
	{
		SerializedProperty m_TextureType;
		TextureImporterType textureType
		{
			get
			{
				if (textureTypeHasMultipleDifferentValues)
					return (TextureImporterType) (-1);
				if (m_TextureType.intValue < 0)
					return (target as TextureImporter).textureType;
				return (TextureImporterType) (m_TextureType.intValue);
			}
		}

		public new void OnDisable ()
		{
			base.OnDisable();
		}

		bool textureTypeHasMultipleDifferentValues
		{
			get
			{
				if (m_TextureType.hasMultipleDifferentValues)
					// We know the texture types are different
					return true;
				if (m_TextureType.intValue >= 0)
					// We know the texture types are the same
					return false;
				// If the serialized value is -1 for all the textures we don't know anything and have to test.
				// @TODO: Fix this when refactoring texture importing so the returned serialized value is never -1.
				TextureImporterType first = (target as TextureImporter).textureType;
				foreach (Object t in targets)
					if ((t as TextureImporter).textureType != first)
						return true;
				return false;
			}
		}

		// Don't show the imported texture as a separate editor
		internal override bool showImportedObject { get { return false; } }

		internal static bool IsGLESMobileTargetPlatform(BuildTarget target)
		{
			return target == BuildTarget.iPhone || target == BuildTarget.Android || target == BuildTarget.BB10 || target == BuildTarget.Tizen;
		}

		// Which platforms should we display?
		// For each of these, what are the formats etc. to display?
		[SerializeField]
		protected List<PlatformSetting> m_PlatformSettings;

		private enum AdvancedTextureType
		{
			Default = 0,
			NormalMap =1,
			LightMap = 2
		}

		static int[] s_TextureFormatsValueAll;
		static int[] TextureFormatsValueAll
		{
			get
			{
				if(s_TextureFormatsValueAll != null)
					return s_TextureFormatsValueAll;

				bool requireETC = false;
				bool requirePVRTC = false;
				bool requireATC = false;
                bool requireETC2 = false;
                bool requireASTC = false;

				// Build available formats based on available platforms
				BuildPlayerWindow.BuildPlatform[] validPlatforms = GetBuildPlayerValidPlatforms ();
				foreach (BuildPlayerWindow.BuildPlatform platform in validPlatforms)
				{
					switch (platform.DefaultTarget)
					{
						case BuildTarget.StandaloneGLESEmu:
							requirePVRTC = true;
							requireETC = true;
                            requireETC2 = true;
                            requireASTC = true;
							break;
						case BuildTarget.Android:
							requirePVRTC = true;
							requireETC = true;
							requireATC = true;
                            requireETC2 = true;
                            requireASTC = true;
							break;
						case BuildTarget.iPhone:
							requirePVRTC = true;
							break;
						case BuildTarget.BB10:
							requirePVRTC = true;
							requireETC = true;
							break;
						case BuildTarget.Tizen:
							requireETC = true;
							break;
						default:
							break;
					}
				}
				List<int> formatValues = new List<int>();

				formatValues.AddRange(new[]{
					(int)TextureImporterFormat.AutomaticCompressed,
					(int)TextureImporterFormat.DXT1,
					(int)TextureImporterFormat.DXT5
				});

				if(requireETC)
					formatValues.Add((int)TextureImporterFormat.ETC_RGB4);

				if(requirePVRTC)
					formatValues.AddRange(new[]{
						(int)TextureImporterFormat.PVRTC_RGB2,
						(int)TextureImporterFormat.PVRTC_RGBA2,
						(int)TextureImporterFormat.PVRTC_RGB4,
						(int)TextureImporterFormat.PVRTC_RGBA4
					});

				if(requireATC)
					formatValues.AddRange(new int[]{
						(int)TextureImporterFormat.ATC_RGB4,
						(int)TextureImporterFormat.ATC_RGBA8,
					});

                if(requireETC2)
                    formatValues.AddRange(new[]{
						(int)TextureImporterFormat.ETC2_RGB4,
						(int)TextureImporterFormat.ETC2_RGB4_PUNCHTHROUGH_ALPHA,
						(int)TextureImporterFormat.ETC2_RGBA8
					});

                if(requireASTC)
                    formatValues.AddRange(new[]{
						(int)TextureImporterFormat.ASTC_RGB_4x4,
						(int)TextureImporterFormat.ASTC_RGB_5x5,
						(int)TextureImporterFormat.ASTC_RGB_6x6,
						(int)TextureImporterFormat.ASTC_RGB_8x8,
						(int)TextureImporterFormat.ASTC_RGB_10x10,
						(int)TextureImporterFormat.ASTC_RGB_12x12,
						(int)TextureImporterFormat.ASTC_RGBA_4x4,
						(int)TextureImporterFormat.ASTC_RGBA_5x5,
						(int)TextureImporterFormat.ASTC_RGBA_6x6,
						(int)TextureImporterFormat.ASTC_RGBA_8x8,
						(int)TextureImporterFormat.ASTC_RGBA_10x10,
						(int)TextureImporterFormat.ASTC_RGBA_12x12
					});



				formatValues.AddRange(new[]{
					(int)TextureImporterFormat.Automatic16bit,
					(int)TextureImporterFormat.RGB16,
					(int)TextureImporterFormat.ARGB16,
					(int)TextureImporterFormat.RGBA16,

					(int)TextureImporterFormat.AutomaticTruecolor,
					(int)TextureImporterFormat.RGB24,
					(int)TextureImporterFormat.Alpha8,
					(int)TextureImporterFormat.ARGB32,
					(int)TextureImporterFormat.RGBA32,
				});

				s_TextureFormatsValueAll = formatValues.ToArray();
				return s_TextureFormatsValueAll;
			}
		}

		static readonly int[] kTextureFormatsValueWeb =
		{
			(int)TextureImporterFormat.DXT1,
			(int)TextureImporterFormat.DXT5,
			(int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.ARGB16,
			(int)TextureImporterFormat.ARGB32,
		};
		static readonly int[] kTextureFormatsValueWii =
		{
			(int)TextureImporterFormat.WiiI4,
			(int)TextureImporterFormat.WiiI8,
			(int)TextureImporterFormat.WiiIA4,
			(int)TextureImporterFormat.WiiIA8,
			(int)TextureImporterFormat.WiiRGB565,
			(int)TextureImporterFormat.WiiRGB5A3,
			(int)TextureImporterFormat.WiiRGBA8,
			(int)TextureImporterFormat.WiiCMPR,
		};
		static readonly int[] kTextureFormatsValueiPhone =
		{
			(int)TextureImporterFormat.PVRTC_RGB2,
			(int)TextureImporterFormat.PVRTC_RGBA2,
			(int)TextureImporterFormat.PVRTC_RGB4,
			(int)TextureImporterFormat.PVRTC_RGBA4,
			(int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.RGBA16,
			(int)TextureImporterFormat.RGBA32,
		};

		static readonly int[] kTextureFormatsValueAndroid =
		{
			(int)TextureImporterFormat.DXT1,
			(int)TextureImporterFormat.DXT5,

			(int)TextureImporterFormat.ETC_RGB4,

			(int)TextureImporterFormat.ETC2_RGB4,
			(int)TextureImporterFormat.ETC2_RGB4_PUNCHTHROUGH_ALPHA,
			(int)TextureImporterFormat.ETC2_RGBA8,


			(int)TextureImporterFormat.PVRTC_RGB2,
			(int)TextureImporterFormat.PVRTC_RGBA2,
			(int)TextureImporterFormat.PVRTC_RGB4,
			(int)TextureImporterFormat.PVRTC_RGBA4,

			(int)TextureImporterFormat.ATC_RGB4,
			(int)TextureImporterFormat.ATC_RGBA8,

			(int)TextureImporterFormat.ASTC_RGB_4x4,
			(int)TextureImporterFormat.ASTC_RGB_5x5,
			(int)TextureImporterFormat.ASTC_RGB_6x6,
			(int)TextureImporterFormat.ASTC_RGB_8x8,
			(int)TextureImporterFormat.ASTC_RGB_10x10,
			(int)TextureImporterFormat.ASTC_RGB_12x12,
			(int)TextureImporterFormat.ASTC_RGBA_4x4,
			(int)TextureImporterFormat.ASTC_RGBA_5x5,
			(int)TextureImporterFormat.ASTC_RGBA_6x6,
			(int)TextureImporterFormat.ASTC_RGBA_8x8,
			(int)TextureImporterFormat.ASTC_RGBA_10x10,
			(int)TextureImporterFormat.ASTC_RGBA_12x12,

			(int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.RGBA16,
			(int)TextureImporterFormat.RGBA32
		};

		static readonly int[] kTextureFormatsValueBB10 =
		{
			(int)TextureImporterFormat.ETC_RGB4,

			(int)TextureImporterFormat.PVRTC_RGB2,
			(int)TextureImporterFormat.PVRTC_RGBA2,
			(int)TextureImporterFormat.PVRTC_RGB4,
			(int)TextureImporterFormat.PVRTC_RGBA4,
			(int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.RGBA16,
			(int)TextureImporterFormat.RGBA32,
		};

		static readonly int[] kTextureFormatsValueTizen =
		{
			(int)TextureImporterFormat.ETC_RGB4,

			(int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.RGBA16,
			(int)TextureImporterFormat.RGBA32,
		};

		static readonly int[] kTextureFormatsValueFlash =
		{
			//(int)TextureImporterFormat.ATF_RGB_DXT1,
			(int)TextureImporterFormat.ATF_RGB_JPG,
			(int)TextureImporterFormat.ATF_RGBA_JPG,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.RGBA32
		};

		static readonly int[] kTextureFormatsValueGLESEmu =
		{
			(int)TextureImporterFormat.ETC_RGB4,
			(int)TextureImporterFormat.PVRTC_RGB2,
			(int)TextureImporterFormat.PVRTC_RGBA2,
			(int)TextureImporterFormat.PVRTC_RGB4,
			(int)TextureImporterFormat.PVRTC_RGBA4,

			(int)TextureImporterFormat.ETC2_RGB4,
			(int)TextureImporterFormat.ETC2_RGB4_PUNCHTHROUGH_ALPHA,
			(int)TextureImporterFormat.ETC2_RGBA8,

			(int)TextureImporterFormat.ASTC_RGB_4x4,
			(int)TextureImporterFormat.ASTC_RGB_5x5,
			(int)TextureImporterFormat.ASTC_RGB_6x6,
			(int)TextureImporterFormat.ASTC_RGB_8x8,
			(int)TextureImporterFormat.ASTC_RGB_10x10,
			(int)TextureImporterFormat.ASTC_RGB_12x12,
			(int)TextureImporterFormat.ASTC_RGBA_4x4,
			(int)TextureImporterFormat.ASTC_RGBA_5x5,
			(int)TextureImporterFormat.ASTC_RGBA_6x6,
			(int)TextureImporterFormat.ASTC_RGBA_8x8,
			(int)TextureImporterFormat.ASTC_RGBA_10x10,
			(int)TextureImporterFormat.ASTC_RGBA_12x12,

            
            (int)TextureImporterFormat.RGB16,
			(int)TextureImporterFormat.RGB24,
			(int)TextureImporterFormat.Alpha8,
			(int)TextureImporterFormat.RGBA16,
			(int)TextureImporterFormat.ARGB32,
		};

		static int[] s_NormalFormatsValueAll;
		static int[] NormalFormatsValueAll
		{
			get
			{
				bool requireETC = false;
				bool requirePVRTC = false;
				bool requireATC = false;
                bool requireETC2 = false;
                bool requireASTC = false;

				// Build available normals formats based on available platforms
				BuildPlayerWindow.BuildPlatform[] validPlatforms = GetBuildPlayerValidPlatforms();
				foreach (BuildPlayerWindow.BuildPlatform platform in validPlatforms)
				{
					switch (platform.DefaultTarget)
					{
						case BuildTarget.StandaloneGLESEmu:
							requirePVRTC = true;
							requireETC = true;
                            requireETC2 = true;
                            requireASTC = true;
							break;
						case BuildTarget.Android:
							requirePVRTC = true;
							requireATC = true;
							requireETC = true;
                            requireETC2 = true;
                            requireASTC = true;
							break;
						case BuildTarget.iPhone:
							requirePVRTC = true;
							requireETC = true;
							break;
						case BuildTarget.BB10:
							requirePVRTC = true;
							requireETC = true;
							break;
						case BuildTarget.Tizen:
							requireETC = true;
							break;
						default:
							break;
					}
				}
				List<int> formatValues = new List<int>();

				formatValues.AddRange(new[]{
					(int)TextureImporterFormat.AutomaticCompressed,
					(int)TextureImporterFormat.DXT5
				});

				if(requirePVRTC)
					formatValues.AddRange(new[]{
						(int)TextureImporterFormat.PVRTC_RGB2,
						(int)TextureImporterFormat.PVRTC_RGBA2,
						(int)TextureImporterFormat.PVRTC_RGB4,
						(int)TextureImporterFormat.PVRTC_RGBA4,
					});

				if(requireATC)
					formatValues.AddRange(new int[]{
						(int)TextureImporterFormat.ATC_RGB4,
						(int)TextureImporterFormat.ATC_RGBA8,
					});

				if(requireETC)
					formatValues.AddRange(new int[]{
						(int)TextureImporterFormat.ETC_RGB4,
					});

                if (requireETC2)
                    formatValues.AddRange(new[]{
						(int)TextureImporterFormat.ETC2_RGB4,
						(int)TextureImporterFormat.ETC2_RGB4_PUNCHTHROUGH_ALPHA,
						(int)TextureImporterFormat.ETC2_RGBA8
					});

                if (requireASTC)
                    formatValues.AddRange(new[]{
						(int)TextureImporterFormat.ASTC_RGB_4x4,
						(int)TextureImporterFormat.ASTC_RGB_5x5,
						(int)TextureImporterFormat.ASTC_RGB_6x6,
						(int)TextureImporterFormat.ASTC_RGB_8x8,
						(int)TextureImporterFormat.ASTC_RGB_10x10,
						(int)TextureImporterFormat.ASTC_RGB_12x12,
						(int)TextureImporterFormat.ASTC_RGBA_4x4,
						(int)TextureImporterFormat.ASTC_RGBA_5x5,
						(int)TextureImporterFormat.ASTC_RGBA_6x6,
						(int)TextureImporterFormat.ASTC_RGBA_8x8,
						(int)TextureImporterFormat.ASTC_RGBA_10x10,
						(int)TextureImporterFormat.ASTC_RGBA_12x12
					});

				formatValues.AddRange(new[]{
					(int)TextureImporterFormat.Automatic16bit,
					(int)TextureImporterFormat.ARGB16,
					(int)TextureImporterFormat.RGBA16,

					(int)TextureImporterFormat.AutomaticTruecolor,
					(int)TextureImporterFormat.RGBA32
				});

				s_NormalFormatsValueAll = formatValues.ToArray();

				return s_NormalFormatsValueAll;
			}
		}

		static readonly int[] kNormalFormatsValueWeb =
		{
			(int)TextureImporterFormat.DXT5,
			(int)TextureImporterFormat.ARGB16,
			(int)TextureImporterFormat.ARGB32,
		};

		static readonly int[] kNormalFormatsValueFlash =
		{
			(int)TextureImporterFormat.ATF_RGBA_JPG,
			(int)TextureImporterFormat.RGBA32
		};

		// Are these even needed on Wii?
		static readonly int[] kNormalFormatValueWii =
		{
			(int)TextureImporterFormat.WiiRGB565,
			(int)TextureImporterFormat.WiiRGB5A3,
			(int)TextureImporterFormat.WiiRGBA8,
			(int)TextureImporterFormat.WiiCMPR,
		};

		static readonly TextureImporterFormat[] kFormatsWithCompressionSettings =
		{
			TextureImporterFormat.PVRTC_RGB2,
			TextureImporterFormat.PVRTC_RGB4,
			TextureImporterFormat.PVRTC_RGBA2,
			TextureImporterFormat.PVRTC_RGBA4,
			TextureImporterFormat.ATC_RGB4,
			TextureImporterFormat.ATC_RGBA8,
			TextureImporterFormat.ETC_RGB4,
			TextureImporterFormat.ATF_RGB_JPG,
			TextureImporterFormat.ATF_RGBA_JPG,
            TextureImporterFormat.ETC2_RGB4,
            TextureImporterFormat.ETC2_RGB4_PUNCHTHROUGH_ALPHA,
            TextureImporterFormat.ETC2_RGBA8,
            TextureImporterFormat.ASTC_RGB_4x4,
            TextureImporterFormat.ASTC_RGB_5x5,
            TextureImporterFormat.ASTC_RGB_6x6,
            TextureImporterFormat.ASTC_RGB_8x8,
            TextureImporterFormat.ASTC_RGB_10x10,
            TextureImporterFormat.ASTC_RGB_12x12,
            TextureImporterFormat.ASTC_RGBA_4x4,
            TextureImporterFormat.ASTC_RGBA_5x5,
            TextureImporterFormat.ASTC_RGBA_6x6,
            TextureImporterFormat.ASTC_RGBA_8x8,
            TextureImporterFormat.ASTC_RGBA_10x10,
            TextureImporterFormat.ASTC_RGBA_12x12

		};

		static string[] s_TextureFormatStringsAll;
		static string[] s_TextureFormatStringsWii;
		static string[] s_TextureFormatStringsGLESEmu;
		static string[] s_TextureFormatStringsiPhone;
		static string[] s_TextureFormatStringsAndroid;
		static string[] s_TextureFormatStringsBB10;
		static string[] s_TextureFormatStringsTizen;
		static string[] s_TextureFormatStringsFlash;
		static string[] s_TextureFormatStringsWeb;
		static string[] s_NormalFormatStringsAll;
		static string[] s_NormalFormatStringsFlash;
		static string[] s_NormalFormatStringsWeb;

		[System.Serializable]
		protected class PlatformSetting
		{
			[SerializeField] public string name;

			// Is Overridden
			[SerializeField] private bool m_Overridden;
			[SerializeField] private bool m_OverriddenIsDifferent = false;
			public bool overridden { get { return m_Overridden; } }
			public bool overriddenIsDifferent { get { return m_OverriddenIsDifferent; } }
			public bool allAreOverridden { get { return isDefault || (m_Overridden && !m_OverriddenIsDifferent); } }
			public void SetOverriddenForAll (bool overridden)
			{
				m_Overridden = overridden;
				m_OverriddenIsDifferent = false;
				m_HasChanged = true;
			}

			// Maximum texture size
			[SerializeField] private int m_MaxTextureSize;
			[SerializeField] private bool m_MaxTextureSizeIsDifferent = false;
			public int maxTextureSize { get { return m_MaxTextureSize; } }
			public bool maxTextureSizeIsDifferent { get { return m_MaxTextureSizeIsDifferent; } }
			public void SetMaxTextureSizeForAll (int maxTextureSize)
			{
				Assert.IsTrue (allAreOverridden, "Attempting to set max texture size for all platforms even though settings are not overwritten for all platforms.");
				m_MaxTextureSize = maxTextureSize;
				m_MaxTextureSizeIsDifferent = false;
				m_HasChanged = true;
			}

			// Compression rate
			[SerializeField] private int m_CompressionQuality;
			[SerializeField] private bool m_CompressionQualityIsDifferent = false;
			public int compressionQuality { get { return m_CompressionQuality; } }
			public bool compressionQualityIsDifferent { get { return m_CompressionQualityIsDifferent; } }
			public void SetCompressionQualityForAll (int quality)
			{
				Assert.IsTrue (allAreOverridden, "Attempting to set texture compression quality for all platforms even though settings are not overwritten for all platforms.");
				m_CompressionQuality = quality;
				m_CompressionQualityIsDifferent = false;
				m_HasChanged = true;
			}


			// Texture format
			[SerializeField] private TextureImporterFormat[] m_TextureFormatArray;
			[SerializeField] private bool m_TextureFormatIsDifferent = false;
			public TextureImporterFormat[] textureFormats { get { return m_TextureFormatArray; } }
			public bool textureFormatIsDifferent { get { return m_TextureFormatIsDifferent; } }
			public void SetTextureFormatForAll (TextureImporterFormat format)
			{
				Assert.IsTrue (allAreOverridden, "Attempting to set texture format for all platforms even though settings are not overwritten for all platforms.");
				for (int i=0; i<m_TextureFormatArray.Length; i++)
				{
					m_TextureFormatArray[i] = format;
				}
				m_TextureFormatIsDifferent = false;
				m_HasChanged = true;
			}

			[SerializeField] public BuildTarget m_Target;
			[SerializeField] TextureImporter[] m_Importers;
			public TextureImporter[] importers { get { return m_Importers; } }

			[SerializeField] bool m_HasChanged = false;
			[SerializeField] TextureImporterInspector m_Inspector;
			public bool isDefault { get { return name == ""; } }

			public PlatformSetting (string name, BuildTarget target, TextureImporterInspector inspector)
			{
				this.name = name;

				m_Target = target;
				m_Inspector = inspector;
				m_Overridden = false;
				m_Importers = inspector.targets.Select (x => x as TextureImporter).ToArray ();
				m_TextureFormatArray = new TextureImporterFormat[importers.Length];
				for (int i=0; i<importers.Length; i++)
				{
					TextureImporter imp = importers[i];
					TextureImporterFormat format;
					int maxTextureSize;
					int compressionQuality;
					bool overridden;

					if (!isDefault)
					{
						overridden = imp.GetPlatformTextureSettings (name, out maxTextureSize, out format, out compressionQuality);
					}
					else
					{
						overridden = true;
						maxTextureSize = imp.maxTextureSize;
						format = imp.textureFormat;
						compressionQuality = imp.compressionQuality;
					}

					m_TextureFormatArray[i] = format;
					if (i == 0)
					{
						m_Overridden = overridden;
						m_MaxTextureSize = maxTextureSize;
						m_CompressionQuality = compressionQuality;
					}
					else
					{
						if (overridden != m_Overridden)
							m_OverriddenIsDifferent = true;
						if (maxTextureSize != m_MaxTextureSize)
							m_MaxTextureSizeIsDifferent = true;
						if (compressionQuality != m_CompressionQuality)
							m_CompressionQualityIsDifferent = true;
						if (format != m_TextureFormatArray[0])
							m_TextureFormatIsDifferent = true;
					}
				}

				Sync ();
			}

			public bool SupportsFormat (TextureImporterFormat format, TextureImporter importer)
			{
				TextureImporterSettings settings = GetSettings (importer);
				int[] testValues;
				switch (m_Target)
				{
					case BuildTarget.Wii:
						testValues = settings.normalMap ? kNormalFormatValueWii : kTextureFormatsValueWii;
						break;
					case BuildTarget.StandaloneGLESEmu:
						testValues = settings.normalMap ? kNormalFormatsValueWeb : kTextureFormatsValueGLESEmu;
						break;
					case BuildTarget.FlashPlayer:
						testValues = settings.normalMap ? kNormalFormatsValueFlash : kTextureFormatsValueFlash;
						break;

					// on gles mobile targets we use rgb normal maps, so we can use whatever format we want
					case BuildTarget.iPhone:
						testValues = kTextureFormatsValueiPhone;
						break;
					case BuildTarget.Android:
						testValues = kTextureFormatsValueAndroid;
						break;
					case BuildTarget.BB10:
						testValues = kTextureFormatsValueBB10;
						break;
					case BuildTarget.Tizen:
						testValues = kTextureFormatsValueTizen;
						break;

					default:
						testValues = settings.normalMap ? kNormalFormatsValueWeb : kTextureFormatsValueWeb;
						break;
				}

				if ((testValues as IList).Contains ((int)format))
					return true;
				return false;
			}

			public TextureImporterSettings GetSettings (TextureImporter importer)
			{
				TextureImporterSettings settings = new TextureImporterSettings ();
				// Get import settings for this importer
				importer.ReadTextureSettings (settings);
				// Get settings that have been changed in the inspector
				m_Inspector.GetSerializedPropertySettings (settings);
				return settings;
			}

			public virtual bool HasChanged ()
			{
				return m_HasChanged;
			}

			public void Sync ()
			{
				// Use settings from default if any of the targets are not overridden
				if (!isDefault && (!m_Overridden || m_OverriddenIsDifferent))
				{
					PlatformSetting defaultSettings = m_Inspector.m_PlatformSettings[0];
					m_MaxTextureSize = defaultSettings.m_MaxTextureSize;
					m_MaxTextureSizeIsDifferent = defaultSettings.m_MaxTextureSizeIsDifferent;
					m_TextureFormatArray = (TextureImporterFormat[])defaultSettings.m_TextureFormatArray.Clone ();
					m_TextureFormatIsDifferent = defaultSettings.m_TextureFormatIsDifferent;
					m_CompressionQuality = defaultSettings.m_CompressionQuality;
					m_CompressionQualityIsDifferent = defaultSettings.m_CompressionQualityIsDifferent;
				}

				TextureImporterType tType = m_Inspector.textureType;

				for (int i=0; i<importers.Length; i++)
				{
					TextureImporter imp = importers[i];
					// Get the settings of this importer, overwritten with settings from the inspector
					TextureImporterSettings settings = GetSettings (imp);

					if (tType == TextureImporterType.Advanced)
					{
						if (isDefault)
							continue;

						// Convert to simple format if not supported
						if (!SupportsFormat (m_TextureFormatArray[i], imp))
							m_TextureFormatArray[i] = TextureImporter.FullToSimpleTextureFormat (m_TextureFormatArray[i]);

						// Convert simple format to full format
						if ((int)m_TextureFormatArray[i] < 0)
						{
							m_TextureFormatArray[i] = TextureImporter.SimpleToFullTextureFormat2 (
																								  m_TextureFormatArray[i],
																								  tType,
																								  settings,
																								  imp.DoesSourceTextureHaveAlpha (),
																								  imp.DoesSourceTextureHaveColor (),
																								  m_Target);
						}
					}
					else
					{
						if ((int)m_TextureFormatArray[i] >= 0)
							m_TextureFormatArray[i] = TextureImporter.FullToSimpleTextureFormat (m_TextureFormatArray[i]);
					}

					// Force alpha for normal maps (on non-gles platforms)
					if (settings.normalMap && !IsGLESMobileTargetPlatform(m_Target))
						m_TextureFormatArray[i] = MakeTextureFormatHaveAlpha (m_TextureFormatArray[i]);
				}

				// Check if texture format is same for all targets
				m_TextureFormatIsDifferent = false;
				foreach (TextureImporterFormat format in m_TextureFormatArray)
				if (format != m_TextureFormatArray[0])
					m_TextureFormatIsDifferent = true;
			}

			private bool GetOverridden (TextureImporter importer)
			{
				if (!m_OverriddenIsDifferent)
					return m_Overridden;
				int dummySize;
				TextureImporterFormat dummyFormat;
				return importer.GetPlatformTextureSettings (name, out dummySize, out dummyFormat);
			}

			public void Apply ()
			{
				for (int i=0; i<importers.Length; i++)
				{
					TextureImporter imp = importers[i];

					// Extract current values
					int textureSize;
					int curCompressionQuality = -1;
					TextureImporterFormat dummyFormat;
					bool overridden = false;
					if (isDefault)
						textureSize = imp.maxTextureSize;
					else
						overridden = imp.GetPlatformTextureSettings (name, out textureSize, out dummyFormat, out curCompressionQuality);

					// Get settings from default if the target wasn't overwritten on last apply.
					if (!overridden)
					{
						// Only relevant for texture size so far. Format is handled specially and compression doesn't have a default.
						textureSize = imp.maxTextureSize;
					}

					// Overwrite with inspector max size if same for all targets
					if (!m_MaxTextureSizeIsDifferent)
						textureSize = m_MaxTextureSize;
					if (!m_CompressionQualityIsDifferent)
						curCompressionQuality = m_CompressionQuality;

					if (!isDefault)
					{
						// Overwrite with inspector override flag if same for all targets
						if (!m_OverriddenIsDifferent)
							overridden = m_Overridden;

						if (overridden)
							imp.SetPlatformTextureSettings (name, textureSize, m_TextureFormatArray[i], curCompressionQuality);
						else
							imp.ClearPlatformTextureSettings (name);
					}
					else
					{
						imp.maxTextureSize = textureSize;
						imp.textureFormat = m_TextureFormatArray[i];
					}
				}
			}
		}

		enum CookieMode
		{
			Spot = 0, Directional = 1, Point = 2
		}

		readonly AnimValueManager m_Anims = new AnimValueManager ();
		readonly AnimBool m_ShowBumpGenerationSettings = new AnimBool();
		readonly AnimBool m_ShowCookieCubeMapSettings = new AnimBool();
#if ENABLE_SPRITES
		readonly AnimBool m_ShowGenericSpriteSettings = new AnimBool ();
		readonly AnimBool m_ShowManualAtlasGenerationSettings = new AnimBool ();
		readonly GUIContent m_EmptyContent = new GUIContent(" ");
#endif
		// keep in sync with Styles.textureTypeOptions
		readonly int[] m_TextureTypeValues =
			{
				0,
				1,
				2,
#				if ENABLE_SPRITES
				8,
#				endif
				7,
				3,
				4,
				6,
				5
			};
		// keep in sync with Styles.textureFormatOptions
		readonly int[] m_TextureFormatValues = { 0, 1, 2 };

		string	m_ImportWarning = null;
		private void UpdateImportWarning()
		{
			TextureImporter importer = target as TextureImporter;
			m_ImportWarning = importer ? importer.GetImportWarnings() : null;
		}

		class Styles
		{
			public readonly GUIContent textureType = EditorGUIUtility.TextContent ("TextureImporter.TextureType");
			public readonly GUIContent[] textureTypeOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Image"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Normalmap"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.GUI"),
#				if ENABLE_SPRITES
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Sprite"),
#				endif
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Cursor"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Reflection"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Cookie"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Lightmap"),
				EditorGUIUtility.TextContent ("TextureImporter.TextureType.Advanced")
			};

			public readonly GUIContent generateAlphaFromGrayscale = EditorGUIUtility.TextContent ("TextureImporter.GenerateAlphaFromGrayscale");

			public readonly GUIContent cookieType = EditorGUIUtility.TextContent ("TextureImporter.Cookie");
			public readonly GUIContent[] cookieOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.Cookie.Spot"),
				EditorGUIUtility.TextContent ("TextureImporter.Cookie.Directional"),
				EditorGUIUtility.TextContent ("TextureImporter.Cookie.Point"),
			};
			public readonly GUIContent generateFromBump = EditorGUIUtility.TextContent ("TextureImporter.GenerateFromBump");
			public readonly GUIContent generateBumpmap = EditorGUIUtility.TextContent ("TextureImporter.GenerateBumpmap");
			public readonly GUIContent bumpiness = EditorGUIUtility.TextContent ("TextureImporter.Bumpiness");
			public readonly GUIContent bumpFiltering = EditorGUIUtility.TextContent ("TextureImporter.BumpFiltering");
			public readonly GUIContent[] bumpFilteringOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.BumpFiltering.Sharp"),
				EditorGUIUtility.TextContent ("TextureImporter.BumpFiltering.Smooth"),
			};
			public readonly GUIContent refMap = EditorGUIUtility.TextContent ("TextureImporter.RefMap");
			public readonly GUIContent[] refMapOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.RefMap.Sphere"),
				EditorGUIUtility.TextContent ("TextureImporter.RefMap.Cylindrical"),
				EditorGUIUtility.TextContent ("TextureImporter.RefMap.SimpleSphere"),
				EditorGUIUtility.TextContent ("TextureImporter.RefMap.NiceSphere"),
				EditorGUIUtility.TextContent ("TextureImporter.RefMap.FullCubemap"),
			};
			public readonly GUIContent seamlessCubemap = EditorGUIUtility.TextContent ("TextureImporter.SeamlessCubemap");
			public readonly GUIContent maxSize = EditorGUIUtility.TextContent ("TextureImporter.MaxSize");
			public readonly GUIContent textureFormat = EditorGUIUtility.TextContent ("TextureImporter.TextureFormat");
			public readonly string[] textureFormatOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.TextureFormat.Compressed").text,
				EditorGUIUtility.TextContent ("TextureImporter.TextureFormat.16 bit").text,
				EditorGUIUtility.TextContent ("TextureImporter.TextureFormat.Truecolor").text
			};

			public readonly GUIContent defaultPlatform = EditorGUIUtility.TextContent ("TextureImporter.Platforms.Default");
			public readonly GUIContent mipmapFadeOutToggle = EditorGUIUtility.TextContent ("TextureImporter.MipmapFadeToggle");
			public readonly GUIContent mipmapFadeOut = EditorGUIUtility.TextContent ("TextureImporter.MipmapFade");
			public readonly GUIContent readWrite = EditorGUIUtility.TextContent ("TextureImporter.ReadWrite");
			public readonly GUIContent generateMipMaps = EditorGUIUtility.TextContent ("TextureImporter.GenerateMipMaps");
			public readonly GUIContent mipMapsInLinearSpace = EditorGUIUtility.TextContent("TextureImporter.MipMapsInLinearSpace");
			public readonly GUIContent linearTexture = EditorGUIUtility.TextContent("TextureImporter.LinearTexture");
			public readonly GUIContent borderMipMaps = EditorGUIUtility.TextContent ("TextureImporter.BorderMipMaps");
			public readonly GUIContent mipMapFilter = EditorGUIUtility.TextContent ("TextureImporter.MipMapFilter");
			public readonly GUIContent[] mipMapFilterOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.MipMapFilterOptions.Box"),
				EditorGUIUtility.TextContent ("TextureImporter.MipMapFilterOptions.Kaiser"),
			};
			public readonly GUIContent normalmap = EditorGUIUtility.TextContent ("TextureImporter.NormalMap");
			public readonly GUIContent npot = EditorGUIUtility.TextContent ("TextureImporter.Npot");
			public readonly GUIContent generateCubemap = EditorGUIUtility.TextContent ("TextureImporter.GenerateCubemap");
			public readonly GUIContent lightmap = EditorGUIUtility.TextContent("TextureImporter.Lightmap");

			public readonly GUIContent compressionQuality = EditorGUIUtility.TextContent ("TextureImporter.CompressionQuality");
			public readonly GUIContent[] mobileCompressionQualityOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.CompressionQualityOptions.Fast"),
				EditorGUIUtility.TextContent ("TextureImporter.CompressionQualityOptions.Normal"),
				EditorGUIUtility.TextContent ("TextureImporter.CompressionQualityOptions.Best")
			};

#if ENABLE_SPRITES
			public readonly GUIContent spriteMode = EditorGUIUtility.TextContent("TextureImporter.SpriteMode");
			public readonly GUIContent[] spriteModeOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.SpriteModeOptions.SingleSprite"),
				EditorGUIUtility.TextContent ("TextureImporter.SpriteModeOptions.Multiple"),
			};
			public readonly GUIContent[] spriteModeOptionsAdvanced =
			{
				EditorGUIUtility.TextContent ("TextureImporter.SpriteModeOptions.None"),
				EditorGUIUtility.TextContent ("TextureImporter.SpriteModeOptions.SingleSprite"),
				EditorGUIUtility.TextContent ("TextureImporter.SpriteModeOptions.Multiple"),
			};
			public readonly GUIContent[] spriteMeshTypeOptions =
			{
				EditorGUIUtility.TextContent ("TextureImporter.SpriteMeshTypeOptions.FullRect"),
				EditorGUIUtility.TextContent ("TextureImporter.SpriteMeshTypeOptions.Tight"),
			};

			public readonly GUIContent spritePackingTag = EditorGUIUtility.TextContent ("TextureImporter.SpritePackingTag");
			public readonly GUIContent spritePixelsToUnits = EditorGUIUtility.TextContent ("TextureImporter.SpritePixelsToUnits");
			public readonly GUIContent spriteExtrude = EditorGUIUtility.TextContent ("TextureImporter.SpriteExtrude");
			public readonly GUIContent spriteMeshType = EditorGUIUtility.TextContent("TextureImporter.SpriteMeshType");
#if ENABLE_SPRITECOLLIDER
			public readonly GUIContent spriteColliderAlphaCutoff = EditorGUIUtility.TextContent ("TextureImporter.SpriteColliderAlphaCutoff");
			public readonly GUIContent spriteColliderDetail = EditorGUIUtility.TextContent ("TextureImporter.SpriteColliderDetail");
#endif
			public readonly GUIContent spriteAlignment = EditorGUIUtility.TextContent ("SpriteInspector.Pivot");
			public readonly GUIContent[] spriteAlignmentOptions =
			{
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Center"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Top"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Left"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Right"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Bottom"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Custom"),
			};
#endif

			public readonly GUIContent alphaIsTransparency = EditorGUIUtility.TextContent("TextureImporter.AlphaIsTransparency");
		}

		static Styles s_Styles;

		void ToggleFromInt (SerializedProperty property, GUIContent label)
		{
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = property.hasMultipleDifferentValues;
			int value = EditorGUILayout.Toggle (label, property.intValue > 0) ? 1 : 0;
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				property.intValue = value;
		}

		void EnumPopup (SerializedProperty property, System.Type type, GUIContent label)
		{
			EditorGUILayout.IntPopup (property,
									  EditorGUIUtility.TempContent (System.Enum.GetNames (type)),
									  System.Enum.GetValues (type) as int[],
									  label);
		}

		SerializedProperty m_GrayscaleToAlpha;
		SerializedProperty m_ConvertToNormalMap;
		SerializedProperty m_NormalMap;
		SerializedProperty m_HeightScale;
		SerializedProperty m_NormalMapFilter;
		SerializedProperty m_GenerateCubemap;
		SerializedProperty m_SeamlessCubemap;
		SerializedProperty m_BorderMipMap;
		SerializedProperty m_NPOTScale;
		SerializedProperty m_IsReadable;
		SerializedProperty m_LinearTexture;
		SerializedProperty m_EnableMipMap;
		SerializedProperty m_GenerateMipsInLinearSpace;
		SerializedProperty m_MipMapMode;
		SerializedProperty m_FadeOut;
		SerializedProperty m_MipMapFadeDistanceStart;
		SerializedProperty m_MipMapFadeDistanceEnd;
		SerializedProperty m_Lightmap;

		SerializedProperty m_Aniso;
		SerializedProperty m_FilterMode;
		SerializedProperty m_WrapMode;

#if ENABLE_SPRITES
		SerializedProperty m_SpriteMode;
		SerializedProperty m_SpritePackingTag;
		SerializedProperty m_SpritePixelsToUnits;
		SerializedProperty m_SpriteExtrude;
		SerializedProperty m_SpriteMeshType;
		#if ENABLE_SPRITECOLLIDER
		SerializedProperty m_SpriteColliderDetail;
		SerializedProperty m_SpriteColliderAlphaCutoff;
		#endif
		SerializedProperty m_Alignment;
		SerializedProperty m_SpritePivot;
#endif

		SerializedProperty m_AlphaIsTransparency;

		void CacheSerializedProperties ()
		{
			m_TextureType = serializedObject.FindProperty ("m_TextureType");
			m_GrayscaleToAlpha = serializedObject.FindProperty ("m_GrayScaleToAlpha");
			m_ConvertToNormalMap = serializedObject.FindProperty ("m_ConvertToNormalMap");
			m_NormalMap = serializedObject.FindProperty ("m_ExternalNormalMap");
			m_HeightScale = serializedObject.FindProperty ("m_HeightScale");
			m_NormalMapFilter = serializedObject.FindProperty ("m_NormalMapFilter");
			m_GenerateCubemap = serializedObject.FindProperty ("m_GenerateCubemap");
			m_SeamlessCubemap = serializedObject.FindProperty ("m_SeamlessCubemap");
			m_BorderMipMap = serializedObject.FindProperty ("m_BorderMipMap");
			m_NPOTScale = serializedObject.FindProperty ("m_NPOTScale");
			m_IsReadable = serializedObject.FindProperty ("m_IsReadable");
			m_LinearTexture = serializedObject.FindProperty("m_LinearTexture");
			m_EnableMipMap = serializedObject.FindProperty("m_EnableMipMap");
			m_MipMapMode = serializedObject.FindProperty ("m_MipMapMode");
			m_GenerateMipsInLinearSpace = serializedObject.FindProperty("correctGamma");
			m_FadeOut = serializedObject.FindProperty ("m_FadeOut");
			m_MipMapFadeDistanceStart = serializedObject.FindProperty ("m_MipMapFadeDistanceStart");
			m_MipMapFadeDistanceEnd = serializedObject.FindProperty ("m_MipMapFadeDistanceEnd");
			m_Lightmap = serializedObject.FindProperty ("m_Lightmap");

			m_Aniso = serializedObject.FindProperty ("m_TextureSettings.m_Aniso");
			m_FilterMode = serializedObject.FindProperty ("m_TextureSettings.m_FilterMode");
			m_WrapMode = serializedObject.FindProperty ("m_TextureSettings.m_WrapMode");


#if ENABLE_SPRITES
			m_SpriteMode = serializedObject.FindProperty ("m_SpriteMode");
			m_SpritePackingTag = serializedObject.FindProperty ("m_SpritePackingTag");
			m_SpritePixelsToUnits = serializedObject.FindProperty ("m_SpritePixelsToUnits");
			m_SpriteExtrude = serializedObject.FindProperty ("m_SpriteExtrude");
			m_SpriteMeshType = serializedObject.FindProperty("m_SpriteMeshType");
			m_Alignment = serializedObject.FindProperty ("m_Alignment");
			#if ENABLE_SPRITECOLLIDER
			m_SpriteColliderAlphaCutoff = serializedObject.FindProperty ("m_SpriteColliderAlphaCutoff");
			m_SpriteColliderDetail = serializedObject.FindProperty ("m_SpriteColliderDetail");
			#endif
			m_SpritePivot = serializedObject.FindProperty ("m_SpritePivot");
#endif

			m_AlphaIsTransparency = serializedObject.FindProperty("m_AlphaIsTransparency");
		}

		public virtual void OnEnable ()
		{
			CacheSerializedProperties ();

			m_Anims.Add (m_ShowBumpGenerationSettings);
			m_Anims.Add (m_ShowCookieCubeMapSettings);
			m_ShowCookieCubeMapSettings.value = (textureType == TextureImporterType.Cookie && m_GenerateCubemap.intValue != 0);
			m_ShowBumpGenerationSettings.value = m_ConvertToNormalMap.intValue > 0;
			//@TODO change to use spriteMode enum when available
#if ENABLE_SPRITES
			m_Anims.Add (m_ShowGenericSpriteSettings);
			m_Anims.Add (m_ShowManualAtlasGenerationSettings);
			m_ShowGenericSpriteSettings.value = m_SpriteMode.intValue != 0;
			m_ShowManualAtlasGenerationSettings.value = m_SpriteMode.intValue == 2;
#endif
		}

		void SetSerializedPropertySettings (TextureImporterSettings settings)
		{
			m_GrayscaleToAlpha.intValue = settings.grayscaleToAlpha ? 1 : 0;
			m_ConvertToNormalMap.intValue = settings.convertToNormalMap ? 1 : 0;
			m_NormalMap.intValue = settings.normalMap ? 1 : 0;
			m_HeightScale.floatValue = settings.heightmapScale;
			m_NormalMapFilter.intValue = (int)settings.normalMapFilter;
			m_GenerateCubemap.intValue = (int)settings.generateCubemap;
			m_SeamlessCubemap.intValue = settings.seamlessCubemap ? 1 : 0;
			m_BorderMipMap.intValue = settings.borderMipmap ? 1 : 0;
			m_NPOTScale.intValue = (int)settings.npotScale;
			m_IsReadable.intValue = settings.readable ? 1 : 0;
			m_EnableMipMap.intValue = settings.mipmapEnabled ? 1 : 0;
			m_LinearTexture.intValue = settings.linearTexture ? 1 : 0;
			m_MipMapMode.intValue = (int)settings.mipmapFilter;
			m_GenerateMipsInLinearSpace.intValue = settings.generateMipsInLinearSpace ? 1 : 0;
			m_FadeOut.intValue = settings.fadeOut ? 1 : 0;
			m_MipMapFadeDistanceStart.intValue = settings.mipmapFadeDistanceStart;
			m_MipMapFadeDistanceEnd.intValue = settings.mipmapFadeDistanceEnd;
			m_Lightmap.intValue = settings.lightmap ? 1 : 0;

#if ENABLE_SPRITES
			m_SpriteMode.intValue = settings.spriteMode;
			m_SpritePixelsToUnits.floatValue = settings.spritePixelsToUnits;
			m_SpriteExtrude.intValue = (int)settings.spriteExtrude;
			m_SpriteMeshType.intValue = (int)settings.spriteMeshType;
			m_Alignment.intValue = settings.spriteAlignment;
			#if ENABLE_SPRITECOLLIDER
			m_SpriteColliderAlphaCutoff.intValue = settings.spriteColliderAlphaCutoff;
			m_SpriteColliderDetail.floatValue = settings.spriteColliderDetail;
			#endif
#endif
			m_WrapMode.intValue = (int)settings.wrapMode;
			m_FilterMode.intValue = (int)settings.filterMode;
			m_Aniso.intValue = settings.aniso;

			m_AlphaIsTransparency.intValue = settings.alphaIsTransparency ? 1 : 0;
		}

		TextureImporterSettings GetSerializedPropertySettings ()
		{
			return GetSerializedPropertySettings (new TextureImporterSettings ());
		}

		TextureImporterSettings GetSerializedPropertySettings (TextureImporterSettings settings)
		{
			if (!m_GrayscaleToAlpha.hasMultipleDifferentValues)
				settings.grayscaleToAlpha = m_GrayscaleToAlpha.intValue > 0;

			if (!m_ConvertToNormalMap.hasMultipleDifferentValues)
				settings.convertToNormalMap = m_ConvertToNormalMap.intValue > 0;

			if (!m_NormalMap.hasMultipleDifferentValues)
				settings.normalMap = m_NormalMap.intValue > 0;

			if (!m_HeightScale.hasMultipleDifferentValues)
				settings.heightmapScale = m_HeightScale.floatValue;

			if (!m_NormalMapFilter.hasMultipleDifferentValues)
				settings.normalMapFilter = (TextureImporterNormalFilter)m_NormalMapFilter.intValue;

			if (!m_GenerateCubemap.hasMultipleDifferentValues)
				settings.generateCubemap = (TextureImporterGenerateCubemap)m_GenerateCubemap.intValue;

			if (!m_SeamlessCubemap.hasMultipleDifferentValues)
				settings.seamlessCubemap = m_SeamlessCubemap.intValue > 0;

			if (!m_BorderMipMap.hasMultipleDifferentValues)
				settings.borderMipmap = m_BorderMipMap.intValue > 0;

			if (!m_NPOTScale.hasMultipleDifferentValues)
				settings.npotScale = (TextureImporterNPOTScale)m_NPOTScale.intValue;

			if (!m_IsReadable.hasMultipleDifferentValues)
				settings.readable = m_IsReadable.intValue > 0;

			if (!m_LinearTexture.hasMultipleDifferentValues)
				settings.linearTexture = m_LinearTexture.intValue > 0;

			if (!m_EnableMipMap.hasMultipleDifferentValues)
				settings.mipmapEnabled = m_EnableMipMap.intValue > 0;

			if (!m_GenerateMipsInLinearSpace.hasMultipleDifferentValues)
				settings.generateMipsInLinearSpace = m_GenerateMipsInLinearSpace.intValue > 0;

			if (!m_MipMapMode.hasMultipleDifferentValues)
				settings.mipmapFilter = (TextureImporterMipFilter)m_MipMapMode.intValue;

			if (!m_FadeOut.hasMultipleDifferentValues)
				settings.fadeOut = m_FadeOut.intValue > 0;

			if (!m_MipMapFadeDistanceStart.hasMultipleDifferentValues)
				settings.mipmapFadeDistanceStart = m_MipMapFadeDistanceStart.intValue;

			if (!m_MipMapFadeDistanceEnd.hasMultipleDifferentValues)
				settings.mipmapFadeDistanceEnd = m_MipMapFadeDistanceEnd.intValue;

			if (!m_Lightmap.hasMultipleDifferentValues)
				settings.lightmap = m_Lightmap.intValue > 0;

#if ENABLE_SPRITES
			if (!m_SpriteMode.hasMultipleDifferentValues)
				settings.spriteMode = m_SpriteMode.intValue;

			if (!m_SpritePixelsToUnits.hasMultipleDifferentValues)
				settings.spritePixelsToUnits = m_SpritePixelsToUnits.floatValue;

			if (!m_SpriteExtrude.hasMultipleDifferentValues)
				settings.spriteExtrude = (uint)m_SpriteExtrude.intValue;

			if (!m_SpriteMeshType.hasMultipleDifferentValues)
				settings.spriteMeshType = (SpriteMeshType)m_SpriteMeshType.intValue;

			if (!m_Alignment.hasMultipleDifferentValues)
				settings.spriteAlignment = m_Alignment.intValue;

			#if ENABLE_SPRITECOLLIDER
			if (!m_SpriteColliderAlphaCutoff.hasMultipleDifferentValues)
				settings.spriteColliderAlphaCutoff = m_SpriteColliderAlphaCutoff.intValue;

			if (!m_SpriteColliderDetail.hasMultipleDifferentValues)
				settings.spriteColliderDetail = m_SpriteColliderDetail.floatValue;
			#endif

			if (!m_SpritePivot.hasMultipleDifferentValues)
				settings.spritePivot = m_SpritePivot.vector2Value;
#endif

			if (!m_WrapMode.hasMultipleDifferentValues)
				settings.wrapMode = (TextureWrapMode)m_WrapMode.intValue;

			if (!m_FilterMode.hasMultipleDifferentValues)
				settings.filterMode = (FilterMode)m_FilterMode.intValue;

			if (!m_Aniso.hasMultipleDifferentValues)
				settings.aniso = m_Aniso.intValue;


			if (!m_AlphaIsTransparency.hasMultipleDifferentValues)
				settings.alphaIsTransparency = m_AlphaIsTransparency.intValue > 0;

			return settings;
		}

		public override void OnInspectorGUI ()
		{
			if (m_Anims.callback == null)
				m_Anims.callback = Repaint;

			if (s_Styles == null)
				s_Styles = new Styles ();

			bool wasEnabled = GUI.enabled;

			// Texture type
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = textureTypeHasMultipleDifferentValues;
			int newTextureType = EditorGUILayout.IntPopup (s_Styles.textureType, (int)textureType, s_Styles.textureTypeOptions, m_TextureTypeValues);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
			{
				m_TextureType.intValue = newTextureType;

				TextureImporterSettings settings = GetSerializedPropertySettings ();
				settings.ApplyTextureType(textureType, true);
				SetSerializedPropertySettings(settings);

				SyncPlatformSettings();
				ApplySettingsToTexture();
			}

			// Texture type specific GUI
			if (!textureTypeHasMultipleDifferentValues)
			{
				switch (textureType)
				{
					case TextureImporterType.Image:
						ImageGUI ();
						break;
					case TextureImporterType.Bump:
						BumpGUI ();
						break;
					case TextureImporterType.Sprite:
#if ENABLE_SPRITES
						SpriteGUI ();
#endif
						break;
					case TextureImporterType.Cursor:
						break;
					case TextureImporterType.Reflection:
						ReflectionGUI ();
						break;
					case TextureImporterType.Cookie:
						CookieGUI ();
						break;
					case TextureImporterType.Lightmap:
						// no GUI needed
						break;
					case TextureImporterType.Advanced:
						AdvancedGUI ();
						break;
				}
			}

			EditorGUILayout.Space ();

			// Filter mode, aniso, and wrap mode GUI
			PreviewableGUI ();

			SizeAndFormatGUI ();

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();
			ApplyRevertGUI ();
			GUILayout.EndHorizontal();

			// screw this - after lots of retries i have no idea how to poll it only when we change related stuff
			UpdateImportWarning();
			if(m_ImportWarning != null)
				EditorGUILayout.HelpBox(m_ImportWarning, MessageType.Warning);

			GUI.enabled = wasEnabled;
		}

		void PreviewableGUI ()
		{
			EditorGUI.BeginChangeCheck ();

			if (textureType != TextureImporterType.GUI &&
				textureType != TextureImporterType.Sprite &&
				textureType != TextureImporterType.Reflection &&
				textureType != TextureImporterType.Cookie &&
				textureType != TextureImporterType.Lightmap)
			{
				// Wrap mode
				EditorGUI.BeginChangeCheck ();
				EditorGUI.showMixedValue = m_WrapMode.hasMultipleDifferentValues;
				TextureWrapMode wrap = (TextureWrapMode)m_WrapMode.intValue;
				if ((int)wrap == -1)
					wrap = TextureWrapMode.Repeat;
				wrap = (TextureWrapMode)EditorGUILayout.EnumPopup (EditorGUIUtility.TempContent ("Wrap Mode"), wrap);
				EditorGUI.showMixedValue = false;
				if (EditorGUI.EndChangeCheck ())
					m_WrapMode.intValue = (int)wrap;
			}

			// Filter mode
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_FilterMode.hasMultipleDifferentValues;
			FilterMode filter = (FilterMode)m_FilterMode.intValue;
			if ((int)filter == -1)
			{
				if (m_FadeOut.intValue > 0 || m_ConvertToNormalMap.intValue > 0 || m_NormalMap.intValue > 0)
					filter = FilterMode.Trilinear;
				else
					filter = FilterMode.Bilinear;
			}
			filter = (FilterMode)EditorGUILayout.EnumPopup (EditorGUIUtility.TempContent ("Filter Mode"), filter);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				m_FilterMode.intValue = (int)filter;

			if (filter != FilterMode.Point && (m_EnableMipMap.intValue > 0 || textureType == TextureImporterType.Advanced))
			{
				// Aniso
				EditorGUI.BeginChangeCheck ();
				EditorGUI.showMixedValue = m_Aniso.hasMultipleDifferentValues;
				int aniso = m_Aniso.intValue;
				if (aniso == -1)
					aniso = 1;
				aniso = EditorGUILayout.IntSlider("Aniso Level", aniso, 0, 9);
				EditorGUI.showMixedValue = false;
				if (EditorGUI.EndChangeCheck ())
					m_Aniso.intValue = aniso;
			}

			if (EditorGUI.EndChangeCheck ())
				ApplySettingsToTexture ();
		}

		void ApplySettingsToTexture ()
		{

			foreach (AssetImporter importer in targets)
			{
				Texture tex = AssetDatabase.LoadMainAssetAtPath (importer.assetPath) as Texture;
				if (m_Aniso.intValue != -1)
					TextureUtil.SetAnisoLevelNoDirty (tex, m_Aniso.intValue);
				if (m_FilterMode.intValue != -1)
					TextureUtil.SetFilterModeNoDirty (tex, (FilterMode)m_FilterMode.intValue);
				if (m_WrapMode.intValue != -1)
					TextureUtil.SetWrapModeNoDirty (tex, (TextureWrapMode)m_WrapMode.intValue);
			}

			SceneView.RepaintAll ();
		}

		void ImageGUI ()
		{
			ToggleFromInt (m_GrayscaleToAlpha, s_Styles.generateAlphaFromGrayscale);
			ToggleFromInt (m_AlphaIsTransparency, s_Styles.alphaIsTransparency);
		}

		void BumpGUI ()
		{
			ToggleFromInt (m_ConvertToNormalMap, s_Styles.generateFromBump);
			m_ShowBumpGenerationSettings.target = m_ConvertToNormalMap.intValue > 0;
			if (EditorGUILayout.BeginFadeGroup (m_ShowBumpGenerationSettings.faded))
			{
				EditorGUILayout.Slider (m_HeightScale, 0.0F, 0.3F, s_Styles.bumpiness);
				EditorGUILayout.Popup (m_NormalMapFilter, s_Styles.bumpFilteringOptions, s_Styles.bumpFiltering);
			}
			EditorGUILayout.EndFadeGroup();
		}

#if ENABLE_SPRITES
		private void SpriteGUI ()
		{
			// Sprite mode selection
			EditorGUI.BeginChangeCheck();

			if (textureType == TextureImporterType.Advanced)
				EditorGUILayout.IntPopup (m_SpriteMode, s_Styles.spriteModeOptionsAdvanced, new[] {0, 1, 2}, s_Styles.spriteMode);
			else
				EditorGUILayout.IntPopup (m_SpriteMode, s_Styles.spriteModeOptions, new[] { 1, 2 }, s_Styles.spriteMode);

			// Ensure that PropertyField focus will be cleared when we change spriteMode.
			if (EditorGUI.EndChangeCheck())
			{
				GUIUtility.keyboardControl = 0;
			}

			EditorGUI.indentLevel++;

			// Show generic attributes
			m_ShowGenericSpriteSettings.target = (m_SpriteMode.intValue != 0);
			if (EditorGUILayout.BeginFadeGroup (m_ShowGenericSpriteSettings.faded))
			{
				m_SpritePackingTag.stringValue = EditorGUILayout.TextField (s_Styles.spritePackingTag, m_SpritePackingTag.stringValue);
				m_SpritePixelsToUnits.floatValue = EditorGUILayout.FloatField (s_Styles.spritePixelsToUnits, m_SpritePixelsToUnits.floatValue);
				if (textureType == TextureImporterType.Advanced
					&& Application.HasAdvancedLicense()) //Note: tight mesh and extrude is Pro only.
				{
					EditorGUILayout.IntPopup (m_SpriteMeshType, s_Styles.spriteMeshTypeOptions, new[] {0, 1}, s_Styles.spriteMeshType);
					m_SpriteExtrude.intValue = EditorGUILayout.IntSlider(s_Styles.spriteExtrude, m_SpriteExtrude.intValue, 0, 32);
				}

				if (m_SpriteMode.intValue == 1)
				{
					#if ENABLE_SPRITECOLLIDER
					m_SpriteColliderDetail.floatValue = EditorGUILayout.Slider (s_Styles.spriteColliderDetail, m_SpriteColliderDetail.floatValue, 0f, 1f);
					m_SpriteColliderAlphaCutoff.intValue = EditorGUILayout.IntSlider (s_Styles.spriteColliderAlphaCutoff, m_SpriteColliderAlphaCutoff.intValue, 0, 254);
					#endif

					EditorGUILayout.Popup (m_Alignment, s_Styles.spriteAlignmentOptions, s_Styles.spriteAlignment);

					if (m_Alignment.intValue == (int) SpriteAlignment.Custom)
					{
						GUILayout.BeginHorizontal ();
						EditorGUILayout.PropertyField (m_SpritePivot, m_EmptyContent);
						GUILayout.EndHorizontal ();
					}
				}

				EditorGUI.BeginDisabledGroup(targets.Length != 1);

				if (m_SpriteMode.intValue == 2)
				{
					GUILayout.BeginHorizontal();

					bool wasMultipleMode = (target as TextureImporter).spriteImportMode == SpriteImportMode.Multiple;

					GUILayout.FlexibleSpace();
					if (GUILayout.Button ("Sprite Editor"))
					{
						// The texture must be imported in multiple mode
						// in order to bring up the Sprite Editor.
						if (!wasMultipleMode)
							ApplyAndImport ();

						EditorWindow.GetWindow<SpriteEditorWindow>();

						// We reimported the asset which destroyed the editor, so we can't keep running the UI here.
						GUIUtility.ExitGUI();
					}
					GUILayout.EndHorizontal ();
				}

				EditorGUI.EndDisabledGroup();
			}
			EditorGUILayout.EndFadeGroup ();

			EditorGUI.indentLevel--;
		}
#endif

		void ReflectionGUI ()
		{
			ReflectionMappingGUI ();
		}

		void ReflectionMappingGUI ()
		{
			EditorGUI.showMixedValue = m_GenerateCubemap.hasMultipleDifferentValues || m_SeamlessCubemap.hasMultipleDifferentValues;
			EditorGUI.BeginChangeCheck ();
			int value = EditorGUILayout.Popup (s_Styles.refMap, m_GenerateCubemap.intValue - 1, s_Styles.refMapOptions) + 1;
			if (EditorGUI.EndChangeCheck ())
				m_GenerateCubemap.intValue = value;

			ToggleFromInt (m_SeamlessCubemap, s_Styles.seamlessCubemap);

			EditorGUI.showMixedValue = false;
		}

		void CookieGUI ()
		{
			EditorGUI.BeginChangeCheck ();
			CookieMode cm;
			if (m_BorderMipMap.intValue > 0)
				cm = CookieMode.Spot;
			else if (m_GenerateCubemap.intValue != (int)TextureImporterGenerateCubemap.None)
				cm = CookieMode.Point;
			else
				cm = CookieMode.Directional;

			cm = (CookieMode)EditorGUILayout.Popup (s_Styles.cookieType, (int)cm, s_Styles.cookieOptions);
			if (EditorGUI.EndChangeCheck ())
				SetCookieMode (cm);

			m_ShowCookieCubeMapSettings.target = (cm == CookieMode.Point);
			if (EditorGUILayout.BeginFadeGroup (m_ShowCookieCubeMapSettings.faded))
				ReflectionMappingGUI ();
			EditorGUILayout.EndFadeGroup ();

			ToggleFromInt (m_GrayscaleToAlpha, s_Styles.generateAlphaFromGrayscale);

		}

		void SetCookieMode (CookieMode cm)
		{
			switch (cm)
			{
				case CookieMode.Spot:
					m_BorderMipMap.intValue = 1;
					m_WrapMode.intValue = (int)TextureWrapMode.Clamp;
					m_GenerateCubemap.intValue = (int)TextureImporterGenerateCubemap.None;
					break;
				case CookieMode.Point:
					m_BorderMipMap.intValue = 0;
					m_WrapMode.intValue = (int)TextureWrapMode.Clamp;
					m_GenerateCubemap.intValue = (int)TextureImporterGenerateCubemap.SimpleSpheremap;
					break;
				case CookieMode.Directional:
					m_BorderMipMap.intValue = 0;
					m_WrapMode.intValue = (int)TextureWrapMode.Repeat;
					m_GenerateCubemap.intValue = (int)TextureImporterGenerateCubemap.None;
					break;
			}
		}

		void BeginToggleGroup (SerializedProperty property, GUIContent label)
		{
			EditorGUI.showMixedValue = property.hasMultipleDifferentValues;
			EditorGUI.BeginChangeCheck ();
			int value = EditorGUILayout.BeginToggleGroup (label, property.intValue > 0) ? 1 : 0;
			if (EditorGUI.EndChangeCheck ())
				property.intValue = value;
			EditorGUI.showMixedValue = false;
		}

		void AdvancedGUI ()
		{
			var importer = target as TextureImporter;
			if (importer == null)
				return;

			// why are we doing this every update?
			var textureHeight = 0;
			var textureWidth = 0;
			importer.GetWidthAndHeight (ref textureWidth, ref textureHeight);
			var POT = IsPowerOfTwo (textureHeight) && IsPowerOfTwo (textureWidth);

			// disable NPOT scale for textures that are power of two already
			EditorGUI.BeginDisabledGroup (POT);
			EnumPopup (m_NPOTScale, typeof (TextureImporterNPOTScale), s_Styles.npot);
			EditorGUI.EndDisabledGroup ();

			EditorGUI.BeginDisabledGroup (!POT && m_NPOTScale.intValue == (int)TextureImporterNPOTScale.None);
			EnumPopup (m_GenerateCubemap, typeof (TextureImporterGenerateCubemap), s_Styles.generateCubemap);
			EditorGUI.EndDisabledGroup ();

			ToggleFromInt (m_IsReadable, s_Styles.readWrite);

			AdvancedTextureType type = AdvancedTextureType.Default;
			if (m_NormalMap.intValue > 0)
				type = AdvancedTextureType.NormalMap;
			else if (m_Lightmap.intValue > 0)
				type = AdvancedTextureType.LightMap;

			EditorGUI.BeginChangeCheck();
			type = (AdvancedTextureType) EditorGUILayout.Popup("Import Type", (int)type, new[] { "Default", "Normal Map", "Lightmap" });
			if (EditorGUI.EndChangeCheck())
			{
				switch (type)
				{
					case AdvancedTextureType.Default:
						m_NormalMap.intValue = 0;
						m_Lightmap.intValue = 0;
						m_ConvertToNormalMap.intValue = 0;
						break;
					case AdvancedTextureType.NormalMap:
						m_NormalMap.intValue = 1;
						m_Lightmap.intValue = 0;
						m_LinearTexture.intValue = 1;
						break;
					case AdvancedTextureType.LightMap:
						m_NormalMap.intValue = 0;
						m_Lightmap.intValue = 1;
						m_ConvertToNormalMap.intValue = 0;
						m_LinearTexture.intValue = 1;
						break;
				}
			}

			EditorGUI.indentLevel++;
			if (type == AdvancedTextureType.NormalMap)
			{
				// Generate bumpmap
				EditorGUI.BeginChangeCheck();
				BumpGUI();
				if (EditorGUI.EndChangeCheck())
					SyncPlatformSettings();
			}
			else if (type == AdvancedTextureType.Default)
			{
				ToggleFromInt(m_GrayscaleToAlpha, s_Styles.generateAlphaFromGrayscale);
				ToggleFromInt(m_AlphaIsTransparency, s_Styles.alphaIsTransparency);
				ToggleFromInt(m_LinearTexture, s_Styles.linearTexture);

#if ENABLE_SPRITES
				SpriteGUI ();
#endif
			}
			EditorGUI.indentLevel--;


			ToggleFromInt (m_EnableMipMap, s_Styles.generateMipMaps);
			if (m_EnableMipMap.boolValue && !m_EnableMipMap.hasMultipleDifferentValues)
			{
				EditorGUI.indentLevel++;
				ToggleFromInt (m_GenerateMipsInLinearSpace, s_Styles.mipMapsInLinearSpace);
				ToggleFromInt (m_BorderMipMap, s_Styles.borderMipMaps);
				EditorGUILayout.Popup (m_MipMapMode, s_Styles.mipMapFilterOptions, s_Styles.mipMapFilter);
	
				// Mipmap fadeout
				ToggleFromInt (m_FadeOut, s_Styles.mipmapFadeOutToggle);
				if (m_FadeOut.intValue > 0)
				{
					EditorGUI.indentLevel++;
					EditorGUI.BeginChangeCheck ();
					float min = m_MipMapFadeDistanceStart.intValue;
					float max = m_MipMapFadeDistanceEnd.intValue;
					EditorGUILayout.MinMaxSlider (s_Styles.mipmapFadeOut, ref min, ref max, 0, 10);
					if (EditorGUI.EndChangeCheck ())
					{
						m_MipMapFadeDistanceStart.intValue = Mathf.RoundToInt (min);
						m_MipMapFadeDistanceEnd.intValue = Mathf.RoundToInt (max);
					}
					EditorGUI.indentLevel--;
				}
				EditorGUI.indentLevel--;
			}
		}

		void SyncPlatformSettings ()
		{
			foreach (PlatformSetting ps in m_PlatformSettings)
				ps.Sync ();
		}

		static string[] BuildTextureStrings (int[] texFormatValues)
		{
			string[] retval = new string[texFormatValues.Length];
			for (int i=0;i<texFormatValues.Length;i++)
			{
				int val = texFormatValues[i];

				switch (val)
				{
					case -1:
						retval[i] = "Automatic Compressed";
						break;
					case -2:
						retval[i] = "Automatic 16 bits";
						break;
					case -3:
						retval[i] = "Automatic Truecolor";
						break;
					default:
						retval[i] = " " + TextureUtil.GetTextureFormatString((TextureFormat)val);
						break;
				}
			}
			return retval;
		}

		static TextureImporterFormat MakeTextureFormatHaveAlpha (TextureImporterFormat format)
		{
			switch (format)
			{
				case TextureImporterFormat.PVRTC_RGB2:
					return TextureImporterFormat.PVRTC_RGBA2;
				case TextureImporterFormat.PVRTC_RGB4:
					return TextureImporterFormat.PVRTC_RGBA4;
				case TextureImporterFormat.RGB16:
					return TextureImporterFormat.ARGB16;
				case TextureImporterFormat.RGB24:
					return TextureImporterFormat.ARGB32;
				case TextureImporterFormat.DXT1:
					return TextureImporterFormat.DXT5;
			}
			return format;
		}

		protected void SizeAndFormatGUI ()
		{
			BuildPlayerWindow.BuildPlatform[] validPlatforms = GetBuildPlayerValidPlatforms ().ToArray ();
			GUILayout.Space (10);
			int shownTextureFormatPage = EditorGUILayout.BeginPlatformGrouping (validPlatforms, s_Styles.defaultPlatform);
			PlatformSetting realPS = m_PlatformSettings[shownTextureFormatPage + 1];

			if (!realPS.isDefault)
			{
				EditorGUI.BeginChangeCheck ();
				EditorGUI.showMixedValue = realPS.overriddenIsDifferent;
				bool newOverride = GUILayout.Toggle (realPS.overridden, "Override for " + realPS.name);
				EditorGUI.showMixedValue = false;
				if (EditorGUI.EndChangeCheck ())
				{
					realPS.SetOverriddenForAll (newOverride);
					SyncPlatformSettings ();
				}
			}

			// Disable size and format GUI if not overwritten for all objects
			bool notAllOverriddenForThisPlatform = (!realPS.isDefault && !realPS.allAreOverridden);
			EditorGUI.BeginDisabledGroup (notAllOverriddenForThisPlatform);

			// Max texture size
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = realPS.overriddenIsDifferent || realPS.maxTextureSizeIsDifferent;
			int maxTextureSize = EditorGUILayout.IntPopup (s_Styles.maxSize.text, realPS.maxTextureSize, kMaxTextureSizeStrings, kMaxTextureSizeValues);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
			{
				realPS.SetMaxTextureSizeForAll (maxTextureSize);
				SyncPlatformSettings ();
			}

			// Texture format
			int[] formatValuesForAll = null;
			string[] formatStringsForAll = null;
			bool formatOptionsAreDifferent = false;

			int formatForAll = 0;
			bool formatIsDifferent = false;
			// Full format is the concrete format that the potentially simple format will be converted to.
			// In the advanced texture type the full format is what we show directly.
			int fullFormatForAll = 0;
			bool fullFormatIsDifferent = false;

			for (int i=0; i<targets.Length; i++)
			{
				TextureImporter imp = targets[i] as TextureImporter;
				TextureImporterType textureTypeForThis = textureTypeHasMultipleDifferentValues ? imp.textureType : textureType;
				TextureImporterSettings settings = realPS.GetSettings (imp);
				int[] formatValues;
				string[] formatStrings;
				int format = (int)realPS.textureFormats[i];

				int fullFormat = format;
				if (!realPS.isDefault && format < 0)
				{
					fullFormat = (int)TextureImporter.SimpleToFullTextureFormat2 ((TextureImporterFormat)fullFormat,
																			  textureTypeForThis,
																			  settings,
																			  imp.DoesSourceTextureHaveAlpha (),
																			  imp.DoesSourceTextureHaveColor (),
																			  realPS.m_Target);
				}
				if (settings.normalMap && !IsGLESMobileTargetPlatform(realPS.m_Target))
					fullFormat = (int)MakeTextureFormatHaveAlpha ((TextureImporterFormat)fullFormat);

				switch (textureTypeForThis)
				{
					case TextureImporterType.Cookie:
						formatValues = new int[] { 0 };
						formatStrings = new string[] { "8 Bit Alpha" };
						format = 0;
						break;
					case TextureImporterType.Advanced:
						format = fullFormat;

						// on gles targets we use rgb normal maps so no need to split formats
						if(IsGLESMobileTargetPlatform(realPS.m_Target))
						{
							if (s_TextureFormatStringsiPhone == null)
								s_TextureFormatStringsiPhone = BuildTextureStrings (kTextureFormatsValueiPhone);
							if (s_TextureFormatStringsAndroid == null)
								s_TextureFormatStringsAndroid = BuildTextureStrings (kTextureFormatsValueAndroid);

							if (realPS.m_Target == BuildTarget.iPhone)
							{
								formatValues = kTextureFormatsValueiPhone;
								formatStrings = s_TextureFormatStringsiPhone;
							}
							else
							{
								formatValues = kTextureFormatsValueAndroid;
								formatStrings = s_TextureFormatStringsAndroid;
							}
						}
						else if (!settings.normalMap)
						{
							if (s_TextureFormatStringsAll == null)
								s_TextureFormatStringsAll = BuildTextureStrings(TextureFormatsValueAll);
							if (s_TextureFormatStringsWii == null)
								s_TextureFormatStringsWii = BuildTextureStrings(kTextureFormatsValueWii);
							if (s_TextureFormatStringsFlash == null)
								s_TextureFormatStringsFlash = BuildTextureStrings (kTextureFormatsValueFlash);
							if (s_TextureFormatStringsGLESEmu == null)
								s_TextureFormatStringsGLESEmu = BuildTextureStrings(kTextureFormatsValueGLESEmu);
							if (s_TextureFormatStringsWeb == null)
								s_TextureFormatStringsWeb = BuildTextureStrings (kTextureFormatsValueWeb);

							if (realPS.isDefault)
							{
								formatValues = TextureFormatsValueAll;
								formatStrings = s_TextureFormatStringsAll;
							}
							else if (realPS.m_Target == BuildTarget.Wii)
							{
								formatValues = kTextureFormatsValueWii;
								formatStrings = s_TextureFormatStringsWii;
							}
							else if (realPS.m_Target == BuildTarget.StandaloneGLESEmu)
							{
								formatValues = kTextureFormatsValueGLESEmu;
								formatStrings = s_TextureFormatStringsGLESEmu;
							}
							else if (realPS.m_Target == BuildTarget.FlashPlayer)
							{
								formatValues = kTextureFormatsValueFlash;
								formatStrings = s_TextureFormatStringsFlash;
							}
							else
							{
								formatValues = kTextureFormatsValueWeb;
								formatStrings = s_TextureFormatStringsWeb;
							}
						}
						else
						{
							if (s_NormalFormatStringsAll == null)
								s_NormalFormatStringsAll = BuildTextureStrings(NormalFormatsValueAll);
							if (s_NormalFormatStringsFlash == null)
								s_NormalFormatStringsFlash = BuildTextureStrings (kNormalFormatsValueFlash);
							if (s_NormalFormatStringsWeb == null)
								s_NormalFormatStringsWeb = BuildTextureStrings (kNormalFormatsValueWeb);

							if (realPS.isDefault)
							{
								formatValues = NormalFormatsValueAll;
								formatStrings = s_NormalFormatStringsAll;
							}
							else if (realPS.m_Target == BuildTarget.FlashPlayer)
							{
								formatValues = kNormalFormatsValueFlash;
								formatStrings = s_NormalFormatStringsFlash;
							}
							else
							{
								formatValues = kNormalFormatsValueWeb;
								formatStrings = s_NormalFormatStringsWeb;
							}
						}
						break;
					default:
						formatValues = m_TextureFormatValues;
						formatStrings = s_Styles.textureFormatOptions;
						// Format is one of the simple texture formats - if not, we make sure it is
						if (format >= 0)
							format = (int)TextureImporter.FullToSimpleTextureFormat ((TextureImporterFormat)format);
						// The simple formats use negative numbers (excluding 0).
						// We convert to positive (including 0) to make them work with the dropdown.
						format = -1 - format;
						break;
				}

				// Check if values are the same
				if (i == 0)
				{
					formatValuesForAll = formatValues;
					formatStringsForAll = formatStrings;
					formatForAll = format;
					fullFormatForAll = fullFormat;
				}
				else
				{
					if (format != formatForAll)
						formatIsDifferent = true;
					if (fullFormat != fullFormatForAll)
						fullFormatIsDifferent = true;

					if (!formatValues.SequenceEqual (formatValuesForAll) || !formatStrings.SequenceEqual (formatStringsForAll))
					{
						formatOptionsAreDifferent = true;
						break;
					}
				}
			}

			EditorGUI.BeginDisabledGroup (formatOptionsAreDifferent || formatStringsForAll.Length == 1);
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = formatOptionsAreDifferent || formatIsDifferent;
			formatForAll = EditorGUILayout.IntPopup (s_Styles.textureFormat, formatForAll, EditorGUIUtility.TempContent (formatStringsForAll), formatValuesForAll);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
			{
				if (textureType != TextureImporterType.Advanced)
				{
					// We show simple formats if not on the advanced importer type.
					// The simple formats use negative numbers (excluding 0),
					// but we converted it to positive for the dropdown (see comment further up).
					// Here we convert it back to negative.
					formatForAll = -1 - formatForAll;
				}
				realPS.SetTextureFormatForAll ((TextureImporterFormat)formatForAll);
				SyncPlatformSettings ();
			}
			EditorGUI.EndDisabledGroup ();

			// compression quality
			if (!fullFormatIsDifferent && ArrayUtility.Contains<TextureImporterFormat> (kFormatsWithCompressionSettings, (TextureImporterFormat)fullFormatForAll))
			{
				EditorGUI.BeginChangeCheck ();
				EditorGUI.showMixedValue = realPS.overriddenIsDifferent || realPS.compressionQualityIsDifferent;
				int compressionQuality = EditCompressionQuality(realPS.m_Target, realPS.compressionQuality);
				EditorGUI.showMixedValue = false;
				if (EditorGUI.EndChangeCheck ())
				{
					realPS.SetCompressionQualityForAll (compressionQuality);
					SyncPlatformSettings ();
				}
			}

			EditorGUI.EndDisabledGroup ();

			EditorGUILayout.EndPlatformGrouping();
		}

		private int EditCompressionQuality (BuildTarget target, int compression)
		{
			if( target==BuildTarget.iPhone || target==BuildTarget.Android || target==BuildTarget.BB10 || target==BuildTarget.Tizen)
			{
				int compressionMode = 1;
				if(compression == (int)TextureCompressionQuality.Fast)
					compressionMode = 0;
				else if(compression == (int)TextureCompressionQuality.Best)
					compressionMode = 2;

				int ret = EditorGUILayout.Popup (s_Styles.compressionQuality, compressionMode, s_Styles.mobileCompressionQualityOptions);

				switch(ret)
				{
					case 0 : return (int)TextureCompressionQuality.Fast;
					case 1 : return (int)TextureCompressionQuality.Normal;
					case 2 : return (int)TextureCompressionQuality.Best;

					default: return (int)TextureCompressionQuality.Normal;
				}
			}
			else if (target == BuildTarget.FlashPlayer)
			{
				return EditorGUILayout.IntSlider(s_Styles.compressionQuality, compression, 0, 100);
			}

			return compression;
		}

		private static bool IsPowerOfTwo (int f)
		{
			return ((f & (f - 1)) == 0);
		}

		public static BuildPlayerWindow.BuildPlatform[] GetBuildPlayerValidPlatforms ()
		{
			List<BuildPlayerWindow.BuildPlatform> validPlatforms = BuildPlayerWindow.GetValidPlatforms ();
			List<BuildPlayerWindow.BuildPlatform> filtered = new List<BuildPlayerWindow.BuildPlatform> ();

			foreach (BuildPlayerWindow.BuildPlatform bp in validPlatforms)
			{
				// Nacl is handled as the webplayer target for the purpose of texture import settings.
				// This is really hacky, the whole build target group & texture import targets really needs a re-write...
				if (bp.targetGroup == BuildTargetGroup.NaCl)
					continue;
			
				filtered.Add(bp);
			}
			return filtered.ToArray();
		}


		public virtual void BuildTargetList ()
		{
			BuildPlayerWindow.BuildPlatform[] validPlatforms = GetBuildPlayerValidPlatforms ();

			m_PlatformSettings = new List<PlatformSetting> ();
			m_PlatformSettings.Add (new PlatformSetting ("", BuildTarget.StandaloneWindows, this));

			foreach (BuildPlayerWindow.BuildPlatform bp in validPlatforms)
				m_PlatformSettings.Add (new PlatformSetting (bp.name, bp.DefaultTarget, this));
		}

		internal override bool HasModified ()
		{
			if (base.HasModified ())
				return true;

			foreach (PlatformSetting ps in m_PlatformSettings)
			{
				if (ps.HasChanged ())
					return true;
			}

			return false;
		}

		internal override void ResetValues ()
		{
			base.ResetValues ();

			CacheSerializedProperties ();

			BuildTargetList ();
			Assert.IsFalse (HasModified (), "TextureImporter settings are marked as modified after calling Reset.");
			ApplySettingsToTexture ();
		}

		internal override void Apply ()
		{
			base.Apply ();
			SyncPlatformSettings ();
			foreach (PlatformSetting ps in m_PlatformSettings)
				ps.Apply ();
		}

		static readonly string[] kMaxTextureSizeStrings = { "32", "64", "128", "256", "512", "1024", "2048", "4096" };
		static readonly int[] kMaxTextureSizeValues = { 32, 64, 128, 256, 512, 1024, 2048, 4096 };
	}
}
