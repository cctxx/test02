using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	[CustomEditor(typeof(Cubemap))]
	internal class CubemapTextureInspector : Editor
	{	
		static private string[] kSizes = { "16", "32", "64", "128" , "256" , "512" , "1024" , "2048" } ;
		static private int[] kSizesValues = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
		const int kTextureSize = 64;
		
        private PreviewRenderUtility m_PreviewUtility;

        private Texture2D[] images;
        private Material m_Material;
        private Mesh m_Mesh;

		public void OnEnable()
		{	
			InitTexturesFromCubemap();
		}
		
		public void OnDisable()
		{
            if (images != null)
			    for (int i = 0; i < 6; ++i)
					if (!EditorUtility.IsPersistent (images[i]))
						DestroyImmediate(images[i]);
            images = null;

			if (m_PreviewUtility != null)
			{
				m_PreviewUtility.Cleanup ();
				m_PreviewUtility = null;
			}
			
			if (m_Material)
			{
				DestroyImmediate (m_Material.shader, true);
				DestroyImmediate (m_Material, true);
                m_Material = null;
			}
		}
		
		private void InitTexturesFromCubemap()
		{
			Cubemap c = target as Cubemap;
			if (c != null)
			{
				if( images == null )
					images = new Texture2D[6];
				for (int i = 0; i < 6; ++i)
				{
					if (images[i])
						if (!EditorUtility.IsPersistent (images[i]))
							DestroyImmediate (images[i]);
					if (TextureUtil.GetSourceTexture(c,(CubemapFace)i))
					{
						images[i] = TextureUtil.GetSourceTexture(c,(CubemapFace)i);
					}
					else
					{
						images[i] = new Texture2D (kTextureSize, kTextureSize, TextureFormat.ARGB32, false);
    	                images[i].hideFlags = HideFlags.HideAndDontSave;
						TextureUtil.CopyCubemapFaceIntoTexture (c, (CubemapFace)i, images[i]);
					}
				}
			}
		}

		public override void OnInspectorGUI()
		{
			if (images == null)
				InitTexturesFromCubemap();
			
			EditorGUIUtility.labelWidth = 50;
			Cubemap c = target as Cubemap;
			
			GUILayout.BeginVertical();
			
			GUILayout.BeginHorizontal();			
				ShowFace( "Right\n(+X)", CubemapFace.PositiveX );
				ShowFace( "Left\n(-X)", CubemapFace.NegativeX );				
			GUILayout.EndHorizontal();
			
			GUILayout.BeginHorizontal();
				ShowFace( "Top\n(+Y)", CubemapFace.PositiveY );
				ShowFace( "Bottom\n(-Y)", CubemapFace.NegativeY );
			GUILayout.EndHorizontal();
			
			GUILayout.BeginHorizontal();
				ShowFace( "Front\n(+Z)", CubemapFace.PositiveZ );
				ShowFace( "Back\n(-Z)", CubemapFace.NegativeZ );
			GUILayout.EndHorizontal();			
			
			GUILayout.EndVertical();
			
			EditorGUIUtility.labelWidth = 0;
			
			EditorGUILayout.Space ();
			
			EditorGUI.BeginChangeCheck ();
			
			int faceSize = TextureUtil.GetGLWidth(c);
			faceSize = EditorGUILayout.IntPopup("Face size", faceSize, kSizes, kSizesValues);
			
			int mipMaps = TextureUtil.CountMipmaps(c);
			bool useMipMap = EditorGUILayout.Toggle("MipMaps", mipMaps > 1);
			
			bool linear = TextureUtil.GetLinearSampled(c);
			linear = EditorGUILayout.Toggle("Linear", linear);
			
			bool readable = TextureUtil.IsCubemapReadable(c);
			readable = EditorGUILayout.Toggle("Readable", readable);
			
			if (EditorGUI.EndChangeCheck ())
			{
				// reformat the cubemap
				if (TextureUtil.ReformatCubemap(ref c, faceSize, faceSize, c.format, useMipMap, linear))
					InitTexturesFromCubemap();
				
				TextureUtil.MarkCubemapReadable(c,readable);
				c.Apply();				
			}						
		}
		
		// A minimal list of settings to be shown in the Asset Store preview inspector
		internal override void OnAssetStoreInspectorGUI ()
		{
			OnInspectorGUI();
		}
		
		private void ShowFace( string label, CubemapFace face )
		{
			Cubemap c = target as Cubemap;
			int iface = (int)face;
			GUI.changed = false;
			
			Texture2D tex = (Texture2D)ObjectField(label, images[iface], typeof(Texture2D), false);
			if (GUI.changed) {
				TextureUtil.CopyTextureIntoCubemapFace (tex, c, face);
				// enable this line in order to retain connections from cube faces to their corresponding
				// texture2D assets, this allows auto-update functionality when editing the source texture
				// images 
				//TextureUtil.SetSourceTexture(c, face, tex);
				images[iface] = tex;

			}
		}
		
		// Variation of ObjectField where label is not restricted to one line
		public static Object ObjectField (string label, Object obj, System.Type objType, bool allowSceneObjects, params GUILayoutOption[] options)
		{
			GUILayout.BeginHorizontal ();
			Rect r = GUILayoutUtility.GetRect (EditorGUIUtility.labelWidth, EditorGUI.kSingleLineHeight*2, EditorStyles.label, GUILayout.ExpandWidth (false));
			GUI.Label (r, label, EditorStyles.label);
			r = GUILayoutUtility.GetAspectRect (1, EditorStyles.objectField, GUILayout.Width (64));
			Object retval = EditorGUI.ObjectField (r, obj, objType, allowSceneObjects);
			GUILayout.EndHorizontal ();
			return retval;
		}

		void InitPreview ()
        {
			if (m_PreviewUtility == null) {
				m_PreviewUtility = new PreviewRenderUtility ();
				m_PreviewUtility.m_CameraFieldOfView = 30.0f;
				m_Material = new Material (
                        @"Shader ""Hidden/CubemapInspector"" {
                        Properties {
	                        _MainTex ("""", Cube) = """" { TexGen CubeReflect }
                        }
                        SubShader {
                            Tags { ""ForceSupported"" = ""True"" } 
	                        Pass { SetTexture[_MainTex] { matrix [_CubemapRotation] combine texture } }
                        }
                        Fallback Off
                        }");
				m_Material.hideFlags = HideFlags.HideAndDontSave;
				m_Material.shader.hideFlags = HideFlags.HideAndDontSave;
				m_Material.mainTexture = target as Texture;
			}

			if (m_Mesh == null)
			{
				GameObject handleGo = (GameObject)EditorGUIUtility.LoadRequired ("Previews/PreviewMaterials.fbx");
				// @TODO: temp workaround to make it not render in the scene
				handleGo.SetActive(false);
				foreach (Transform t in handleGo.transform) {
					if (t.name == "sphere")
						m_Mesh = ((MeshFilter)t.GetComponent (typeof (MeshFilter))).sharedMesh;
				}
			}
		}

		public override bool HasPreviewGUI ()
		{
			return (target != null);
		}
		
		public override void OnPreviewSettings ()
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
				return;
			GUI.enabled = true;
			InitPreview ();
		}

		public Vector2 previewDir = new Vector2 (0, 0);
		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				if (Event.current.type == EventType.Repaint)
					EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "Cubemap preview requires\nrender texture support");
				return;
			}

			InitPreview ();

			previewDir = PreviewGUI.Drag2D (previewDir, r);

			if (Event.current.type != EventType.Repaint)
				return;

			m_PreviewUtility.BeginPreview (r, background);
			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty (false);

			m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * 3.0f;
			m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
			Quaternion rot = Quaternion.Euler (previewDir.y, 0, 0) * Quaternion.Euler (0, previewDir.x, 0);
			m_Material.SetMatrix ("_CubemapRotation", Matrix4x4.TRS(Vector3.zero, rot, Vector3.one));
			m_PreviewUtility.DrawMesh (m_Mesh, Vector3.zero, rot, m_Material, 0);
			m_PreviewUtility.m_Camera.Render ();

			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			Texture renderedTexture = m_PreviewUtility.EndPreview ();
			GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
		}
	}
	
}
