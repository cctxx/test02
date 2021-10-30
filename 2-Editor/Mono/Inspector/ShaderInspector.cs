using UnityEngine;
using UnityEditor;

namespace UnityEditor
{
	[CustomEditor(typeof(Shader))]
	internal class ShaderInspector : Editor
	{
		private static string[] kPropertyTypes = {
			"Color: ",
			"Vector: ",
			"Float: ",
			"Range: ",
			"Texture: ",
		};
		private static string[] kTextureTypes = {
			"No Texture?: ",
			"1D texture: ",
			"Texture: ",
			"Volume: ",
			"Cubemap: ",
			"Any texture: ",
		};
		private static string[] kShaderLevels = {
			"Fixed function",
			"SM1.x",
			"SM2.0",
			"SM3.0",
			"SM4.0",
			"SM5.0",
		};

		internal class Styles
		{
			public Texture2D errorIcon = EditorGUIUtility.LoadIcon("console.erroricon.sml");
			public Texture2D warningIcon = EditorGUIUtility.LoadIcon("console.warnicon.sml");
		}
		internal static Styles ms_Styles;
	
		private static string GetPropertyType (Shader s, int index) {
			ShaderUtil.ShaderPropertyType type = ShaderUtil.GetPropertyType (s,index);
			if (type == ShaderUtil.ShaderPropertyType.TexEnv) {
				return kTextureTypes[(int)ShaderUtil.GetTexDim(s,index)];
			} else {
				return kPropertyTypes[(int)type];
			}
		}
		
		public override void OnInspectorGUI()
		{
			if (ms_Styles == null)
				ms_Styles = new Styles();
			
			GUI.enabled = true;

			Shader s = target as Shader;
			EditorGUI.indentLevel = 0;

			int n;
			if (s == null || !s.isSupported)
			{
				GUILayout.Label ("Shader has errors or is not supported by your graphics card", EditorStyles.helpBox);
			}
			else
			{
				//EditorGUILayout.LabelField ("Name", s.name);
				/*
				bool pixelLit = ShaderUtil.IsPixelLit (s);
				bool shadows = ShaderUtil.DoesSupportShadows (s);
				EditorGUILayout.LabelField ("Per-pixel lit",
					pixelLit ?
					(shadows ? "yes, with shadows" : "yes, no shadows") :
					"no");
				*/
				EditorGUILayout.LabelField ("Cast shadows", (ShaderUtil.HasShadowCasterPass (s) && ShaderUtil.HasShadowCollectorPass (s)) ? "yes" : "no");
				EditorGUILayout.LabelField ("Render queue", ShaderUtil.GetRenderQueue (s).ToString ());
				EditorGUILayout.LabelField ("LOD", ShaderUtil.GetLOD (s).ToString ());
				EditorGUILayout.LabelField ("Geometry", ShaderUtil.GetSourceChannels (s));
				EditorGUILayout.LabelField ("Vertex shader", kShaderLevels[(int)ShaderUtil.GetVertexModel (s)]);
				EditorGUILayout.LabelField ("Fragment shader", kShaderLevels[(int)ShaderUtil.GetFragmentModel (s)]);
				//EditorGUILayout.LabelField ("Subshaders", ShaderUtil.GetSubShaderCount(s).ToString());
				EditorGUILayout.LabelField ("Ignore projector", ShaderUtil.DoesIgnoreProjector (s) ? "yes" : "no");
				GUILayout.Label ("Properties:", EditorStyles.boldLabel);
				n = ShaderUtil.GetPropertyCount (s);
				for (int i = 0; i < n; ++i)
				{
					string pname = ShaderUtil.GetPropertyName (s, i);
					string pdesc = GetPropertyType (s, i) + ShaderUtil.GetPropertyDescription (s, i);
					EditorGUILayout.LabelField (pname, pdesc);
				}
			}

			n = ShaderUtil.GetErrorCount(s);
			if (n != 0)
				GUILayout.Label("Errors:", EditorStyles.boldLabel);
			EditorGUIUtility.SetIconSize (new Vector2(16.0f, 16.0f));
			for (int i = 0; i < n; ++i)
			{
				string err = ShaderUtil.GetShaderErrorMessage (s, i);
				bool warn = ShaderUtil.GetShaderErrorWarning (s, i);
				int line = ShaderUtil.GetShaderErrorLine (s, i);
				if (line >= 0)
					err += " at line " + line;
				GUILayout.Label(new GUIContent(err, warn ? ms_Styles.warningIcon : ms_Styles.errorIcon));
			}
			EditorGUIUtility.SetIconSize (Vector2.zero);
			EditorGUILayout.BeginHorizontal();
			EditorGUILayout.Space ();
			if (GUILayout.Button ("Open compiled shader", "MiniButton"))
			{
				ShaderUtil.OpenCompiledShader (s);
				GUIUtility.ExitGUI ();
			}
			EditorGUILayout.EndHorizontal ();
		}
	}
}