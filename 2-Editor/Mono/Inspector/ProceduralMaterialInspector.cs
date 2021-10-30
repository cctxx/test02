// SUBSTANCE HOOK

using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor
{
	[CustomEditor(typeof(ProceduralMaterial))]
	[CanEditMultipleObjects]
	internal class ProceduralMaterialInspector : MaterialEditor
	{
		private static ProceduralMaterial m_Material = null;
		private static Shader m_ShaderPMaterial = null;
		private static SubstanceImporter m_Importer = null;
		private static string[] kMaxTextureSizeStrings = { "32", "64", "128", "256", "512", "1024", "2048" };
		private static int[] kMaxTextureSizeValues = { 32, 64, 128, 256, 512, 1024, 2048 };
		private bool m_ShowTexturesSection = false;
		private bool m_ShowHSLInputs = true;
		private string m_LastGroup;
		private Styles m_Styles;
		private static string[] kMaxLoadBehaviorStrings = { "Do nothing", "Build on level load", "Bake and keep Substance", "Bake and discard Substance", "Use caching" };
		private static int[] kMaxLoadBehaviorValues = { 0, 1, 2, 3, 4 };
		private bool m_MightHaveModified = false;

		private static Dictionary<ProceduralMaterial, float> m_GeneratingSince = new Dictionary<ProceduralMaterial, float> ();

		private class Styles
		{
			public GUIContent hslContent = new GUIContent ("HSL Adjustment", "Hue_Shift, Saturation, Luminosity");
			public GUIContent randomSeedContent = new GUIContent ("Random Seed", "$randomseed : the overall random aspect of the texture.");
			public GUIContent randomizeButtonContent = new GUIContent ("Randomize");
			public GUIContent generateAllOutputsContent = new GUIContent("Generate all outputs", "Force the generation of all substance's outputs.");
			public GUIContent animatedContent = new GUIContent("Animation update rate", "Set the animation update rate in millisecond");
			public GUIContent defaultPlatform = EditorGUIUtility.TextContent("TextureImporter.Platforms.Default");
			public GUIContent targetWidth = new GUIContent("Target Width");
			public GUIContent targetHeight = new GUIContent("Target Height");
			public GUIContent textureFormat = EditorGUIUtility.TextContent("TextureImporter.TextureFormat");
			public GUIContent[] textureFormatOptions = {
				EditorGUIUtility.TextContent ("Compressed"),
				EditorGUIUtility.TextContent ("RAW")
			};
			public GUIContent[] coordNames = {
				new GUIContent("x"),
				new GUIContent("y"),
				new GUIContent("z"),
				new GUIContent("w")
			};
			public GUIContent loadBehavior = new GUIContent("Load Behavior");			
		}

		private void ReimportSubstances()
		{
			string[] asset_names = new string[targets.GetLength(0)];
			int i = 0;
			foreach (ProceduralMaterial material in targets)
			{
				asset_names[i++] = AssetDatabase.GetAssetPath(material);
			}
			for (int j = 0; j < i; ++j)
			{
				SubstanceImporter importer = AssetImporter.GetAtPath(asset_names[j]) as SubstanceImporter;
				if (importer && EditorUtility.IsDirty(importer.GetInstanceID()))
				{
					AssetDatabase.ImportAsset(asset_names[j], ImportAssetOptions.ForceUncompressedImport);
				}
			}
		}

		public override void Awake ()
		{
			base.Awake ();
			m_ShowTexturesSection = EditorPrefs.GetBool("ProceduralShowTextures", false);
		}

		public override void OnEnable ()
		{
			base.OnEnable ();

			// Make sure we use all cores for substances while editing material settings
			if (!EditorApplication.isPlaying)
				ProceduralMaterial.substanceProcessorUsage = ProceduralProcessorUsage.All;

			Undo.undoRedoPerformed += UndoRedoPerformed;
		}

		public override void OnDisable ()
		{
			// TODO: replace this bool by something to know if it's open to edit the asset or only to render previews
			if (m_MightHaveModified)
			{
				// Reimport if required
				if (!EditorApplication.isPlaying && !InternalEditorUtility.ignoreInspectorChanges)
					ReimportSubstances();

				// Make sure we use all cores for substances while editing material settings
				if (!EditorApplication.isPlaying)
					ProceduralMaterial.substanceProcessorUsage = ProceduralProcessorUsage.One;
			}

			Undo.undoRedoPerformed -= UndoRedoPerformed;
			base.OnDisable ();
		}

		// A minimal list of settings to be shown in the Asset Store preview inspector
		internal override void OnAssetStoreInspectorGUI ()
		{
			if (m_Styles == null)
				m_Styles = new Styles();

			// Procedural Material
			ProceduralMaterial material = target as ProceduralMaterial;
			// Set current material & shader to test for shader changes later on
			if (m_Material != material)
			{
				m_Material = material;
				m_ShaderPMaterial = material.shader;
			}

			ProceduralProperties();
			GUILayout.Space(15);
			GeneratedTextures();
		}

		// Don't disable the gui based on hideFlags of the targets because this editor applies changes
		// back to the importer, so it's an exception. We still want to respect IsOpenForEdit() though.
		internal override bool IsEnabled () { return IsOpenForEdit (); }

		internal override void OnHeaderTitleGUI (Rect titleRect, string header)
		{
			// Procedural Material
			ProceduralMaterial material = target as ProceduralMaterial;

			// Retrieve the Substance importer
			string path = AssetDatabase.GetAssetPath(target);
			m_Importer = AssetImporter.GetAtPath(path) as SubstanceImporter;

			// In case the user somehow created a ProceduralMaterial manually outside of the importer
			if (m_Importer == null)
				return;			

			string materialName = material.name;
			materialName = EditorGUI.DelayedTextField (titleRect, materialName, null, EditorStyles.textField);
			if (materialName != material.name)
			{
				if (m_Importer.RenameMaterial(material, materialName))
				{
					AssetDatabase.ImportAsset(m_Importer.assetPath, ImportAssetOptions.ForceUncompressedImport);
					GUIUtility.ExitGUI();
				}
				else
				{
					materialName = material.name;
				}
			}
		}

		public override void OnInspectorGUI ()
		{
			EditorGUI.BeginDisabledGroup(AnimationMode.InAnimationMode());

			m_MightHaveModified = true;

			if (m_Styles == null)
				m_Styles = new Styles();

			// Procedural Material
			ProceduralMaterial material = target as ProceduralMaterial;

			// Retrieve the Substance importer
			string path = AssetDatabase.GetAssetPath(target);
			m_Importer = AssetImporter.GetAtPath(path) as SubstanceImporter;

			// In case the user somehow created a ProceduralMaterial manually outside of the importer
			if (m_Importer == null)
			{
				GUILayout.Label("Invalid Procedural Material");
				return;
			}

			// Set current material & shader to test for shader changes later on
			if (m_Material != material)
			{
				m_Material = material;
				m_ShaderPMaterial = material.shader;
			}

			// Show Material header that also works as foldout
			if (!isVisible || material.shader == null)
			{
				return;
			}

			// Update importer if shader changed
			// The importer needs to be up-to-date when handling the PropertiesGUI
			// so we can't wait with doing this until afterwards.
			if (m_ShaderPMaterial != material.shader)
			{
				m_ShaderPMaterial = material.shader;

				foreach (ProceduralMaterial mat in targets)
				{
					string matPath = AssetDatabase.GetAssetPath(mat);
					SubstanceImporter importer = AssetImporter.GetAtPath(matPath) as SubstanceImporter;
					importer.OnShaderModified(mat);
				}
			}

			// Show standard GUI without substance textures
			if (PropertiesGUI ())
			{
				m_ShaderPMaterial = material.shader;				
				foreach (ProceduralMaterial mat in targets)
				{
					string matPath = AssetDatabase.GetAssetPath(mat);
					SubstanceImporter importer = AssetImporter.GetAtPath(matPath) as SubstanceImporter;
					importer.OnShaderModified(mat);
				}
				PropertiesChanged ();
			}

			GUI.changed = false;

			// Show input header
			GUILayout.Space(5);
			ProceduralProperties();
			GUILayout.Space(15);
			GeneratedTextures();

			EditorGUI.EndDisabledGroup();
		}

		void ProceduralProperties() {
			GUILayout.Label("Procedural Properties", EditorStyles.boldLabel, GUILayout.ExpandWidth(true));

			// Ensure that materials are updated
			foreach (ProceduralMaterial mat in targets)
			{
				if (mat.isProcessing)
				{
					Repaint ();
					SceneView.RepaintAll ();
					GameView.RepaintAll ();
					break;
				}
			}

			if (targets.Length > 1)
			{
				GUILayout.Label("Procedural properties do not support multi-editing.", EditorStyles.wordWrappedLabel);
				return;
			}

			// Reset label and field widths
			EditorGUIUtility.labelWidth = 0;
			EditorGUIUtility.fieldWidth = 0;
			
			// Show inputs
			
			if (m_Importer != null) {
				// Display warning if substance is not supported
				if (!ProceduralMaterial.isSupported)
				{
					GUILayout.Label ("Procedural Materials are not supported on "+EditorUserBuildSettings.activeBuildTarget+". Textures will be baked.",
						EditorStyles.helpBox, GUILayout.ExpandWidth (true));
				}
				EditorGUI.BeginDisabledGroup(EditorApplication.isPlaying);
				{ // Generate all outputs flag
					bool value = EditorGUILayout.Toggle(m_Styles.generateAllOutputsContent, m_Importer.GetGenerateAllOutputs(m_Material));
					if (value != m_Importer.GetGenerateAllOutputs(m_Material))
						m_Importer.SetGenerateAllOutputs(m_Material, value);
				}
				EditorGUI.EndDisabledGroup();
				if (m_Material.HasProceduralProperty("$time"))
				{ // Animation update rate
					int value = EditorGUILayout.IntField(m_Styles.animatedContent, m_Importer.GetAnimationUpdateRate(m_Material));
					if (value != m_Importer.GetAnimationUpdateRate(m_Material))
						m_Importer.SetAnimationUpdateRate(m_Material, value);
				}
			}
			InputOptions(m_Material);
		}

		void GeneratedTextures()
		{
			if (targets.Length > 1)
				return;
			
			// Generated Textures foldout
			string header = "Generated Textures";
			// Ensure that textures are updated
			if (ShowIsGenerating (target as ProceduralMaterial))
				header += " (Generating...)";

			bool changed = GUI.changed;
			GUI.changed = false;
			m_ShowTexturesSection = EditorGUILayout.Foldout(m_ShowTexturesSection, header);
			if (GUI.changed)
			{
				EditorPrefs.SetBool("ProceduralShowTextures", m_ShowTexturesSection);
			}
			GUI.changed = changed;

			// Generated Textures section
			if (m_ShowTexturesSection)
			{
				// Show textures
				ShowProceduralTexturesGUI(m_Material);
				ShowGeneratedTexturesGUI(m_Material);

				if (m_Importer != null ) {
					// Show texture offset
					if (HasProceduralTextureProperties(m_Material))
						OffsetScaleGUI(m_Material);

					EditorGUI.BeginDisabledGroup (EditorApplication.isPlaying);
						ShowTextureSizeGUI();
					EditorGUI.EndDisabledGroup ();
				}
			}
		}

		public static bool ShowIsGenerating (ProceduralMaterial mat)
		{
			if (!m_GeneratingSince.ContainsKey (mat))
				m_GeneratingSince[mat] = 0;
			if (mat.isProcessing)
				return (Time.realtimeSinceStartup > m_GeneratingSince[mat] + 0.4f);
			else
				m_GeneratingSince[mat] = Time.realtimeSinceStartup;
			return false;
		}

		public override string GetInfoString ()
		{
			ProceduralMaterial material = target as ProceduralMaterial;
			if (material.mainTexture == null)
				return string.Empty;
			return material.mainTexture.width+"x"+material.mainTexture.height;
		}

		public bool HasProceduralTextureProperties (Material material)
		{
			Shader shader = material.shader;
			int count = ShaderUtil.GetPropertyCount(shader);
			for (int i = 0; i < count; i++)
			{
				if (ShaderUtil.GetPropertyType(shader, i) != ShaderUtil.ShaderPropertyType.TexEnv)
					continue;

				string name = ShaderUtil.GetPropertyName(shader, i);
				Texture tex = material.GetTexture(name);

				if (SubstanceImporter.IsProceduralTextureSlot(material, tex, name))
					return true;
			}
			return false;
		}

		protected void OffsetScaleGUI (ProceduralMaterial material)
		{
			if (m_Importer==null || targets.Length>1)
				return;

			GUILayoutOption kWidth10 = GUILayout.Width(10);
			GUILayoutOption kMinWidth32 = GUILayout.MinWidth(32);

			//////////////////////////////////////////////////////////////////////////
			// Show a single Tiling/Offset tweak
			GUILayout.BeginHorizontal();
			{
				EditorGUILayout.BeginVertical();
				{
					Vector2 scale = m_Importer.GetMaterialScale (material);
					Vector2 scaleOrig = scale;
					Vector2 offset = m_Importer.GetMaterialOffset (material);
					Vector2 offsetOrig = offset;

					GUILayout.BeginHorizontal(GUILayout.ExpandWidth(true));
					{
						GUILayout.Space(8);
						GUILayout.FlexibleSpace();
						GUILayout.BeginVertical();
						{
							GUILayout.Label("", EditorStyles.miniLabel, kWidth10);
							GUILayout.Label("x", EditorStyles.miniLabel, kWidth10);
							GUILayout.Label("y", EditorStyles.miniLabel, kWidth10);
						}
						GUILayout.EndVertical();
						GUILayout.BeginVertical();
						{
							GUILayout.Label("Tiling", EditorStyles.miniLabel);
							scale.x = EditorGUILayout.FloatField(scale.x, EditorStyles.miniTextField, kMinWidth32);
							scale.y = EditorGUILayout.FloatField(scale.y, EditorStyles.miniTextField, kMinWidth32);
						}
						GUILayout.EndVertical();
						GUILayout.BeginVertical();
						{
							GUILayout.Label("Offset", EditorStyles.miniLabel);
							offset.x = EditorGUILayout.FloatField(offset.x, EditorStyles.miniTextField, kMinWidth32);
							offset.y = EditorGUILayout.FloatField(offset.y, EditorStyles.miniTextField, kMinWidth32);
						}
						GUILayout.EndVertical();
					}
					GUILayout.EndHorizontal();

					// Apply scale and offset if changed...
					if (scaleOrig != scale || offsetOrig != offset)
					{
						Undo.RecordObjects (new Object[]{material, m_Importer}, "Modify " + material.name + "'s Tiling/Offset");
						m_Importer.SetMaterialOffset (material, offset);
						m_Importer.SetMaterialScale (material, scale);
					}
				}
				GUILayout.EndVertical();
			}
			GUILayout.EndHorizontal();
		}

		protected void InputOptions (ProceduralMaterial material)
		{
			InputsGUI ();

			if (GUI.changed)
			{
				Undo.RecordObjects (new Object[]{material, m_Importer}, "Modify " + material.name + " Procedural Property");
				material.RebuildTextures();
			}
		}

		public override void UndoRedoPerformed ()
		{
			base.UndoRedoPerformed ();
			if (m_Material != null)
			{
				m_Material.RebuildTextures();
			}
			Repaint();
		}

		[MenuItem ("CONTEXT/ProceduralMaterial/Reset", false, -100)]
		public static void ResetSubstance (MenuCommand command)
		{
			// Retrieve the Substance importer
			string path = AssetDatabase.GetAssetPath(command.context);
			m_Importer = AssetImporter.GetAtPath(path) as SubstanceImporter;
			// Reset substance
			m_Importer.ResetMaterial(command.context as ProceduralMaterial);
		}

		[MenuItem ("CONTEXT/ProceduralMaterial/Export Bitmaps", false)]
		public static void ExportBitmaps (MenuCommand command)
		{
			// Retrieve the Substance importer
			string path = AssetDatabase.GetAssetPath(command.context);
			m_Importer = AssetImporter.GetAtPath(path) as SubstanceImporter;
			m_Importer.ExportBitmaps(command.context as ProceduralMaterial);
		}

		protected void ShowProceduralTexturesGUI (ProceduralMaterial material)
		{
			if (targets.Length > 1)
				return;

			GUI.changed = false;
			EditorGUILayout.Space();

			// Show textures
			Shader shader = material.shader;
			if (shader == null)
				return;
			EditorGUILayout.BeginHorizontal();

			GUILayout.Space(4);
			GUILayout.FlexibleSpace();

			float spacing = 10;
			bool first = true;
			for (int i = 0; i < ShaderUtil.GetPropertyCount(shader); i++)
			{
				// Only show texture properties
				if (ShaderUtil.GetPropertyType(shader, i) != ShaderUtil.ShaderPropertyType.TexEnv)
					continue;

				string name = ShaderUtil.GetPropertyName(shader, i);
				Texture tex = material.GetTexture(name);

				// Only show substance textures
				if (!SubstanceImporter.IsProceduralTextureSlot(material, tex, name))
					continue;

				string label = ShaderUtil.GetPropertyDescription(shader, i);

				ShaderUtil.ShaderPropertyTexDim desiredTexdim;
				desiredTexdim = ShaderUtil.GetTexDim(shader, i);
				System.Type t;
				switch (desiredTexdim)
				{
					case ShaderUtil.ShaderPropertyTexDim.TexDim2D:
						t = typeof(Texture);
						break;
					case ShaderUtil.ShaderPropertyTexDim.TexDimCUBE:
						t = typeof(Cubemap);
						break;
					case ShaderUtil.ShaderPropertyTexDim.TexDim3D:
						t = typeof(Texture3D);
						break;
					case ShaderUtil.ShaderPropertyTexDim.TexDimAny:
						t = typeof(Texture);
						break;
					default://			TexDimUnknown, TexDimNone, TexDimDeprecated1D
						t = null;
						break;
				}

				// TODO: Move into styles class
				GUIStyle styleLabel = "ObjectPickerResultsGridLabel";

				if (first)
					first = false;
				else
					GUILayout.Space(spacing);

				GUILayout.BeginVertical(GUILayout.Height(72 + styleLabel.fixedHeight + styleLabel.fixedHeight + 8));

				Rect rect = EditorGUILayoutUtilityInternal.GetRect(72, 72);

				// Create object field with no "texture drop-box"
				DoObjectPingField(rect, rect, EditorGUIUtility.GetControlID(12354, EditorGUIUtility.native, rect), tex, t);

				rect.y = rect.yMax + 2;
				rect.height = styleLabel.fixedHeight;
				ShowAlphaSourceGUI(material, tex as ProceduralTexture, rect);
				rect.y = rect.yMax + 2;
				rect.width += spacing;
				rect.height = styleLabel.fixedHeight;
				GUI.Label(rect, label, styleLabel);

				GUILayout.EndVertical();

				GUILayout.FlexibleSpace();
			}

			GUILayout.Space(4);
			EditorGUILayout.EndHorizontal();
			GUILayout.FlexibleSpace();

		}

		Vector2 m_ScrollPos = new Vector2();
		protected void ShowGeneratedTexturesGUI (ProceduralMaterial material)
		{
			if (targets.Length > 1)
				return;

			if (m_Importer!=null && !m_Importer.GetGenerateAllOutputs(m_Material))
				return;

			GUIStyle styleLabel = "ObjectPickerResultsGridLabel";
			GUI.changed = false;
			EditorGUILayout.Space();
			GUILayout.FlexibleSpace();
			m_ScrollPos = EditorGUILayout.BeginScrollView(m_ScrollPos, GUILayout.Height(64 + styleLabel.fixedHeight + styleLabel.fixedHeight + 16));
			EditorGUILayout.BeginHorizontal();

			GUILayout.FlexibleSpace();

			float spacing = 10;
			Texture[] textures = material.GetGeneratedTextures();
			for (int i = 0; i < textures.Length; ++i)
			{
				ProceduralTexture tex = textures[i] as ProceduralTexture;
				if (tex != null)
				{
					// This hard space is there so that textures do not touch even when the inspector is really narrow,
					// even if we are already in a FlexibleSpace-enclosed block.
						GUILayout.Space(spacing);

					GUILayout.BeginVertical(GUILayout.Height(64 + styleLabel.fixedHeight + 8));

					Rect rect = EditorGUILayoutUtilityInternal.GetRect(64, 64);

					// Create object field with no "texture drop-box"
					DoObjectPingField(rect, rect, EditorGUIUtility.GetControlID(12354, EditorGUIUtility.native, rect), tex, typeof(Texture));

					rect.y = rect.yMax + 2;
					rect.height = styleLabel.fixedHeight;
					ShowAlphaSourceGUI(material, tex, rect);
					rect.y = rect.yMax + 2;
					rect.width += spacing;

					GUILayout.EndVertical();

					// This hard space is there so that textures do not touch even when the inspector is really narrow,
					// even if we are already in a FlexibleSpace-enclosed block.
					GUILayout.Space(spacing);
					
					GUILayout.FlexibleSpace();
				}
			}

			EditorGUILayout.EndHorizontal();
			EditorGUILayout.EndScrollView();
		}

		void ShowAlphaSourceGUI(ProceduralMaterial material, ProceduralTexture tex, Rect rect)
		{
			if (m_Importer != null)
			{
				EditorGUI.BeginDisabledGroup(EditorApplication.isPlaying);
				if (tex.GetProceduralOutputType() != ProceduralOutputType.Normal)
				{
					// create alpha modifier popup
					string[] m_TextureSrc = {
								"Source (A)",
								"Diffuse (A)",
								"Normal (A)",
								"Height (A)",
								"Emissive (A)",
								"Specular (A)",
								"Opacity (A)"
							};
					int alphaSource = (int)m_Importer.GetTextureAlphaSource(material, tex.name);
					EditorGUILayout.Space();
					EditorGUILayout.Space();
					alphaSource = EditorGUI.Popup(rect, alphaSource, m_TextureSrc);
					if (GUI.changed)
					{
						Undo.RecordObjects (new Object[] { material, m_Importer }, "Modify " + material.name + "'s Alpha Modifier");
						m_Importer.SetTextureAlphaSource(material, tex.name, (ProceduralOutputType)alphaSource);
						GUI.changed = false;
					}
				}
				EditorGUI.EndDisabledGroup();
			}
		}

		Object TextureValidator (Object[] references, System.Type objType, SerializedProperty property)
		{
			foreach (Object i in references)
			{
				Texture t = i as Texture;
				if (t)
				{
					return t;
				}
			}
			return null;
		}

		// Similar to ObjectField, but does not allow changing the object.
		// It will still ping or select the object when clicked / double-clicked.
		internal static void DoObjectPingField (Rect position, Rect dropRect, int id, Object obj, System.Type objType)
		{
			Event evt = Event.current;
			EventType eventType = evt.type;

			// special case test, so we continue to ping/select objects with the object field disabled
			if (!GUI.enabled && GUIClip.enabled && (Event.current.rawType == EventType.MouseDown))
				eventType = Event.current.rawType;

			bool usePreview = EditorGUIUtility.HasObjectThumbnail (objType) && position.height > EditorGUI.kSingleLineHeight;
			
			switch (eventType)
			{
				case EventType.MouseDown:
					// Ignore right clicks
					if (Event.current.button != 0)
						break;
					if (position.Contains(Event.current.mousePosition))
					{
						//EditorGUIUtility.editingTextField = false;

						Object actualTargetObject = obj;
						Component com = actualTargetObject as Component;
						if (com)
							actualTargetObject = com.gameObject;

						// One click shows where the referenced object is
						if (Event.current.clickCount == 1)
						{
							GUIUtility.keyboardControl = id;
							if (actualTargetObject)
								EditorGUIUtility.PingObject(actualTargetObject);
							evt.Use();
						}
						// Double click changes selection to referenced object
						else if (Event.current.clickCount == 2)
						{
							if (actualTargetObject)
							{
								AssetDatabase.OpenAsset(actualTargetObject);
								GUIUtility.ExitGUI();
							}
							evt.Use();
						}
					}
					break;

				case EventType.Repaint:
					GUIContent temp = EditorGUIUtility.ObjectContent(obj, objType);
					if (usePreview)
					{
						GUIStyle style = EditorStyles.objectFieldThumb;
						style.Draw(position, GUIContent.none, id, DragAndDrop.activeControlID == id);

						if (obj != null)
						{
							EditorGUI.DrawPreviewTexture(style.padding.Remove(position), temp.image);
						}
						else
						{
							GUIStyle s2 = style.name + "Overlay";
							s2.Draw(position, temp, id);
						}
					}
					else
					{
						GUIStyle style = EditorStyles.objectField;
						style.Draw(position, temp, id, DragAndDrop.activeControlID == id);
					}
					break;
			}
		}

		internal void ResetValues ()
		{
			BuildTargetList();
			if (HasModified())
				Debug.LogError("Impossible");
		}

		internal void Apply ()
		{
			foreach (ProceduralPlatformSetting ps in m_PlatformSettings)
			{
				ps.Apply();
			}
		}

		internal bool HasModified ()
		{
			foreach (ProceduralPlatformSetting ps in m_PlatformSettings)
			{
				if (ps.HasChanged())
				{
					return true;
				}
			}

			return false;
		}

		[System.Serializable]
		protected class ProceduralPlatformSetting
		{
			Object[] targets;
			public string name;
			public bool m_Overridden;
			public int maxTextureWidth;
			public int maxTextureHeight;
			public int m_TextureFormat;
			public int m_LoadBehavior;
			public BuildTarget target;
			public Texture2D icon;
			public bool isDefault { get { return name == ""; } }

			public int textureFormat
			{
				get { return m_TextureFormat; }
				set
				{
					m_TextureFormat = value;
				}

			}

			public ProceduralPlatformSetting (Object[] objects, string _name, BuildTarget _target, Texture2D _icon)
			{
				targets = objects;
				m_Overridden = false;
				target = _target;
				name = _name;
				icon = _icon;
				m_Overridden = false;
				if (name != "")
				{
					foreach (ProceduralMaterial mat in targets)
					{
						string matPath = AssetDatabase.GetAssetPath(mat);
						SubstanceImporter importer = AssetImporter.GetAtPath(matPath) as SubstanceImporter;
						if (importer!=null && importer.GetPlatformTextureSettings(mat.name, name, out maxTextureWidth, out maxTextureHeight, out m_TextureFormat, out m_LoadBehavior))
						{
							m_Overridden = true;
							break;
						}				
					}
				}
				if (!m_Overridden && targets.Length>0)
				{
					string matPath = AssetDatabase.GetAssetPath(targets[0]);
					SubstanceImporter importer = AssetImporter.GetAtPath(matPath) as SubstanceImporter;
					if (importer!=null)
						importer.GetPlatformTextureSettings((targets[0] as ProceduralMaterial).name, "", out maxTextureWidth, out maxTextureHeight, out m_TextureFormat, out m_LoadBehavior);
				}
			}

			public bool overridden
			{
				get
				{
					return m_Overridden;
				}
			}

			public void SetOverride (ProceduralPlatformSetting master)
			{
				m_Overridden = true;
			}

			public void ClearOverride (ProceduralPlatformSetting master)
			{
				m_TextureFormat = master.textureFormat;
				maxTextureWidth = master.maxTextureWidth;
				maxTextureHeight = master.maxTextureHeight;
				m_LoadBehavior = master.m_LoadBehavior;
				m_Overridden = false;
			}

			public bool HasChanged ()
			{
				ProceduralPlatformSetting orig = new ProceduralPlatformSetting(targets, name, target, null);
				return orig.m_Overridden != m_Overridden || orig.maxTextureWidth != maxTextureWidth
					|| orig.maxTextureHeight != maxTextureHeight || orig.textureFormat != textureFormat
					|| orig.m_LoadBehavior != m_LoadBehavior;
			}

			public void Apply ()
			{
				foreach (ProceduralMaterial mat in targets)
				{
					string matPath = AssetDatabase.GetAssetPath(mat);
					SubstanceImporter importer = AssetImporter.GetAtPath(matPath) as SubstanceImporter;

					if (name != "")
					{
						if (m_Overridden)
						{
							importer.SetPlatformTextureSettings(mat.name, name, maxTextureWidth, maxTextureHeight, m_TextureFormat, m_LoadBehavior);
						}
						else
						{
							importer.ClearPlatformTextureSettings(mat.name, name);
						}
					}
					else
					{
						importer.SetPlatformTextureSettings(mat.name, name, maxTextureWidth, maxTextureHeight, m_TextureFormat, m_LoadBehavior);
					}
				}
			}
		}

		protected List<ProceduralPlatformSetting> m_PlatformSettings;

		public void BuildTargetList ()
		{
			List<BuildPlayerWindow.BuildPlatform> validPlatforms = BuildPlayerWindow.GetValidPlatforms();

			m_PlatformSettings = new List<ProceduralPlatformSetting>();
			m_PlatformSettings.Add(new ProceduralPlatformSetting(targets, "", BuildTarget.StandaloneWindows, null));

			foreach (BuildPlayerWindow.BuildPlatform bp in validPlatforms)
			{
				m_PlatformSettings.Add(new ProceduralPlatformSetting(targets, bp.name, bp.DefaultTarget, bp.smallIcon));
			}
		}

		public void ShowTextureSizeGUI ()
		{
			if (m_PlatformSettings == null)
				BuildTargetList();

			TextureSizeGUI();
		}

		protected void TextureSizeGUI ()
		{
			BuildPlayerWindow.BuildPlatform[] validPlatforms = BuildPlayerWindow.GetValidPlatforms().ToArray();
			int shownTextureFormatPage = EditorGUILayout.BeginPlatformGrouping(validPlatforms, m_Styles.defaultPlatform);
			ProceduralPlatformSetting realPS = m_PlatformSettings[shownTextureFormatPage + 1];
			ProceduralPlatformSetting ps = realPS;

			bool newOverride = true;
			if (realPS.name != "")
			{
				GUI.changed = false;
				newOverride = GUILayout.Toggle(realPS.overridden, "Override for " + realPS.name);
				if (GUI.changed)
				{
					if (newOverride)
					{
						realPS.SetOverride(m_PlatformSettings[0]);
					}
					else
					{
						realPS.ClearOverride(m_PlatformSettings[0]);
					}
				}
			}

			EditorGUI.BeginDisabledGroup (!newOverride);

			GUI.changed = false;
			ps.maxTextureWidth = EditorGUILayout.IntPopup(m_Styles.targetWidth.text, ps.maxTextureWidth, kMaxTextureSizeStrings, kMaxTextureSizeValues);
			ps.maxTextureHeight = EditorGUILayout.IntPopup(m_Styles.targetHeight.text, ps.maxTextureHeight, kMaxTextureSizeStrings, kMaxTextureSizeValues);
			if (GUI.changed && ps.isDefault)
			{
				foreach (ProceduralPlatformSetting psToUpdate in m_PlatformSettings)
				{
					if (psToUpdate.isDefault || psToUpdate.overridden) continue;
					psToUpdate.maxTextureWidth = ps.maxTextureWidth;
					psToUpdate.maxTextureHeight = ps.maxTextureHeight;
				}
			}

			GUI.changed = false;
			int tf = (int)ps.textureFormat;
			if (tf<0 || tf>1)
			{
				Debug.LogError("Invalid TextureFormat");
			}
			tf = EditorGUILayout.Popup(m_Styles.textureFormat, tf, m_Styles.textureFormatOptions);
			if (GUI.changed)
			{
				ps.textureFormat = tf;
				if (ps.isDefault)
				{
					foreach (ProceduralPlatformSetting psToUpdate in m_PlatformSettings)
					{
						if (psToUpdate.isDefault || psToUpdate.overridden) continue;
						psToUpdate.textureFormat = ps.textureFormat;
					}
				}
			}
			GUI.changed = false;
			ps.m_LoadBehavior = EditorGUILayout.IntPopup(m_Styles.loadBehavior.text, ps.m_LoadBehavior, kMaxLoadBehaviorStrings, kMaxLoadBehaviorValues);
			if (GUI.changed && ps.isDefault)
			{
				foreach (ProceduralPlatformSetting psToUpdate in m_PlatformSettings)
				{
					if (psToUpdate.isDefault || psToUpdate.overridden) continue;
					psToUpdate.m_LoadBehavior = ps.m_LoadBehavior;
				}
			}
			GUI.changed = false;

			GUILayout.Space(5);
			EditorGUI.BeginDisabledGroup (!HasModified ());
			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();
			if (GUILayout.Button("Revert"))
			{
				ResetValues();
			}

			if (GUILayout.Button("Apply"))
			{
				Apply();
				ReimportSubstances();
				ResetValues();
			}
			GUILayout.EndHorizontal();
			EditorGUI.EndDisabledGroup ();

			GUILayout.Space(5);
			EditorGUILayout.EndPlatformGrouping();

			EditorGUI.EndDisabledGroup ();
		}

		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			base.OnPreviewGUI (r, background);
			if (ShowIsGenerating (target as ProceduralMaterial) && r.width > 50)
				EditorGUI.DropShadowLabel(new Rect (r.x, r.y, r.width, 20), "Generating...");
		}

		public void InputsGUI ()
		{
			ProceduralPropertyDescription[] inputs = m_Material.GetProceduralPropertyDescriptions();

			// Process inputs to see if we can group some of them as compound inputs (e.g. colors) or if they should be used as single inputs
			ProceduralPropertyDescription inputH = null;
			ProceduralPropertyDescription inputS = null;
			ProceduralPropertyDescription inputL = null;

			m_LastGroup = string.Empty;
			for (int i=0; i<inputs.Length; i++)
			{
				ProceduralPropertyDescription input = inputs[i];

				if (input.name == "$outputsize") // Do not show outputsize
				{
					continue;
				}
				else if (input.name == "$randomseed") // Random seed GUI
				{
					InputSeedGUI (input);
				}
				else if (input.name == "Hue_Shift" && input.group == string.Empty) // Found hue
				{
					inputH = input;
					continue;
				}
				else if (input.name == "Saturation" && input.group == string.Empty) // Found saturation
				{
					inputS = input;
					continue;
				}
				else if (input.name == "Luminosity" && input.group == string.Empty) // Found luminosity
				{
					inputL = input;
					continue;
				}
				else // Display as regular single input
				{
					// Skip all special inputs since they require special controls
					if (input.name.Length > 0 && input.name[0] == '$')
						continue;
					InputGUI(input);
				}
			}
			if (inputH != null && inputS != null && inputL != null
				&& inputH.type == ProceduralPropertyType.Float
				&& inputS.type == ProceduralPropertyType.Float
				&& inputL.type == ProceduralPropertyType.Float)
			{
				// We got our HSL color input!
				InputHSLGUI(inputH, inputS, inputL);
			}
			else
			{
				if (inputH != null)
					InputGUI(inputH);
				if (inputS != null)
					InputGUI(inputS);
				if (inputL != null)
					InputGUI(inputL);
			}
		}

		private void InputGUI(ProceduralPropertyDescription input)
		{
			bool showGroup = true;
			ProceduralMaterial material = target as ProceduralMaterial;
			string materialName = material.name;
			string groupCompleteName = materialName + input.group;
			if (input.group != m_LastGroup)
			{
				GUILayout.Space(5);
				if (input.group != string.Empty)
				{
					bool changed = GUI.changed;
					m_LastGroup = input.group;
					showGroup = EditorPrefs.GetBool(groupCompleteName, true);
					GUI.changed = false;
					showGroup = EditorGUILayout.Foldout(showGroup, input.group);
					if (GUI.changed)
					{
						EditorPrefs.SetBool(groupCompleteName, showGroup);
					}
					GUI.changed = changed;
				}
			}
			else
			{
				showGroup = EditorPrefs.GetBool(groupCompleteName, true);
			}
			if (showGroup || (input.group == string.Empty))
			{
				int oldIndent = EditorGUI.indentLevel;
				if (input.group != string.Empty)
					EditorGUI.indentLevel++;

				ProceduralPropertyType type = input.type;
				GUIContent content = new GUIContent(input.label, input.name);
				switch (type)
				{
					case ProceduralPropertyType.Boolean:
					{
						bool val = m_Material.GetProceduralBoolean(input.name);
						bool oldVal = val;
						val = EditorGUILayout.Toggle(content, val);
						if (val != oldVal)
							m_Material.SetProceduralBoolean(input.name, val);
						break;
					}
					case ProceduralPropertyType.Float:
					{
						float val = m_Material.GetProceduralFloat(input.name);
						float oldVal = val;
						if (input.hasRange)
						{
							float min = input.minimum;
							float max = input.maximum;
							val = EditorGUILayout.Slider(content, val, min, max);
						}
						else
						{
							val = EditorGUILayout.FloatField(content, val);
						}
						if (val != oldVal)
							m_Material.SetProceduralFloat(input.name, val);
						break;
					}
					case ProceduralPropertyType.Vector2:
					case ProceduralPropertyType.Vector3:
					case ProceduralPropertyType.Vector4:
					{
						int inputCount = (type == ProceduralPropertyType.Vector2 ? 2 : (type == ProceduralPropertyType.Vector3 ? 3 : 4));
						Vector4 val = m_Material.GetProceduralVector(input.name);
						Vector4 oldVal = val;

						if (input.hasRange)
						{
							float min = input.minimum;
							float max = input.maximum;
							EditorGUILayout.BeginVertical();
							GUILayout.Label(content);
							EditorGUI.indentLevel++;
							for (int i = 0; i < inputCount; i++)
								val[i] = EditorGUILayout.Slider(m_Styles.coordNames[i], val[i], min, max);
							EditorGUI.indentLevel--;
							EditorGUILayout.EndVertical();
						}
						else
						{
							switch (inputCount) {
								case 2: val = EditorGUILayout.Vector2Field(input.name, val); break;
								case 3: val = EditorGUILayout.Vector3Field(input.name, val); break;
								case 4: val = EditorGUILayout.Vector4Field(input.name, val); break;
							}
						}
						if (val != oldVal)
							m_Material.SetProceduralVector(input.name, val);
						break;
					}
					case ProceduralPropertyType.Color3:
					case ProceduralPropertyType.Color4:
					{
						Color val = m_Material.GetProceduralColor(input.name);
						Color oldVal = val;
						val = EditorGUILayout.ColorField(content, val);
						if (val != oldVal)
							m_Material.SetProceduralColor(input.name, val);
						break;
					}
					case ProceduralPropertyType.Enum:
					{
						int val = m_Material.GetProceduralEnum(input.name);
						int oldVal = val;
						GUIContent[] enumOptions = new GUIContent[input.enumOptions.Length];
						for (int i=0 ; i<enumOptions.Length ; ++i)
						{
							enumOptions[i] = new GUIContent(input.enumOptions[i]);
						}
						val = EditorGUILayout.Popup(content, val, enumOptions);
						if (val != oldVal)
							m_Material.SetProceduralEnum(input.name, val);
						break;
					}
					case ProceduralPropertyType.Texture:
					{
						Texture2D val = m_Material.GetProceduralTexture(input.name);
						Texture2D oldVal = val;
						EditorGUILayout.BeginHorizontal ();
						GUILayout.Label(content);
						GUILayout.FlexibleSpace ();
						Rect R = GUILayoutUtility.GetRect (64, 64, GUILayout.ExpandWidth (false));
						val = EditorGUI.DoObjectField (R, R, EditorGUIUtility.GetControlID (12354, EditorGUIUtility.native, R), val as Object, typeof (Texture2D), null, null , false) as Texture2D;
						EditorGUILayout.EndHorizontal ();
						if (val != oldVal)
							m_Material.SetProceduralTexture(input.name, val);
						break;
					}
				}
				EditorGUI.indentLevel = oldIndent;
			}
		}

		private void InputHSLGUI(ProceduralPropertyDescription hInput, ProceduralPropertyDescription sInput, ProceduralPropertyDescription lInput)
		{
			bool changed = GUI.changed;
			GUILayout.Space(5);
			m_ShowHSLInputs = EditorPrefs.GetBool("ProceduralShowHSL", true);
			GUI.changed = false;
			m_ShowHSLInputs = EditorGUILayout.Foldout(m_ShowHSLInputs, m_Styles.hslContent);
			if (GUI.changed)
			{
				EditorPrefs.SetBool("ProceduralShowHSL", m_ShowHSLInputs);
			}
			GUI.changed = changed;

			if (m_ShowHSLInputs)
			{
				EditorGUI.indentLevel++;
				InputGUI(hInput);
				InputGUI(sInput);
				InputGUI(lInput);
				EditorGUI.indentLevel--;
			}
		}

		private void InputSeedGUI(ProceduralPropertyDescription input)
		{
			float val = m_Material.GetProceduralFloat(input.name);
			float oldVal = val;
			Rect r = EditorGUILayout.GetControlRect ();
			val = (float)RandomIntField(r, m_Styles.randomSeedContent, (int)val, 0, 9999);
			if (val != oldVal)
				m_Material.SetProceduralFloat(input.name, val);
		}

		internal int RandomIntField(Rect position, GUIContent label, int val, int min, int max)
		{
			position = EditorGUI.PrefixLabel (position, 0, label);
			return RandomIntField (position, val, min, max);
		}

		internal int RandomIntField (Rect position, int val, int min, int max)
		{
			position.width = position.width - EditorGUIUtility.fieldWidth - EditorGUI.kSpacing;

			if (GUI.Button (position, m_Styles.randomizeButtonContent, EditorStyles.miniButton))
			{
				val = Random.Range(min, max+1);
				GUI.changed = true;
			}

			position.x += position.width + EditorGUI.kSpacing;
			position.width = EditorGUIUtility.fieldWidth;
			val = Mathf.Clamp(EditorGUI.IntField (position, val), min, max);

			return val;
		}
	}
}
