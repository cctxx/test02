using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	[CustomEditor(typeof(RenderTexture))]
	[CanEditMultipleObjects]
	internal class RenderTextureInspector : TextureInspector
	{
		private static GUIContent[] kRenderTextureSizes =
		{
			new GUIContent("16"),
			new GUIContent("32"),
			new GUIContent("64"),
			new GUIContent("128"),
			new GUIContent("256"),
			new GUIContent("512"),
			new GUIContent("1024"),
			new GUIContent("2048")
		};
		private static int[] kRenderTextureSizesValues = { 16, 32, 64, 128 , 256 , 512 , 1024 , 2048 } ;
		private static GUIContent[] kRenderTextureAntiAliasing =
		{
			new GUIContent("None"),
			new GUIContent("2 samples"),
			new GUIContent("4 samples"),
			new GUIContent("8 samples")
		};
		private static int[] kRenderTextureAntiAliasingValues = { 1, 2, 4, 8 };
		private static GUIContent[] kRenderTextureDepths =
		{
			new GUIContent("None"),
			new GUIContent("16 bit"),
			new GUIContent("24 bit")
		};
		private static int[] kRenderTextureDepthsValues = { 0, 1, 2 };
		
		SerializedProperty m_Width;
		SerializedProperty m_Height;
		SerializedProperty m_DepthFormat;
		SerializedProperty m_AntiAliasing;
		//SerializedProperty m_ColorFormat;
		//SerializedProperty m_IsCubemap;
		//SerializedProperty m_MipMap;
		
		protected override void OnEnable ()
		{
			base.OnEnable();
			m_Width = serializedObject.FindProperty ("m_Width");
			m_Height = serializedObject.FindProperty ("m_Height");
			m_AntiAliasing = serializedObject.FindProperty("m_AntiAliasing");
			m_DepthFormat = serializedObject.FindProperty("m_DepthFormat");
		}
		
		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();
			
			RenderTexture rt = target as RenderTexture;
			// Render texture size popups. Only for POT render textures.
			if (rt.isPowerOfTwo)
			{
				// size
				GUI.changed = false;
				GUILayout.BeginHorizontal();
				EditorGUILayout.PrefixLabel("Size", "MiniPopup");
				EditorGUILayout.IntPopup (m_Width, kRenderTextureSizes, kRenderTextureSizesValues, GUIContent.none, GUILayout.MinWidth(40));
				GUILayout.Label("x");
				EditorGUILayout.IntPopup (m_Height, kRenderTextureSizes, kRenderTextureSizesValues, GUIContent.none, GUILayout.MinWidth(40));
				GUILayout.FlexibleSpace();
				GUILayout.EndHorizontal();

				EditorGUILayout.IntPopup(m_AntiAliasing, kRenderTextureAntiAliasing, kRenderTextureAntiAliasingValues, EditorGUIUtility.TempContent("Anti-Aliasing"));
				EditorGUILayout.IntPopup(m_DepthFormat, kRenderTextureDepths, kRenderTextureDepthsValues, EditorGUIUtility.TempContent("Depth Buffer"));

				if (GUI.changed)
					rt.Release();
			}

			// Alway repaint render texture inspectors
			// because they don't dirty automatically
			isInspectorDirty = true;
			
			serializedObject.ApplyModifiedProperties ();
			
			EditorGUILayout.Space();
			
			base.OnInspectorGUI ();
		}
		
		private static string[] kTextureFormatsStrings =
		{
			"RGBA 32bit",
			"Depth",
			"RGBA 64bit FP",
			"Shadowmap",
			"RGB 16bit",
			"RGBA 16bit",
			"RGBA 16bit (5-1)",
			"",
			"RGBA 32bit (10-2)",
			"",
			"RGBA 64bit",
			"RGBA 128bit FP",
			"RG 64bit FP",
			"RG 32bit FP",
			"R 32bit FP",
			"R 16bit FP",
			"R 8bit"
        };
		
        override public string GetInfoString ()
        {
            RenderTexture t = target as RenderTexture;
			
		    string info = t.width.ToString () + "x" + t.height.ToString ();
            if (!t.isPowerOfTwo)
                info += "(NPOT)";
            info += "  " + kTextureFormatsStrings[(int)t.format];
            info += "  " + EditorUtility.FormatBytes (TextureUtil.GetRuntimeMemorySize (t));

			return info;
        }
		
	}
	
}