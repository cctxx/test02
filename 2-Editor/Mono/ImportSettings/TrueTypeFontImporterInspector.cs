using UnityEngine;
using UnityEditor;
using System.Linq;
using System.IO;

namespace UnityEditor {
[CustomEditor (typeof (TrueTypeFontImporter))]
[CanEditMultipleObjects]
internal class TrueTypeFontImporterInspector : AssetImporterInspector
{
	SerializedProperty m_FontSize;
	SerializedProperty m_TextureCase;
	SerializedProperty m_IncludeFontData;
	SerializedProperty m_FontNamesArraySize;
	SerializedProperty m_CustomCharacters;
	SerializedProperty m_FontRenderingMode;
	
	string m_FontNamesString = "";
	string m_DefaultFontNamesString = "";
	bool? m_FormatSupported = null;
	
	
	void OnEnable ()
	{
		m_FontSize = serializedObject.FindProperty ("m_FontSize");
		m_TextureCase = serializedObject.FindProperty ("m_ForceTextureCase");
		m_IncludeFontData = serializedObject.FindProperty ("m_IncludeFontData");
		m_FontNamesArraySize = serializedObject.FindProperty ("m_FontNames.Array.size");
		m_CustomCharacters = serializedObject.FindProperty ("m_CustomCharacters");
		m_FontRenderingMode = serializedObject.FindProperty ("m_FontRenderingMode");
		
		// We don't want to expose GUI for setting included fonts when selecting multiple fonts
		if (targets.Length == 1)
		{
			m_DefaultFontNamesString = GetDefaultFontNames ();
			m_FontNamesString = GetFontNames ();
		}
	}
		
	string GetDefaultFontNames ()
	{
		TrueTypeFontImporter importer = target as TrueTypeFontImporter;
		return importer.fontTTFName;
	}
	
	string GetFontNames ()
	{
		TrueTypeFontImporter importer = target as TrueTypeFontImporter;

		string str = "";
		string[] names = importer.fontNames;
		for (int i=0; i<names.Length; i++)
		{
			str += names[i];
			if (i < names.Length-1)
				str += ", ";
		}
		if (str == "")
			str = m_DefaultFontNamesString;
		return str;
	}
	
	void SetFontNames (string fontNames)
	{
		string[] names;
		if (fontNames == m_DefaultFontNamesString)
		{
			// This is the same as the initial state the data has
			// when the import settings have never been touched.
			names = new string[] { "" };
		}
		else
		{
			// Split into array of font names
			names = fontNames.Split (new char[] {','});
			for (int i=0; i<names.Length; i++)
				names[i] = names[i].Trim();
		}
		
		m_FontNamesArraySize.intValue = names.Length;
		SerializedProperty fontNameProp = m_FontNamesArraySize.Copy ();
		for (int i=0; i<names.Length; i++)
		{
			fontNameProp.Next (false);
			fontNameProp.stringValue = names[i];
		}
	}
	
	private void ShowFormatUnsupportedGUI()
	{
		GUILayout.Space(5);
		EditorGUILayout.HelpBox ("Format of selected font is not supported by Unity.", MessageType.Warning);
	}
	
	static GUIContent[] kCharacterStrings =
	{
		new GUIContent ("Dynamic"),
		new GUIContent ("Unicode"),
		new GUIContent ("ASCII default set"),
		new GUIContent ("ASCII upper case"),
		new GUIContent ("ASCII lower case"),
		new GUIContent ("Custom set")
	};
	static int[] kCharacterValues =
	{
		(int)FontTextureCase.Dynamic,
		(int)FontTextureCase.Unicode,
		(int)FontTextureCase.ASCII, 
		(int)FontTextureCase.ASCIIUpperCase,
		(int)FontTextureCase.ASCIILowerCase,
		(int)FontTextureCase.CustomSet, 
	};

	static GUIContent[] kRenderingModeStrings =
	{
		new GUIContent ("Smooth"),
		new GUIContent ("Hinted Smooth"),
		new GUIContent ("Hinted Raster"),
		new GUIContent ("OS Default"),
	};
	static int[] kRenderingModeValues =
	{
		(int)FontRenderingMode.Smooth,
		(int)FontRenderingMode.HintedSmooth,
		(int)FontRenderingMode.HintedRaster,
		(int)FontRenderingMode.OSDefault,
	};
	
	static string GetUniquePath(string basePath, string extension)
	{
		for (int i=0;i<10000;i++)
		{
			string path = basePath + (i==0?"":""+i) + "." + extension;
			if (!File.Exists(path))
				return path;
		}
		return "";
	}

	[MenuItem ("CONTEXT/TrueTypeFontImporter/Create Editable Copy")]
	static void CreateEditableCopy (MenuCommand command)
	{
		TrueTypeFontImporter importer = command.context as TrueTypeFontImporter;
		if (importer.fontTextureCase == FontTextureCase.Dynamic)
		{
			EditorUtility.DisplayDialog  (
				"Cannot generate editabled font asset for dynamic fonts", 
				"Please reimport the font in a different mode.", 
				"Ok");
			return;
		}
		var basePath = Path.GetDirectoryName(importer.assetPath) + "/" + Path.GetFileNameWithoutExtension(importer.assetPath);
		EditorGUIUtility.PingObject(importer.GenerateEditableFont(GetUniquePath(basePath+"_copy","fontsettings")));
	}

	public override void OnInspectorGUI ()
	{
		if (!m_FormatSupported.HasValue)
		{
			m_FormatSupported = true;
			foreach (Object target in targets)
			{
				TrueTypeFontImporter importer = target as TrueTypeFontImporter;
				if (importer == null || !importer.IsFormatSupported ())
					m_FormatSupported = false;
			}
		}
		
		if (m_FormatSupported == false)
		{
			ShowFormatUnsupportedGUI ();
			return;
		}
		
		EditorGUILayout.PropertyField (m_FontSize);
		if (m_FontSize.intValue < 1)
			m_FontSize.intValue = 1;
		if (m_FontSize.intValue > 500)
			m_FontSize.intValue = 500;

		EditorGUILayout.IntPopup (m_FontRenderingMode, kRenderingModeStrings, kRenderingModeValues, new GUIContent ("Rendering Mode"));
		EditorGUILayout.IntPopup (m_TextureCase, kCharacterStrings, kCharacterValues, new GUIContent ("Character"));
		
		if (!m_TextureCase.hasMultipleDifferentValues)
		{
			if ((FontTextureCase)m_TextureCase.intValue != FontTextureCase.Dynamic)
			{
				if ((FontTextureCase)m_TextureCase.intValue == FontTextureCase.CustomSet)
				{
					// Characters included
					EditorGUI.BeginChangeCheck ();
					GUILayout.BeginHorizontal ();
					EditorGUILayout.PrefixLabel ("Custom Chars");
					EditorGUI.showMixedValue = m_CustomCharacters.hasMultipleDifferentValues;
					string guiChars = EditorGUILayout.TextArea (m_CustomCharacters.stringValue, GUI.skin.textArea, GUILayout.MinHeight (EditorGUI.kSingleLineHeight*2));
					EditorGUI.showMixedValue = false;
					GUILayout.EndHorizontal ();
					if (EditorGUI.EndChangeCheck ())
					{
						guiChars = new string(guiChars.Distinct().ToArray());
						guiChars = guiChars.Replace ("\n", "");
						guiChars = guiChars.Replace ("\r", "");
						m_CustomCharacters.stringValue = guiChars;
					}
				}
			}
			else
			{
				EditorGUILayout.PropertyField (m_IncludeFontData, new GUIContent ("Incl. Font Data"));
				// The default font names are different based on font so it'll be a mess if we show
				// this GUI when multiple fonts are selected.
				if (targets.Length == 1)
				{
					EditorGUI.BeginChangeCheck ();
					
					GUILayout.BeginHorizontal ();
					EditorGUILayout.PrefixLabel ("Font Names");
					GUI.SetNextControlName ("fontnames");
					m_FontNamesString = EditorGUILayout.TextArea (m_FontNamesString, "TextArea", GUILayout.MinHeight (EditorGUI.kSingleLineHeight*2));
					GUILayout.EndHorizontal ();
					GUILayout.BeginHorizontal ();
					GUILayout.FlexibleSpace();
					EditorGUI.BeginDisabledGroup (m_FontNamesString == m_DefaultFontNamesString);
					if (GUILayout.Button ("Reset", "MiniButton"))
					{
						GUI.changed = true;
						if (GUI.GetNameOfFocusedControl() == "fontnames")
							GUIUtility.keyboardControl = 0;
						m_FontNamesString = m_DefaultFontNamesString;
					}
					EditorGUI.EndDisabledGroup ();
					GUILayout.EndHorizontal ();
					
					if (EditorGUI.EndChangeCheck ())
						SetFontNames (m_FontNamesString);
				}
			}
		}
		
		ApplyRevertGUI ();
	}
}
}
