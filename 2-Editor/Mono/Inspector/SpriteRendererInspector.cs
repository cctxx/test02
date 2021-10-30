#if ENABLE_SPRITES

using UnityEditorInternal;
using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{
	[CustomEditor(typeof(SpriteRenderer))]
	[CanEditMultipleObjects]
	internal class SpriteRendererInspector : Editor
	{
		private PreviewRenderUtility m_PreviewUtility;

		private SerializedProperty m_Sprite;
		private SerializedProperty m_Color;
		private SerializedProperty m_Material;
		private SerializedProperty m_SortingOrder;
		private SerializedProperty m_SortingLayerID;

		private static Texture2D s_WarningIcon;
		private GUIContent m_MaterialStyle = EditorGUIUtility.TextContent("SpriteRenderer.Material");
		private GUIContent m_SortingLayerStyle = EditorGUIUtility.TextContent("SpriteRenderer.SortingLayer");
		private GUIContent m_SortingOrderStyle = EditorGUIUtility.TextContent("SpriteRenderer.SortingOrder");

		void InitPreview()
		{
			if (m_PreviewUtility == null)
			{
				m_PreviewUtility = new PreviewRenderUtility();
				m_PreviewUtility.m_CameraFieldOfView = 30.0f;
			}
		}

		public void OnEnable()
		{
			m_Sprite = serializedObject.FindProperty("m_Sprite");
			m_Color = serializedObject.FindProperty("m_Color");
			m_Material = serializedObject.FindProperty("m_Materials.Array.data[0]"); // Only allow to edit one material
			m_SortingOrder = serializedObject.FindProperty("m_SortingOrder");
			m_SortingLayerID = serializedObject.FindProperty("m_SortingLayerID");
		}

		public override void OnInspectorGUI()
		{
			SpriteRenderer spriteRenderer = target as SpriteRenderer;

			EditorUtility.SetSelectedWireframeHidden(spriteRenderer, true);
			serializedObject.Update();
			EditorGUILayout.PropertyField(m_Sprite);

			// We don't support changing the pivot from inspector with multiselection
			if (Selection.objects.Length > 1)
				GUI.enabled = false;
			GUI.enabled = true;

			EditorGUILayout.PropertyField(m_Color, true);
			EditorGUILayout.PropertyField(m_Material, m_MaterialStyle, true);


			EditorGUILayout.Space();
			LayoutSortingLayerField (m_SortingLayerStyle, m_SortingLayerID, EditorStyles.popup);
			EditorGUILayout.PropertyField(m_SortingOrder, m_SortingOrderStyle);

			CheckForErrors();

			serializedObject.ApplyModifiedProperties();
		}

		private void CheckForErrors()
		{
			bool vertex, fragment;
			IsMaterialUsingFixedFunction(out vertex, out fragment);
			if (vertex || fragment)
				ShowError("Material uses fixed function  shader. It is not compatible with SpriteRenderer.");

			if (IsMaterialTextureAtlasConflict())
				ShowError("Material has CanUseSpriteAtlas=False tag. Sprite texture has atlasHint set. Rendering artifacts possible.");

			bool isTextureTiled;
			if (!DoesMaterialHaveSpriteTexture(out isTextureTiled))
				ShowError("Material does not have a _MainTex texture property. It is required for SpriteRenderer.");
			else
			{
				if (isTextureTiled)
					ShowError("Material texture property _MainTex has offset/scale set. It is incompatible with SpriteRenderer.");
			}
		}

		public override bool HasPreviewGUI()
		{
			return true;
		}

		public override void OnPreviewSettings()
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
				return;
			GUI.enabled = true;
			InitPreview();
		}

		public override void OnPreviewGUI(Rect r, GUIStyle background)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				if (Event.current.type == EventType.Repaint)
					EditorGUI.DropShadowLabel(new Rect(r.x, r.y, r.width, 40), "Sprite preview requires\nrender texture support");
				return;
			}

			InitPreview();

			if (Event.current.type != EventType.Repaint)
				return;

			m_PreviewUtility.BeginPreview(r, background);

			DoRenderPreview();

			Texture renderedTexture = m_PreviewUtility.EndPreview();
			EditorGUI.DrawTextureTransparent(r, renderedTexture, ScaleMode.ScaleToFit);
		}

		private void DoRenderPreview()
		{
			SpriteRenderer sprite = target as SpriteRenderer;
			Sprite frame = sprite.sprite;
			if (frame == null)
				return;

			Bounds bounds = frame.bounds;
			float halfSize = bounds.extents.magnitude;
			float distance = halfSize * 4.0f;

			m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * distance + frame.bounds.center;
			m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
			m_PreviewUtility.m_Camera.nearClipPlane = distance - halfSize * 1.1f;
			m_PreviewUtility.m_Camera.farClipPlane = distance + halfSize * 1.1f;

			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty(false);

			m_PreviewUtility.m_Camera.clearFlags = CameraClearFlags.SolidColor;
			m_PreviewUtility.m_Camera.backgroundColor = new Color(0, 0, 0, 0);
			m_PreviewUtility.DrawSprite(frame, Matrix4x4.identity, sprite.sharedMaterial, sprite.color);
			m_PreviewUtility.m_Camera.Render();

			Unsupported.SetRenderSettingsUseFogNoDirty(oldFog);
		}

		private void IsMaterialUsingFixedFunction(out bool vertex, out bool fragment)
		{
			vertex = false;
			fragment = false;

			Material material = (target as SpriteRenderer).sharedMaterial;
			if (material == null)
				return;

			vertex = ShaderUtil.GetVertexModel(material.shader) == ShaderUtil.ShaderModel.None;
			fragment = ShaderUtil.GetFragmentModel(material.shader) == ShaderUtil.ShaderModel.None;
		}

		private bool IsMaterialTextureAtlasConflict()
		{
			Material material = (target as SpriteRenderer).sharedMaterial;
			if (material == null)
				return false;
			string tag = material.GetTag("CanUseSpriteAtlas", false);
			if (tag.ToLower() == "false")
			{
				Sprite frame = m_Sprite.objectReferenceValue as Sprite;
				TextureImporter ti = AssetImporter.GetAtPath(AssetDatabase.GetAssetPath(frame)) as TextureImporter;
				if (ti.spritePackingTag != null && ti.spritePackingTag.Length > 0)
				{
					return true;
				}
			}

			return false;
		}

		private bool DoesMaterialHaveSpriteTexture(out bool tiled)
		{
			tiled = false;

			Material material = (target as SpriteRenderer).sharedMaterial;
			if (material == null)
				return true;

			
			bool has = material.HasProperty("_MainTex");
			if (has)
			{
				Vector2 offset = material.GetTextureOffset("_MainTex");
				Vector2 scale = material.GetTextureScale("_MainTex");
				if (offset.x != 0 || offset.y != 0 || scale.x != 1 || scale.y != 1)
					tiled = true;
			}

			return material.HasProperty("_MainTex");
		}

		private static void ShowError(string error)
		{
			if (s_WarningIcon == null)
				s_WarningIcon = EditorGUIUtility.LoadIcon("console.warnicon");

			var c = new GUIContent(error) {image = s_WarningIcon};

			GUILayout.Space(5);
			GUILayout.BeginVertical(EditorStyles.helpBox);
			GUILayout.Label(c, EditorStyles.wordWrappedMiniLabel);
			GUILayout.EndVertical();
		}


		//@TODO: move all this to EditorGUI

		static int s_SortingLayerFieldHash = "s_SortingLayerFieldHash".GetHashCode ();

		static public void LayoutSortingLayerField (GUIContent label, SerializedProperty layerID, GUIStyle style)
		{
			Rect r = EditorGUILayout.s_LastRect = EditorGUILayout.GetControlRect (false, EditorGUI.kSingleLineHeight, style);
			SortingLayerField (r, label, layerID, style);
		}

		static public void SortingLayerField (Rect position, GUIContent label, SerializedProperty layerID, GUIStyle style)
		{
			int id = EditorGUIUtility.GetControlID (s_SortingLayerFieldHash, EditorGUIUtility.native, position);
			position = EditorGUI.PrefixLabel (position, id, label);

			Event evt = Event.current;
			int selected = EditorGUI.PopupCallbackInfo.GetSelectedValueForControl(id, -1);
			if (selected != -1)
			{
				int[] layerIDs = InternalEditorUtility.sortingLayerUniqueIDs;
				if (selected >= layerIDs.Length)
				{
					((TagManager)EditorApplication.tagManager).m_DefaultExpandedFoldout = "SortingLayers";
					Selection.activeObject = EditorApplication.tagManager;
				}
				else
				{
					layerID.intValue = layerIDs[selected];
				}
			}
		
			if (evt.type == EventType.MouseDown && position.Contains (evt.mousePosition) || evt.MainActionKeyForControl (id))
			{
				int i = 0;
				int[] layerIDs = InternalEditorUtility.sortingLayerUniqueIDs;
				string[] layerNames = InternalEditorUtility.sortingLayerNames;
				for (i = 0; i < layerIDs.Length; i++)
				{
					if (layerIDs[i] == layerID.intValue)
						break;
				}
				ArrayUtility.Add (ref layerNames, "");
				ArrayUtility.Add (ref layerNames, "Add Sorting Layer...");

				EditorGUI.DoPopup (position, id, i, EditorGUIUtility.TempContent (layerNames), style);
			}
			else if (Event.current.type == EventType.Repaint)
			{
				GUIContent layerName;
				if (layerID.hasMultipleDifferentValues)
					layerName = EditorGUI.mixedValueContent;
				else
					layerName = EditorGUIUtility.TempContent (InternalEditorUtility.GetSortingLayerNameFromUniqueID (layerID.intValue));
				EditorGUI.showMixedValue = layerID.hasMultipleDifferentValues;
				EditorGUI.BeginHandleMixedValueContentColor ();
				style.Draw (position, layerName, id, false);
				EditorGUI.EndHandleMixedValueContentColor ();
				EditorGUI.showMixedValue = false;
			}
		}

	}
}

#endif
