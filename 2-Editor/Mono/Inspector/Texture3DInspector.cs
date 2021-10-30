using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	[CustomEditor (typeof (Texture3D))]
	[CanEditMultipleObjects]
	internal class Texture3DInspector : TextureInspector
	{
		private PreviewRenderUtility m_PreviewUtility;
		private Material m_Material;
		private Mesh m_Mesh;

		public override void OnInspectorGUI ()
		{
			base.OnInspectorGUI ();
		}

		public override string GetInfoString ()
		{
			Texture3D tex = target as Texture3D;

			string info = string.Format ("{0}x{1}x{2} {3} {4}",
				tex.width, tex.height, tex.depth,
				TextureUtil.GetTextureFormatString (tex.format),
				EditorUtility.FormatBytes (TextureUtil.GetRuntimeMemorySize (tex)));

			return info;
		}

		public void OnDisable ()
		{
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


		public override void OnPreviewSettings ()
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture || !SystemInfo.supports3DTextures)
				return;
			GUI.enabled = true;
		}

		public Vector2 previewDir = new Vector2 (0, 0);
		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture || !SystemInfo.supports3DTextures)
			{
				if (Event.current.type == EventType.Repaint)
					EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "3D texture preview not supported");
				return;
			}


			previewDir = PreviewGUI.Drag2D (previewDir, r);

			if (Event.current.type != EventType.Repaint)
				return;

			InitPreview ();
			m_Material.mainTexture = target as Texture;

			m_PreviewUtility.BeginPreview (r, background);
			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty (false);

			m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * 3.0f;
			m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
			Quaternion rot = Quaternion.Euler (previewDir.y, 0, 0) * Quaternion.Euler (0, previewDir.x, 0);
			m_PreviewUtility.DrawMesh (m_Mesh, Vector3.zero, rot, m_Material, 0);
			m_PreviewUtility.m_Camera.Render ();

			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			Texture renderedTexture = m_PreviewUtility.EndPreview ();
			GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
		}

		void InitPreview ()
		{
			if (m_PreviewUtility == null)
			{
				m_PreviewUtility = new PreviewRenderUtility ();
				m_PreviewUtility.m_CameraFieldOfView = 30.0f;
				m_Material = new Material (
						@"Shader ""Hidden/3DTextureInspector"" {
                        Properties {
	                        _MainTex ("""", 3D) = """" { TexGen ObjectLinear }
                        }
                        SubShader {
                            Tags { ""ForceSupported"" = ""True"" } 
	                        Pass { SetTexture[_MainTex] { combine texture } }
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
				handleGo.SetActive (false);
				foreach (Transform t in handleGo.transform)
				{
					if (t.name == "sphere")
						m_Mesh = t.GetComponent<MeshFilter>().sharedMesh;
				}
			}
		}		
	}
	
}
