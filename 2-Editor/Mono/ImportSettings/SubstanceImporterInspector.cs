// SUBSTANCE HOOK

using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor
{
	[CustomEditor (typeof (SubstanceArchive))]
	internal class SubstanceImporterInspector : Editor
	{
		private const float kPreviewWidth = 60;
		private const float kPreviewHeight = kPreviewWidth + 16;
		private const int kMaxRows = 2;
		
		private static SubstanceArchive s_LastSelectedPackage = null;
		private static string s_CachedSelectedMaterialInstanceName = null;
		private string m_SelectedMaterialInstanceName = null;
		private Vector2 m_ListScroll = Vector2.zero;
		
		private EditorCache m_EditorCache;

		[System.NonSerialized]
		private string[] m_PrototypeNames = null;
		
		Editor m_MaterialInspector;
		
		// Static preview rendering
		protected bool m_IsVisible = false;
		public Vector2 previewDir = new Vector2 (0, -20);
		public int selectedMesh = 0, lightMode = 1;
		private PreviewRenderUtility m_PreviewUtility;
		static Mesh[] s_Meshes = { null, null, null, null };
		static GUIContent[] s_MeshIcons = { null, null, null, null };
		static GUIContent[] s_LightIcons = { null, null };
		
		// Styles used in the SubstanceImporterInspector
		class SubstanceStyles
		{
			public GUIContent iconToolbarPlus = EditorGUIUtility.IconContent ("Toolbar Plus", "Add substance from prototype.");
			public GUIContent iconToolbarMinus = EditorGUIUtility.IconContent ("Toolbar Minus", "Remove selected substance.");
			public GUIContent iconDuplicate = EditorGUIUtility.IconContent ("TreeEditor.Duplicate", "Duplicate selected substance.");
			public GUIStyle resultsGridLabel = "ObjectPickerResultsGridLabel";
			public GUIStyle resultsGrid = "ObjectPickerResultsGrid";
			public GUIStyle gridBackground = "TE NodeBackground";
			public GUIStyle background = "ObjectPickerBackground";
			public GUIStyle toolbar = "TE Toolbar";
			public GUIStyle toolbarButton = "TE toolbarbutton";
			public GUIStyle toolbarDropDown = "TE toolbarDropDown";
		}
		SubstanceStyles m_SubstanceStyles;
		
		public void OnEnable ()
		{
			if (target == s_LastSelectedPackage)
				m_SelectedMaterialInstanceName = s_CachedSelectedMaterialInstanceName;
			else
				s_LastSelectedPackage = target as SubstanceArchive;
		}
		
		public void OnDisable ()
		{
			if (m_EditorCache != null)
				m_EditorCache.Dispose ();
			if (m_MaterialInspector != null)
				DestroyImmediate (m_MaterialInspector);
			s_CachedSelectedMaterialInstanceName = m_SelectedMaterialInstanceName;
			
			if (m_PreviewUtility != null)
			{
				m_PreviewUtility.Cleanup ();
				m_PreviewUtility = null;
			}
		}
		
		ProceduralMaterial GetSelectedMaterial ()
		{
			SubstanceImporter importer = GetImporter ();
			if (importer == null) 
				return null;
			ProceduralMaterial[] materials = GetSortedMaterials();
			if (m_SelectedMaterialInstanceName == null)
			{
				if (materials.Length > 0)
				{
					m_SelectedMaterialInstanceName = materials[0].name;
					return materials[0];
				}
				return null;
			}
			return System.Array.Find<ProceduralMaterial>(materials, element => element.name==m_SelectedMaterialInstanceName);
		}

		private void SelectNextMaterial ()
		{
			SubstanceImporter importer = GetImporter();
			if (importer == null)
				return;
			string selected = null;
			ProceduralMaterial[] materials = GetSortedMaterials ();
			for (int i = 0; i < materials.Length; ++i)
			{
				if (materials[i].name == m_SelectedMaterialInstanceName)
				{
					int id = System.Math.Min(i+1, materials.Length-1);
					if (id == i) --id;
					if (id >=0) selected = materials[id].name;
					break;
				}
			}
			m_SelectedMaterialInstanceName = selected;
		}
		
		Editor GetSelectedMaterialInspector ()
		{
			// Check if the cached editor is still valid
			ProceduralMaterial material = GetSelectedMaterial ();
			if (material && m_MaterialInspector != null && m_MaterialInspector.target == material)
				return m_MaterialInspector;
				
			// In case the user was editing the name of the material, but didn't apply the changes yet, end text editing
			// Case 535718
			EditorGUI.EndEditingActiveTextField();
			
			// Build a new editor and return it	
			DestroyImmediate (m_MaterialInspector);
			m_MaterialInspector = null;
			
			if (material)
				m_MaterialInspector = Editor.CreateEditor (material);
				
			return m_MaterialInspector;
		}
		
		public override void OnInspectorGUI ()
		{
			// Initialize styles
			if (m_SubstanceStyles == null)
				m_SubstanceStyles = new SubstanceStyles ();
			
			EditorGUILayout.Space ();
			
			EditorGUILayout.BeginVertical ();
				MaterialListing ();
				MaterialManagement ();
			EditorGUILayout.EndVertical ();
			
			Editor materialEditor = GetSelectedMaterialInspector ();
			if (materialEditor)
			{
				materialEditor.DrawHeader ();
				materialEditor.OnInspectorGUI ();
			}
		}
		
		SubstanceImporter GetImporter ()
		{
			return AssetImporter.GetAtPath (AssetDatabase.GetAssetPath (target)) as SubstanceImporter;
		}

		private static int previewNoDragDropHash = "PreviewWithoutDragAndDrop".GetHashCode();
		void MaterialListing()
		{
			ProceduralMaterial[] materials = GetSortedMaterials();
			
			foreach (ProceduralMaterial mat in materials)
			{
				if (mat.isProcessing)
				{
					Repaint ();
					SceneView.RepaintAll ();
					GameView.RepaintAll ();
					break;
				}
			}
			
			int count = materials.Length;
			
			// The height of the content may change based on the selected material,
			// thus an inspector vertical scrollbar may appear or disappear based on selected material.
			// We don't want previews to jump around when you select them.
			// So we don't calculate the width based on the available space (because it would change due to scrollbar)
			// but rather based on total inspector width, with scrollbar width (16) always subtracted (plus margins and borders).
			float listWidth = GUIView.current.position.width - 16 - 18 - 2;
			
			// If the preview list gets its own scrollbar, subtract the width of that one as well
			// This won't jump around based on selection, so ok to conditionally subtract it.
			if (listWidth * kMaxRows < count * kPreviewWidth)
				listWidth -= 16;
			
			// Number of previews per row
			int perRow = Mathf.Max (1, Mathf.FloorToInt (listWidth / kPreviewWidth));
			// Number of preview rows
			int rows = Mathf.CeilToInt (count / (float)perRow);
			// The rect for the preview list
			Rect listRect = new Rect (0, 0, perRow * kPreviewWidth, rows * kPreviewHeight);
			// The rect for the ScrollView the preview list in shown in
			Rect listDisplayRect = GUILayoutUtility.GetRect (
				listRect.width,
				Mathf.Clamp (listRect.height, kPreviewHeight, kPreviewHeight * kMaxRows) + 1 // Add one for top border
			);
			
			// Rect without top and side borders
			Rect listDisplayRectInner = new Rect (listDisplayRect.x+1, listDisplayRect.y+1, listDisplayRect.width-2, listDisplayRect.height-1);
			
			// Background
			GUI.Box (listDisplayRect, GUIContent.none, m_SubstanceStyles.gridBackground);
			GUI.Box (listDisplayRectInner, GUIContent.none, m_SubstanceStyles.background);
			
			m_ListScroll = GUI.BeginScrollView (listDisplayRectInner, m_ListScroll, listRect, false, false);
			
			if (m_EditorCache == null)
				m_EditorCache = new EditorCache (EditorFeatures.PreviewGUI);
			
			for (int i=0; i<materials.Length; i++)
			{
				ProceduralMaterial mat = materials[i];
				if (mat == null)
					continue;
				
				float x = (i % perRow) * kPreviewWidth;
				float y = (i / perRow) * kPreviewHeight;
				
				Rect r = new Rect (x, y, kPreviewWidth, kPreviewHeight);
				bool selected = (mat.name == m_SelectedMaterialInstanceName);
				Event evt = Event.current;
				int id = GUIUtility.GetControlID(previewNoDragDropHash, FocusType.Native, r);
				
				switch (evt.GetTypeForControl (id))
				{
				case EventType.Repaint:
					Rect r2 = r;
					r2.y = r.yMax - 16;
					r2.height = 16;
					m_SubstanceStyles.resultsGridLabel.Draw (r2, EditorGUIUtility.TempContent (mat.name), false, false, selected, selected);
					break;
				case EventType.MouseDown:
					if (evt.button != 0)
						break;
					if (r.Contains (evt.mousePosition))
					{
						// One click selects the material
						if (evt.clickCount == 1)
						{
							m_SelectedMaterialInstanceName = mat.name;
							evt.Use ();
						}
						// Double click opens the SBSAR
						else if (evt.clickCount == 2)
						{
							AssetDatabase.OpenAsset (mat);
							GUIUtility.ExitGUI ();
							evt.Use ();
						}
					}
					break;
				}
				
				r.height -= 16;
				EditorWrapper p = m_EditorCache[mat];
				p.OnPreviewGUI (r, m_SubstanceStyles.background);
			}
			
			GUI.EndScrollView ();
		}
		
		public override bool HasPreviewGUI ()
		{
			return (GetSelectedMaterialInspector () != null);
		}
		
		// Show the preview of the selected substance.
		public override void OnPreviewGUI (Rect position, GUIStyle style)
		{
			Editor editor = GetSelectedMaterialInspector ();
			if (editor)
				editor.OnPreviewGUI (position, style);
		}
		
		public override string GetInfoString ()
		{
			Editor editor = GetSelectedMaterialInspector ();
			if (editor)
				return editor.targetTitle + "\n" + editor.GetInfoString ();
			return string.Empty;
		}
		
		public override void OnPreviewSettings ()
		{
			Editor editor = GetSelectedMaterialInspector ();
			if (editor)
				editor.OnPreviewSettings ();
		}
		
		public void InstanciatePrototype (object prototypeName)
		{
			m_SelectedMaterialInstanceName = GetImporter().InstantiateMaterial(prototypeName as string);			
			ApplyAndRefresh (false);
		}

		public class SubstanceNameComparer : IComparer
		{
			public int Compare(object o1, object o2)
			{
				Object O1 = o1 as Object;
				Object O2 = o2 as Object;
				string s1 = O1.name;
				string s2 = O2.name;

				if (s1.Equals(s2))
					return 0;

				int lastUnderscore1 = s1.LastIndexOf('_');
				int lastUnderscore2 = s2.LastIndexOf('_');

				// If one of the two names DOESN'T have an underscore or if the last underscores are 
				// not at the same position or if one of the underscores is at the end of the string, then we sort as usual
				if ((lastUnderscore1 == -1) ||
					(lastUnderscore2 == -1) ||
					(lastUnderscore1 != lastUnderscore2) ||
					(lastUnderscore1 == s1.Length - 1) ||
					(lastUnderscore2 == s2.Length - 1))
				{
					return s1.CompareTo(s2);
				}

				// Both names have an underscore at the same position
				// We need to check if the parts before the underscore are the same
				if (!(s1.Substring(0, lastUnderscore1).Equals(s2.Substring(0, lastUnderscore2))))
				{
					// The first parts of each string are not the same, compare as usual
					return s1.CompareTo(s2);
				}

				// The parts before the underscores are the same
				// Next check: the parts after the underscore must be pure numbers
				string end1 = s1.Substring(lastUnderscore1 + 1);
				string end2 = s2.Substring(lastUnderscore2 + 1);
				int suffix1, suffix2;
				if (System.Int32.TryParse(end1, out suffix1) && System.Int32.TryParse(end2, out suffix2))
				{
					return suffix1.CompareTo(suffix2);
				}
				else
				{
					return s1.CompareTo(s2);
				}
			}
		}

		private ProceduralMaterial[] GetSortedMaterials()
		{
			SubstanceImporter importer = GetImporter();
			ProceduralMaterial[] materials = importer.GetMaterials();
			System.Array.Sort(materials, new SubstanceNameComparer());
			return materials;
		}

		void MaterialManagement ()
		{
			// Get selected material
			SubstanceImporter importer = GetImporter ();
			
			if (m_PrototypeNames == null)
				m_PrototypeNames = importer.GetPrototypeNames ();
			
			ProceduralMaterial selectedMaterial = GetSelectedMaterial ();
			
			GUILayout.BeginHorizontal (m_SubstanceStyles.toolbar);
			{
				GUILayout.FlexibleSpace ();
				
				EditorGUI.BeginDisabledGroup (EditorApplication.isPlaying);
				
				// Instantiate prototype
				if (m_PrototypeNames.Length>1)
				{
					Rect dropdownRect = EditorGUILayoutUtilityInternal.GetRect (m_SubstanceStyles.iconToolbarPlus, m_SubstanceStyles.toolbarDropDown);
					if (EditorGUI.ButtonMouseDown (dropdownRect, m_SubstanceStyles.iconToolbarPlus, FocusType.Passive, m_SubstanceStyles.toolbarDropDown))
					{
						GenericMenu menu = new GenericMenu ();
						for (int i = 0; i < m_PrototypeNames.Length; i++)
						{
							menu.AddItem (new GUIContent (m_PrototypeNames[i]), false, InstanciatePrototype, m_PrototypeNames[i] as object);
						}
						menu.DropDown (dropdownRect);
					}
				}
				else if (m_PrototypeNames.Length==1)
				{
					if (GUILayout.Button (m_SubstanceStyles.iconToolbarPlus, m_SubstanceStyles.toolbarButton))
					{
						m_SelectedMaterialInstanceName = GetImporter().InstantiateMaterial(m_PrototypeNames[0]);
						ApplyAndRefresh (true);
					}
				}
				
				EditorGUI.BeginDisabledGroup (selectedMaterial == null);
				
				// Delete selected instance
				if (GUILayout.Button (m_SubstanceStyles.iconToolbarMinus, m_SubstanceStyles.toolbarButton))
				{
					if (GetSortedMaterials().Length > 1)
					{
						SelectNextMaterial();
						importer.DestroyMaterial(selectedMaterial);
						ApplyAndRefresh(true);
					}
				}
				
				// Clone selected instance
				if (GUILayout.Button (m_SubstanceStyles.iconDuplicate, m_SubstanceStyles.toolbarButton))
				{
					string cloneName = importer.CloneMaterial (selectedMaterial);
					if (cloneName!="")
					{
						m_SelectedMaterialInstanceName = cloneName;
						ApplyAndRefresh (true);
					}
				}
				
				EditorGUI.EndDisabledGroup ();
				
				EditorGUI.EndDisabledGroup ();
				
			} EditorGUILayout.EndHorizontal ();
		}
		
		void ApplyAndRefresh (bool exitGUI)
		{
			string path = AssetDatabase.GetAssetPath (target);
			AssetDatabase.ImportAsset (path, ImportAssetOptions.ForceUncompressedImport);
			if (exitGUI)
				EditorGUIUtility.ExitGUI ();
			Repaint ();
		}
		
		// Init used for static preview rendering
		void Init ()
		{
			if (m_PreviewUtility == null)
				m_PreviewUtility = new PreviewRenderUtility ();
			
			if (s_Meshes[0] == null)
			{
				GameObject handleGo = (GameObject)EditorGUIUtility.LoadRequired ("Previews/PreviewMaterials.fbx");
				// @TODO: temp workaround to make it not render in the scene
				handleGo.SetActive(false);
				foreach (Transform t in handleGo.transform)
				{
					switch (t.name)
					{
						case "sphere":
							s_Meshes[0] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
							break;
						case "cube":
							s_Meshes[1] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
							break;
						case "cylinder":
							s_Meshes[2] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
							break;
						case "torus":
							s_Meshes[3] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
							break;
						default:
							Debug.Log ("Something is wrong, weird object found: " + t.name);
							break;
					}
				}
				
				s_MeshIcons[0] = EditorGUIUtility.IconContent ("PreMatSphere");
				s_MeshIcons[1] = EditorGUIUtility.IconContent ("PreMatCube");
				s_MeshIcons[2] = EditorGUIUtility.IconContent ("PreMatCylinder");
				s_MeshIcons[3] = EditorGUIUtility.IconContent ("PreMatTorus");
				
				s_LightIcons[0] = EditorGUIUtility.IconContent ("PreMatLight0");
				s_LightIcons[1] = EditorGUIUtility.IconContent ("PreMatLight1");
			}
			
		}
		
		// Note that the static preview for the package is completely different than the dynamic preview.
		// The static preview makes a single image of all the substances, while the dynamic preview
		// redirects to the MaterialEditor dynamic preview for the selected substance.
		public override Texture2D RenderStaticPreview (string assetPath, Object[] subAssets, int width, int height)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
				return null;
			
			Init ();
			
			m_PreviewUtility.BeginStaticPreview (new Rect (0, 0, width, height));
			
			DoRenderPreview (subAssets);
			
			return m_PreviewUtility.EndStaticPreview ();
		}
		
		// Used for static preview only. See note above.
		protected void DoRenderPreview (Object[] subAssets)
		{
			if (m_PreviewUtility.m_RenderTexture.width <= 0 || m_PreviewUtility.m_RenderTexture.height <= 0)
				return;
			
			List<ProceduralMaterial> materials = new List<ProceduralMaterial>();
			foreach (Object obj in subAssets)
				if (obj is ProceduralMaterial)
					materials.Add (obj as ProceduralMaterial);

			int rows = 1;
			int cols = 1;
			while (cols * cols < materials.Count)
				cols++;
			rows = Mathf.CeilToInt (materials.Count / (float)cols);
			
			m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * 5 * cols;
			m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
			m_PreviewUtility.m_Camera.farClipPlane = 5 * cols + 5.0f;
			m_PreviewUtility.m_Camera.nearClipPlane = 5 * cols - 3.0f;
			Color amb;
			if (lightMode == 0)
			{
				m_PreviewUtility.m_Light[0].intensity = .5f;
				m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (30f, 30f, 0);
				m_PreviewUtility.m_Light[1].intensity = 0;
				amb = new Color (.2f, .2f, .2f, 0);
			}
			else
			{
				m_PreviewUtility.m_Light[0].intensity = .5f;
				m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (50f, 50f, 0);
				m_PreviewUtility.m_Light[1].intensity = .5f;
				amb = new Color (.2f, .2f, .2f, 0);
			}
			
			InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);
			
			for (int i = 0; i < materials.Count; i++)
			{
				ProceduralMaterial mat = materials[i];
				Vector3 pos = new Vector3 (i % cols - (cols - 1) * 0.5f, -i / cols + (rows - 1) * 0.5f, 0);
				pos *= Mathf.Tan (m_PreviewUtility.m_Camera.fieldOfView * 0.5f * Mathf.Deg2Rad) * 5 * 2;
				m_PreviewUtility.DrawMesh (s_Meshes[selectedMesh], pos, Quaternion.Euler (previewDir.y, 0, 0) * Quaternion.Euler (0, previewDir.x, 0), mat, 0);
			}
			
			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty (false);
			m_PreviewUtility.m_Camera.Render ();
			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			InternalEditorUtility.RemoveCustomLighting ();
		}
	}
}
