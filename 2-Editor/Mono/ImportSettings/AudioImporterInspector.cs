using UnityEngine;
using UnityEditor;

namespace UnityEditor
{
[CustomEditor ( typeof ( AudioImporter ) )]
internal class AudioImporterInspector : AssetImporterInspector
{

	private AudioImporterFormat m_format;
	private int m_compressionBitrate;
	private AudioImporterLoadType m_loadType;
	private bool m_3D;
	private bool m_Hardware;
	private bool m_Loopable;

	private bool m_ForceToMono;

	private int m_durationMS;
	private int m_origchannels;
	private bool m_iscompressible;
	private AudioType m_type;
	private int m_maxbitrate;
	private int m_minbitrate;
	private AudioType m_compressedType;

	static string[] formatLabels = new string[2];

	internal override bool HasModified ()
	{
		AudioImporter importer = target as AudioImporter;
		return ( importer.format != m_format ) || ( importer.compressionBitrate != m_compressionBitrate ) || ( importer.loadType != m_loadType
				|| ( importer.threeD != m_3D ) || ( importer.forceToMono != m_ForceToMono ) || ( importer.hardware != m_Hardware ) || ( importer.loopable != m_Loopable ) );
	}

	internal override void ResetValues()
	{
		AudioImporter importer = target as AudioImporter;

		m_format = importer.format;
		m_loadType = importer.loadType;
		m_compressionBitrate = importer.compressionBitrate;
		m_3D = importer.threeD;
		m_ForceToMono = importer.forceToMono;
		m_Hardware = importer.hardware;
		m_Loopable = importer.loopable;
		
		// cache these
		importer.updateOrigData();
		m_durationMS = importer.durationMS;
		m_origchannels = importer.origChannelCount;
		m_iscompressible = importer.origIsCompressible;
		m_type = importer.origType;

		// Can't get conversion type if clip didn't import correctly (m_Duration == 0)
		if ( m_durationMS != 0 )
			m_compressedType = AudioUtil.GetPlatformConversionType( m_type, 
																BuildPipeline.GetBuildTargetGroup ( EditorUserBuildSettings.activeBuildTarget ), 
																AudioImporterFormat.Compressed);

		m_minbitrate = importer.minBitrate ( m_compressedType ) / 1000;
		m_maxbitrate = importer.maxBitrate ( m_compressedType ) / 1000;
	}

	internal override void Apply ()
	{
		AudioImporter importer = target as AudioImporter;

		importer.format = m_format;
		importer.loadType = m_loadType;
		importer.compressionBitrate = m_compressionBitrate;
		importer.threeD = m_3D;
		importer.forceToMono = m_ForceToMono;
		importer.hardware = m_Hardware;
		importer.loopable = m_Loopable;
	}

	public override void OnInspectorGUI ()
	{
		BuildTargetGroup targetGroup = BuildPipeline.GetBuildTargetGroup ( EditorUserBuildSettings.activeBuildTarget );
		AudioImporter importer = target as AudioImporter;

		if ( importer != null )
		{
			switch (targetGroup)
			{
				
				case BuildTargetGroup.FlashPlayer:
					{
						// Separate code path for Flash because it only supports so little.
						// In this case it's fewer lines of code this way.
						m_3D = EditorGUILayout.Toggle("3D Sound", m_3D);
					}
					break;
				case BuildTargetGroup.Wii:
					{
						// Duplicate all the code for Wii? This is a mess; just think if we did that for every platform.
						
						// Both options are always enabled
						formatLabels[0] = "Native (" + m_type + ")";
						formatLabels[1] = "Compressed (" + m_compressedType + ")";
						int i = EditorGUILayout.Popup("Audio Format", (int) (m_format) + 1, formatLabels);
						m_format = (AudioImporterFormat) (i - 1);

						m_3D = EditorGUILayout.Toggle("3D Sound", m_3D);
						
						// On Wii Compressed Audio format (GCAPPCM always has only one channel)
						if (m_format != AudioImporterFormat.Compressed && m_origchannels > 1)
						{
							m_ForceToMono = EditorGUILayout.Toggle("Force to mono", m_ForceToMono);
						}

						string[] loadtypeLabels = null;
						// If GCADPCM is chosen "Decompress on load" and "Compressed in memory" has the same meaning
						if (m_format == AudioImporterFormat.Compressed) loadtypeLabels = new string[] {"Compressed in memory", "Stream from disc"};
						else loadtypeLabels = new string[] { "Decompress on load", "Compressed in memory", "Stream from disc" };

						// Map m_loadType to a new index according to loadtypeLabels
						int loadType = loadtypeLabels.Length == 2 ? ((int)m_loadType - 1) : (int)m_loadType;
						loadType = Mathf.Clamp(loadType, 0, loadtypeLabels.Length - 1);
						loadType = EditorGUILayout.Popup("Load type", (int)loadType, loadtypeLabels);
						m_loadType = (AudioImporterLoadType)(loadtypeLabels.Length == 2 ? loadType + 1 : loadType);

						if (m_format == AudioImporterFormat.Compressed)
						{
							m_Loopable = EditorGUILayout.Toggle(new GUIContent("Gapless looping", ""), m_Loopable);
						}
					}
					break;
				default:
					{
						EditorGUI.BeginDisabledGroup((m_type == AudioType.MPEG) || (m_type == AudioType.OGGVORBIS) || !m_iscompressible);
						formatLabels[0] = "Native (" + m_type + ")";
						formatLabels[1] = "Compressed (" + m_compressedType + ")";
						int i = EditorGUILayout.Popup("Audio Format", (int) (m_format) + 1, formatLabels);
						m_format = (AudioImporterFormat) (i - 1);
						EditorGUI.EndDisabledGroup();

						m_3D = EditorGUILayout.Toggle("3D Sound", m_3D);
						var canBeForcedToMono = (m_origchannels > 1) &&
									  (importer.origIsMonoForcable ||
									   (m_format == AudioImporterFormat.Compressed && m_iscompressible));
						EditorGUI.BeginDisabledGroup(!canBeForcedToMono);
						m_ForceToMono = EditorGUILayout.Toggle("Force to mono", m_ForceToMono);
						EditorGUI.EndDisabledGroup();

						string[] loadtypeLabels = null;
						if (m_format == AudioImporterFormat.Compressed)
						{
							loadtypeLabels =  new string[] {"Decompress on load", "Compressed in memory", "Stream from disc"};
							m_loadType = (AudioImporterLoadType) EditorGUILayout.Popup("Load type", (int) m_loadType, loadtypeLabels);
						}
						else
						{
							loadtypeLabels =  new string[] {"Load into memory", "Stream from disc"};
							int loadType = Mathf.Clamp((int)m_loadType - 1, 0, 1);
							
							EditorGUI.BeginChangeCheck ();
							loadType = EditorGUILayout.Popup("Load type", (int) loadType, loadtypeLabels);
							if (EditorGUI.EndChangeCheck ())
							{
								m_loadType = (AudioImporterLoadType)(loadType + 1);
							}
						}	

						EditorGUI.BeginDisabledGroup((targetGroup != BuildTargetGroup.iPhone) ||
									(m_format != AudioImporterFormat.Compressed));
						m_Hardware = EditorGUILayout.Toggle("Hardware decoding", m_Hardware);
						EditorGUI.EndDisabledGroup();

						EditorGUI.BeginDisabledGroup(!(m_iscompressible && (m_compressedType == AudioType.MPEG || m_compressedType == AudioType.XMA) &&
									  (m_format == AudioImporterFormat.Compressed)));
						m_Loopable =
							EditorGUILayout.Toggle(
								new GUIContent("Gapless looping",
											   "Perform special MPEG encoding and stretch to loop the sound perfectly"),
								m_Loopable);
						EditorGUI.EndDisabledGroup();

						if (m_durationMS != 0)
						{
							EditorGUI.BeginDisabledGroup(m_format != AudioImporterFormat.Compressed);
							int t;
							if (targetGroup == BuildTargetGroup.XBOX360)
							{
								// [1;100] gives [58000;256000] and maps to [0.01;1.00]
								t = (56 + 2 * GetQualityGUI()) * 1000;
							}
							else
							{
								t = GetBitRateGUI(importer) * 1000;	
							}
							if ( GUI.changed )
							{
								m_compressionBitrate = t ;
							}
							EditorGUI.EndDisabledGroup();
						}
						else
						{
							GUILayout.BeginHorizontal();
							GUILayout.Label("Unity failed to import this audio file. Try to reimport.");
							GUILayout.EndHorizontal();
						}
					}
					break;
			}

		}

		GUI.enabled = true;

		GUI.changed = false;
		ApplyRevertGUI ();
		if ( GUI.changed )
			GUIUtility.ExitGUI();

	}

	private int GetBitRateGUI(AudioImporter importer)
	{
		int bitrate = m_compressionBitrate < 0 ? importer.defaultBitrate / 1000 : m_compressionBitrate / 1000 ;
				
		int t = EditorGUILayout.IntSlider ( "Compression (kbps)", bitrate, m_minbitrate, m_maxbitrate );
		GUILayout.BeginHorizontal();
		GUILayout.FlexibleSpace();
		if ( m_format == AudioImporterFormat.Compressed )
		{
			float fileSizesec = ( float ) m_durationMS / 1000.0f;
			int newFileSize = ( int ) ( fileSizesec * ( ( float ) ( bitrate * 1000.0f ) / 4.0f ) );

			GUILayout.Label ( string.Format ( "New file size : {0:000}", EditorUtility.FormatBytes ( newFileSize ) ) );
		}
		GUILayout.EndHorizontal();
		return t;
	}

	private int GetQualityGUI()
	{
		float bitrateF = m_compressionBitrate < 0 ? 50.0f : (m_compressionBitrate / 1000.0f - 56) / 2.0f;
		int bitrate = (int)System.Math.Round(bitrateF);

		int t = EditorGUILayout.IntSlider("XMA Quality (1-100)", bitrate, 1, 100);
		return t;
	}

}
}
