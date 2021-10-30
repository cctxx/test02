using UnityEngine;
using UnityEditor;
using System;
using System.Collections;

namespace UnityEditor
{	

	internal class ModelImporterModelEditor : AssetImporterInspector
	{
		bool m_SecondaryUVAdvancedOptions = false;
		bool m_ShowAllMaterialNameOptions = true;
		
		// Material
		SerializedProperty m_ImportMaterials;
		SerializedProperty m_MaterialName;
		SerializedProperty m_MaterialSearch;
		
		// Model
		SerializedProperty m_GlobalScale;
		SerializedProperty m_MeshCompression;
		SerializedProperty m_ImportBlendShapes;
		SerializedProperty m_AddColliders;
		SerializedProperty m_SwapUVChannels;
		SerializedProperty m_GenerateSecondaryUV;
		SerializedProperty m_UseFileUnits;
		SerializedProperty m_SecondaryUVAngleDistortion;
		SerializedProperty m_SecondaryUVAreaDistortion;
		SerializedProperty m_SecondaryUVHardAngle;
		SerializedProperty m_SecondaryUVPackMargin;
		SerializedProperty m_NormalSmoothAngle;
		SerializedProperty m_SplitTangentsAcrossSeams;
		SerializedProperty m_NormalImportMode;
		SerializedProperty m_TangentImportMode;
	SerializedProperty m_OptimizeMeshForGPU;
		SerializedProperty m_IsReadable;
			
		private void UpdateShowAllMaterialNameOptions()
		{
			// We need to display BasedOnTextureName_Or_ModelNameAndMaterialName obsolete option for objects which use this option
#pragma warning disable 618
			m_MaterialName = serializedObject.FindProperty ("m_MaterialName");
			m_ShowAllMaterialNameOptions = (m_MaterialName.intValue == (int)ModelImporterMaterialName.BasedOnTextureName_Or_ModelNameAndMaterialName);
#pragma warning restore 618
		}
		
		void OnEnable ()
		{
			// Material
			m_ImportMaterials = serializedObject.FindProperty("m_ImportMaterials");
			m_MaterialName = serializedObject.FindProperty ("m_MaterialName");
			m_MaterialSearch = serializedObject.FindProperty ("m_MaterialSearch");
			
			// Model
			m_GlobalScale = serializedObject.FindProperty ("m_GlobalScale");
			m_MeshCompression = serializedObject.FindProperty ("m_MeshCompression");
			m_ImportBlendShapes = serializedObject.FindProperty("m_ImportBlendShapes");
			m_AddColliders = serializedObject.FindProperty ("m_AddColliders");
			m_SwapUVChannels = serializedObject.FindProperty ("swapUVChannels");
			m_GenerateSecondaryUV = serializedObject.FindProperty ("generateSecondaryUV");
			m_UseFileUnits = serializedObject.FindProperty ("m_UseFileUnits");
			m_SecondaryUVAngleDistortion = serializedObject.FindProperty ("secondaryUVAngleDistortion");
			m_SecondaryUVAreaDistortion = serializedObject.FindProperty ("secondaryUVAreaDistortion");
			m_SecondaryUVHardAngle = serializedObject.FindProperty ("secondaryUVHardAngle");
			m_SecondaryUVPackMargin = serializedObject.FindProperty ("secondaryUVPackMargin");
			m_NormalSmoothAngle = serializedObject.FindProperty ("normalSmoothAngle");
			m_SplitTangentsAcrossSeams = serializedObject.FindProperty ("splitTangentsAcrossUV");
			m_NormalImportMode = serializedObject.FindProperty ("normalImportMode");
			m_TangentImportMode = serializedObject.FindProperty ("tangentImportMode");
		m_OptimizeMeshForGPU = serializedObject.FindProperty ("optimizeMeshForGPU");
			m_IsReadable = serializedObject.FindProperty ("m_IsReadable");
		
			UpdateShowAllMaterialNameOptions();
		}
		
		internal override void ResetValues ()
		{
			base.ResetValues ();
			UpdateShowAllMaterialNameOptions();
		}
		
		internal override void Apply ()
		{
            ScaleAvatar();
			base.Apply ();
			UpdateShowAllMaterialNameOptions();
		}
		
		class Styles {
			public GUIContent Meshes = EditorGUIUtility.TextContent ("ModelImporter.Meshes");
			public GUIContent ScaleFactor = EditorGUIUtility.TextContent ("ModelImporter.ScaleFactor");
			public GUIContent UseFileUnits = EditorGUIUtility.TextContent("ModelImporter.UseFileUnits");
			public GUIContent ImportBlendShapes = EditorGUIUtility.TextContent("ModelImporter.ImportBlendShapes");
			public GUIContent GenerateColliders = EditorGUIUtility.TextContent ("ModelImporter.GenerateColliders");
			public GUIContent SwapUVChannels = EditorGUIUtility.TextContent("ModelImporter.SwapUVChannels");
			
			public GUIContent GenerateSecondaryUV			= EditorGUIUtility.TextContent("ModelImporterGenerateSecondaryUV");
			public GUIContent GenerateSecondaryUVAdvanced	= EditorGUIUtility.TextContent("ModelImporterGenerateSecondaryUVAdvanced");
			public GUIContent secondaryUVAngleDistortion	= EditorGUIUtility.TextContent("ModelImporterSecondaryUVAngleDistortion");
			public GUIContent secondaryUVAreaDistortion		= EditorGUIUtility.TextContent("ModelImporterSecondaryUVAreaDistortion");
			public GUIContent secondaryUVHardAngle			= EditorGUIUtility.TextContent("ModelImporterSecondaryUVHardAngle");
			public GUIContent secondaryUVPackMargin			= EditorGUIUtility.TextContent("ModelImporterSecondaryUVPackMargin");
			public GUIContent secondaryUVDefaults			= EditorGUIUtility.TextContent("ModelImporterSecondaryUVDefaults");
	
			public GUIContent TangentSpace = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Title");
			public GUIContent TangentSpaceNormalLabel = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Normals");
			public GUIContent TangentSpaceTangentLabel = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Tangents");
	
			public GUIContent TangentSpaceOptionImport = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Options.Import");
			public GUIContent TangentSpaceOptionCalculate = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Options.Calculate");
			public GUIContent TangentSpaceOptionNone = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.Options.None");
	
			public GUIContent[] TangentSpaceModeOptLabelsAll;
			public GUIContent[] TangentSpaceModeOptLabelsCalculate;
			public GUIContent[] TangentSpaceModeOptLabelsNone;
	
			public ModelImporterTangentSpaceMode[] TangentSpaceModeOptEnumsAll;
			public ModelImporterTangentSpaceMode[] TangentSpaceModeOptEnumsCalculate;
			public ModelImporterTangentSpaceMode[] TangentSpaceModeOptEnumsNone;
	
			public GUIContent SmoothingAngle = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.NormalSmoothingAngle");
			public GUIContent SplitTangents = EditorGUIUtility.TextContent("ModelImporter.TangentSpace.SplitTangents");
	
			public GUIContent MeshCompressionLabel = new GUIContent("Mesh Compression");
			public GUIContent[] MeshCompressionOpt = {
				new GUIContent ("Off"),
				new GUIContent ("Low"),
				new GUIContent ("Medium"),
				new GUIContent ("High")
			};
	
		public GUIContent OptimizeMeshForGPU = EditorGUIUtility.TextContent("ModelImporterOptimizeMesh");
			public GUIContent IsReadable = EditorGUIUtility.TextContent("ModelImporterIsReadable");
			public GUIContent Materials = EditorGUIUtility.TextContent ("ModelImporterMaterials");
			public GUIContent ImportMaterials = EditorGUIUtility.TextContent("ModelImporterMatImportMaterials");
			public GUIContent MaterialName = EditorGUIUtility.TextContent("ModelImporterMatMaterialName");
			public GUIContent MaterialNameTex = EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTex");
			public GUIContent MaterialNameMat = EditorGUIUtility.TextContent("ModelImporterMatMaterialNameMat");
			public GUIContent[] MaterialNameOptMain = {
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTex"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameMat"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameModelMat"),
			};
			public GUIContent[] MaterialNameOptAll = {
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTex"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameMat"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameModelMat"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTex_ModelMat"),
			};
			public GUIContent MaterialSearch = EditorGUIUtility.TextContent("ModelImporterMatMaterialSearch");
			public GUIContent[] MaterialSearchOpt = {
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchLocal"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchRecursiveUp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchEverywhere")
			};
	
			public GUIContent MaterialHelpStart = EditorGUIUtility.TextContent("ModelImporterMatHelpStart");
			public GUIContent MaterialHelpEnd = EditorGUIUtility.TextContent("ModelImporterMatHelpEnd");
			public GUIContent MaterialHelpDefault = EditorGUIUtility.TextContent("ModelImporterMatDefaultHelp");
			public GUIContent[] MaterialNameHelp = {
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTexHelp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameMatHelp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameModelMatHelp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialNameTex_ModelMatHelp"),
			};
			public GUIContent[] MaterialSearchHelp = {
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchLocalHelp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchRecursiveUpHelp"),
				EditorGUIUtility.TextContent("ModelImporterMatMaterialSearchEverywhereHelp")
			};
	
			public Styles()
			{
				TangentSpaceModeOptLabelsAll = new GUIContent[] { TangentSpaceOptionImport, TangentSpaceOptionCalculate, TangentSpaceOptionNone };
				TangentSpaceModeOptLabelsCalculate = new GUIContent[] { TangentSpaceOptionCalculate, TangentSpaceOptionNone };
				TangentSpaceModeOptLabelsNone = new GUIContent[] { TangentSpaceOptionNone };
	
				TangentSpaceModeOptEnumsAll = new ModelImporterTangentSpaceMode[] { ModelImporterTangentSpaceMode.Import, ModelImporterTangentSpaceMode.Calculate, ModelImporterTangentSpaceMode.None };
				TangentSpaceModeOptEnumsCalculate = new ModelImporterTangentSpaceMode[] { ModelImporterTangentSpaceMode.Calculate, ModelImporterTangentSpaceMode.None };
				TangentSpaceModeOptEnumsNone = new ModelImporterTangentSpaceMode[] { ModelImporterTangentSpaceMode.None };
			}
		}
	
		static Styles styles;
	
		public override void OnInspectorGUI ()
		{
			// Material modes
			if (styles == null)
				styles = new Styles();
	
			GUILayout.Label(styles.Meshes, EditorStyles.boldLabel);
			// Global scale
			EditorGUILayout.PropertyField (m_GlobalScale, styles.ScaleFactor);
	
			// BackwardsCompatibleUnitImport
			bool isUseFileUnitsSupported = true;
			foreach (ModelImporter importer in targets)
				if (!importer.isUseFileUnitsSupported)
					isUseFileUnitsSupported = false;
			if (isUseFileUnitsSupported)
				EditorGUILayout.PropertyField (m_UseFileUnits, styles.UseFileUnits);
	
			// mesh compression
			EditorGUILayout.Popup (m_MeshCompression, styles.MeshCompressionOpt, styles.MeshCompressionLabel);
	
			EditorGUILayout.PropertyField (m_IsReadable, styles.IsReadable);
			EditorGUILayout.PropertyField (m_OptimizeMeshForGPU, styles.OptimizeMeshForGPU);
	
			// Import BlendShapes
			EditorGUILayout.PropertyField(m_ImportBlendShapes, styles.ImportBlendShapes);

			// Add Collider
			EditorGUILayout.PropertyField (m_AddColliders, styles.GenerateColliders);
	
			// Swap uv channel
			EditorGUILayout.PropertyField (m_SwapUVChannels, styles.SwapUVChannels);
	
			// Secondary UV generation
			EditorGUILayout.PropertyField (m_GenerateSecondaryUV, styles.GenerateSecondaryUV);
			if (m_GenerateSecondaryUV.boolValue)
			{
				EditorGUI.indentLevel++;
				m_SecondaryUVAdvancedOptions = EditorGUILayout.Foldout (m_SecondaryUVAdvancedOptions, styles.GenerateSecondaryUVAdvanced, EditorStyles.foldout);
				if (m_SecondaryUVAdvancedOptions)
				{
					// TODO: all slider min/max values should be revisited
					EditorGUI.BeginChangeCheck ();
					EditorGUILayout.Slider (m_SecondaryUVHardAngle, 0, 180, styles.secondaryUVHardAngle);
					EditorGUILayout.Slider (m_SecondaryUVPackMargin, 1, 64, styles.secondaryUVPackMargin);
					EditorGUILayout.Slider (m_SecondaryUVAngleDistortion, 1, 75, styles.secondaryUVAngleDistortion);
					EditorGUILayout.Slider (m_SecondaryUVAreaDistortion, 1, 75, styles.secondaryUVAreaDistortion);
					if (EditorGUI.EndChangeCheck ())
					{
						m_SecondaryUVHardAngle.floatValue = Mathf.Round (m_SecondaryUVHardAngle.floatValue);
						m_SecondaryUVPackMargin.floatValue = Mathf.Round (m_SecondaryUVPackMargin.floatValue);
						m_SecondaryUVAngleDistortion.floatValue = Mathf.Round (m_SecondaryUVAngleDistortion.floatValue);
						m_SecondaryUVAreaDistortion.floatValue = Mathf.Round (m_SecondaryUVAreaDistortion.floatValue);
					}
				}
				EditorGUI.indentLevel--;
			}
	
			{
				// Tangent space
				GUILayout.Label(styles.TangentSpace, EditorStyles.boldLabel);
	
				bool isTangentImportSupported = true;
				foreach (ModelImporter importer in targets)
					if (!importer.isTangentImportSupported)
						isTangentImportSupported = false;
	
				// TODO : check if normal import is supported!
				//normalImportMode = styles.TangentSpaceModeOptEnumsAll[EditorGUILayout.Popup(styles.TangentSpaceNormalLabel, (int)normalImportMode, styles.TangentSpaceModeOptLabelsAll)];
				EditorGUI.BeginChangeCheck ();
				EditorGUILayout.Popup (m_NormalImportMode, styles.TangentSpaceModeOptLabelsAll, styles.TangentSpaceNormalLabel);
				if (EditorGUI.EndChangeCheck ())
				{
					// Let the tangent mode follow the normal mode - that's a sane default and it's needed
					// because the tangent mode value can't be lower than the normal mode.
					// We make the tangent mode follow in BOTH directions for consistency
					// - so that if you change the normal mode one way and then back, the tangent mode will also go back again.
					m_TangentImportMode.intValue = m_NormalImportMode.intValue;
					if (!isTangentImportSupported && m_TangentImportMode.intValue == 0)
						m_TangentImportMode.intValue = 1;
				}
	
				// Choose the option values and labels based on what the NormalImportMode is
				GUIContent[] tangentImportModeOptLabels = styles.TangentSpaceModeOptLabelsAll;
				ModelImporterTangentSpaceMode[] tangentImportModeOptEnums = styles.TangentSpaceModeOptEnumsAll;
				if (m_NormalImportMode.intValue == (int)ModelImporterTangentSpaceMode.Calculate || !isTangentImportSupported)
				{
					tangentImportModeOptLabels = styles.TangentSpaceModeOptLabelsCalculate;
					tangentImportModeOptEnums = styles.TangentSpaceModeOptEnumsCalculate;
				}
				else if (m_NormalImportMode.intValue == (int)ModelImporterTangentSpaceMode.None)
				{
					tangentImportModeOptLabels = styles.TangentSpaceModeOptLabelsNone;
					tangentImportModeOptEnums = styles.TangentSpaceModeOptEnumsNone;
				}
	
				EditorGUI.BeginDisabledGroup (m_NormalImportMode.intValue == (int)ModelImporterTangentSpaceMode.None);
				int tangentOption = Array.IndexOf (tangentImportModeOptEnums, (ModelImporterTangentSpaceMode)m_TangentImportMode.intValue);
				EditorGUI.BeginChangeCheck ();
				tangentOption = EditorGUILayout.Popup(styles.TangentSpaceTangentLabel, tangentOption, tangentImportModeOptLabels);
				if (EditorGUI.EndChangeCheck ())
					m_TangentImportMode.intValue = (int)tangentImportModeOptEnums[tangentOption];
				EditorGUI.EndDisabledGroup ();
	
				// Normal split angle
				EditorGUI.BeginDisabledGroup (m_NormalImportMode.intValue != (int)ModelImporterTangentSpaceMode.Calculate);
				EditorGUI.BeginChangeCheck ();
				EditorGUILayout.Slider (m_NormalSmoothAngle, 0, 180, styles.SmoothingAngle);
				// Property is serialized as float but we want to show it as an int so we round the value when changed
				if (EditorGUI.EndChangeCheck ())
					m_NormalSmoothAngle.floatValue = Mathf.Round (m_NormalSmoothAngle.floatValue);
				EditorGUI.EndDisabledGroup ();
	
				// Split tangents
				EditorGUI.BeginDisabledGroup (m_TangentImportMode.intValue != (int)ModelImporterTangentSpaceMode.Calculate);
				EditorGUILayout.PropertyField (m_SplitTangentsAcrossSeams, styles.SplitTangents);
				EditorGUI.EndDisabledGroup ();
			}
	
			GUILayout.Label(styles.Materials, EditorStyles.boldLabel);
			EditorGUILayout.PropertyField(m_ImportMaterials, styles.ImportMaterials);
	
			string materialHelp;
			if (m_ImportMaterials.boolValue)
			{
				EditorGUILayout.Popup (m_MaterialName, m_ShowAllMaterialNameOptions ? styles.MaterialNameOptAll : styles.MaterialNameOptMain, styles.MaterialName);
				EditorGUILayout.Popup (m_MaterialSearch, styles.MaterialSearchOpt, styles.MaterialSearch);
	
				materialHelp =
					styles.MaterialHelpStart.text.Replace("%MAT%", styles.MaterialNameHelp[m_MaterialName.intValue].text) + "\n" +
					styles.MaterialSearchHelp[m_MaterialSearch.intValue].text + "\n" +
					styles.MaterialHelpEnd.text;
			}
			else
				materialHelp = styles.MaterialHelpDefault.text;
			GUILayout.Label(new GUIContent(materialHelp), EditorStyles.helpBox);
	
			ApplyRevertGUI();
			                                        		    
		}    

        private void ScaleAvatar()
        {            
            foreach (object o in targets)
            {
                float prevScale = (o as ModelImporter).globalScale;
                float currentScale = m_GlobalScale.floatValue;

                if (prevScale != currentScale && currentScale != 0 && prevScale != 0)
                {
                    float scaleFactor = currentScale/prevScale;
                    SerializedProperty skeletonBoneArray = serializedObject.FindProperty(AvatarSetupTool.sSkeleton);
                    for (int i = 0; i < skeletonBoneArray.arraySize; ++i)
                    {
                        SerializedProperty bone = skeletonBoneArray.GetArrayElementAtIndex(i);
                        bone.FindPropertyRelative(AvatarSetupTool.sPosition).vector3Value =
                            bone.FindPropertyRelative(AvatarSetupTool.sPosition).vector3Value*scaleFactor;
                    }
                }
            }
        }
	}

    

}
