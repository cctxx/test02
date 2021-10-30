using UnityEngine;
using UnityEditor;
using UnityEditorInternal;


namespace UnityEditor
{
	[CustomEditor(typeof(Mesh))]
	[CanEditMultipleObjects]
	internal class ModelInspector : Editor
	{
		private PreviewRenderUtility m_PreviewUtility;
		private Material m_Material;
		private Material m_WireMaterial;
		public Vector2 previewDir = new Vector2 (120, -20);
		
		void Init ()
		{
			if (m_PreviewUtility == null)
			{
				m_PreviewUtility = new PreviewRenderUtility ();
				m_PreviewUtility.m_CameraFieldOfView = 30.0f;
				m_Material = EditorGUIUtility.GetBuiltinExtraResource (typeof (Material), "Default-Diffuse.mat") as Material;
				m_WireMaterial = new Material (
@"Shader ""Hidden/ModelInspectorWireframe"" {
SubShader {
	Tags { ""ForceSupported"" = ""True"" } 
	Color (0,0,0,0.3) Blend SrcAlpha OneMinusSrcAlpha
	ZTest LEqual ZWrite Off
	Offset -1, -1
	Pass { Cull Off }
}}");
				m_WireMaterial.hideFlags = HideFlags.HideAndDontSave;
				m_WireMaterial.shader.hideFlags = HideFlags.HideAndDontSave;
			}
		}

		public override void OnPreviewSettings ()
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
				return;
			GUI.enabled = true;
			Init ();
		}

		private void DoRenderPreview() 
		{
			Mesh mesh = target as Mesh;
			Bounds bounds = mesh.bounds;
			float halfSize = bounds.extents.magnitude;
			float distance = halfSize * 4.0f;

			m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * distance;
			m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
			m_PreviewUtility.m_Camera.nearClipPlane = distance - halfSize * 1.1f;
			m_PreviewUtility.m_Camera.farClipPlane = distance + halfSize * 1.1f;
			m_PreviewUtility.m_Light[0].intensity = .7f;
			m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (40f, 40f, 0);
			m_PreviewUtility.m_Light[1].intensity = .7f;
			Color amb = new Color (.1f, .1f, .1f, 0);

			InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);
			Quaternion rot = Quaternion.Euler (previewDir.y, 0, 0) * Quaternion.Euler (0, previewDir.x, 0);
			Vector3 pos = rot*(-bounds.center);
			
			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty (false);
			
			int submeshes = mesh.subMeshCount;
			m_PreviewUtility.m_Camera.clearFlags = CameraClearFlags.Nothing;
			for (int i = 0; i < submeshes; ++i)
				m_PreviewUtility.DrawMesh (mesh, pos, rot, m_Material, i);
			m_PreviewUtility.m_Camera.Render ();
			m_PreviewUtility.m_Camera.clearFlags = CameraClearFlags.Nothing;
			ShaderUtil.wireframeMode = true;
			for (int i = 0; i < submeshes; ++i)
				m_PreviewUtility.DrawMesh (mesh, pos, rot, m_WireMaterial, i);
			m_PreviewUtility.m_Camera.Render ();
			
			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			ShaderUtil.wireframeMode = false;
			InternalEditorUtility.RemoveCustomLighting ();
		}

		public override Texture2D RenderStaticPreview(string assetPath, Object[] subAssets, int width, int height)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture) {
				//Debug.Log("Could not generate static preview. Render texture not supported by hardware.");
				return null;
			}
			
			Init ();

			m_PreviewUtility.BeginStaticPreview (new Rect(0,0,width,height));
			
			DoRenderPreview();

			return m_PreviewUtility.EndStaticPreview ();
		}

		public override bool HasPreviewGUI()
		{
			return (target != null);
		}
		
		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				if (Event.current.type == EventType.Repaint)
					EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "Mesh preview requires\nrender texture support");
				return;
			}

			Init ();

			previewDir = PreviewGUI.Drag2D (previewDir, r);

			if (Event.current.type != EventType.Repaint)
				return;

			m_PreviewUtility.BeginPreview (r, background);
			
			DoRenderPreview();

			Texture renderedTexture = m_PreviewUtility.EndPreview ();
			GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
		}

		// A minimal list of settings to be shown in the Asset Store preview inspector
		internal override void OnAssetStoreInspectorGUI ()
		{
			OnInspectorGUI();
		}

		public void OnDestroy ()
		{
			if (m_PreviewUtility != null)
			{
				m_PreviewUtility.Cleanup ();
				m_PreviewUtility = null;
			}
			if (m_WireMaterial) {
				DestroyImmediate (m_WireMaterial.shader, true);
				DestroyImmediate (m_WireMaterial, true);
			}
		}

		public override string GetInfoString ()
		{
			Mesh mesh = target as Mesh;
			string info = mesh.vertexCount + " verts, " + InternalMeshUtil.GetPrimitiveCount (mesh) + " tris";
			int submeshes = mesh.subMeshCount;
			if (submeshes > 1)
				info += ", " + submeshes + " submeshes";

			int blendShapeCount = mesh.blendShapeCount;
			if (blendShapeCount > 1)
				info += ", " + blendShapeCount + " blendShapes";

			info += "\n" + InternalMeshUtil.GetVertexFormat (mesh);
			return info;
		}
	}	
}


