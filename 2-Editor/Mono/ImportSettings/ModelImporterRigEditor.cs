using System;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using Object = UnityEngine.Object;
using System.IO;

namespace UnityEditor
{
	internal class ModelImporterRigEditor : AssetImporterInspector
	{
		const float kDeleteWidth = 17;
		
		ModelImporter singleImporter { get { return targets[0] as ModelImporter; } }
		
		public int m_SelectedClipIndex = -1;
		
		Avatar m_Avatar;
		
		SerializedProperty m_OptimizeGameObjects;
		SerializedProperty m_AnimationType;
		SerializedProperty m_AvatarSource;
		SerializedProperty m_CopyAvatar;
		SerializedProperty m_LegacyGenerateAnimations;
		SerializedProperty m_AnimationCompression;
		
		SerializedProperty m_RootMotionBoneName;
		GUIContent [] m_RootMotionBoneList;

		private ExposeTransformEditor m_ExposeTransformEditor;

		private ModelImporterAnimationType animationType
		{
			get { return (ModelImporterAnimationType)m_AnimationType.intValue; }
			set { m_AnimationType.intValue = (int)value; } 
		}
		
		bool m_AvatarCopyIsUpToDate;
		private bool m_CanMultiEditTransformList;
		
		public int rootIndex { get; set; }
		
		private class Styles
		{
			public GUIContent AnimationType = EditorGUIUtility.TextContent("ModelImporterAnimationType");
			public GUIContent[] AnimationTypeOpt = {
				EditorGUIUtility.TextContent ("ModelImporterAnimationTypeNone"),
				EditorGUIUtility.TextContent ("ModelImporterAnimationTypeLegacy"),
				EditorGUIUtility.TextContent ("ModelImporterAnimationTypeGeneric"),
				EditorGUIUtility.TextContent ("ModelImporterAnimationTypeHumanoid")
			};
			
			public GUIContent AnimLabel = EditorGUIUtility.TextContent ("ModelImporterAnimLabel");
			public GUIContent[] AnimationsOpt = {
				EditorGUIUtility.TextContent ("ModelImporterAnimNone"),
				EditorGUIUtility.TextContent ("ModelImporterAnimOrigRoot"),
				EditorGUIUtility.TextContent ("ModelImporterAnimNode"),
				EditorGUIUtility.TextContent ("ModelImporterAnimRoot"),
				EditorGUIUtility.TextContent ("ModelImporterAnimRootNew")
			};
			public GUIStyle helpText = new GUIStyle (EditorStyles.helpBox);
			
			public GUIContent avatar = new GUIContent ("Animator");
			public GUIContent configureAvatar = EditorGUIUtility.TextContent ("Configure...");
			public GUIContent avatarValid = EditorGUIUtility.TextContent ("\u2713");
			public GUIContent avatarInvalid = EditorGUIUtility.TextContent ("\u2715");
			public GUIContent avatarPending = EditorGUIUtility.TextContent ("...");

		
			public GUIContent UpdateMuscleDefinitionFromSource = EditorGUIUtility.TextContent("ModelImporterRigUpdateMuscleDefinitionFromSource");
			public GUIContent RootNode = EditorGUIUtility.TextContent("ModelImporterRigRootNode");

			public GUIContent AvatarDefinition = EditorGUIUtility.TextContent("ModelImporterRigAvatarDefinition");

			public GUIContent[] AvatarDefinitionOpt = {
				EditorGUIUtility.TextContent("ModelImporterRigCreateFromThisModel"),
				EditorGUIUtility.TextContent("ModelImporterRigCopyFromOtherAvatar")
			};

			public GUIContent UpdateReferenceClips = EditorGUIUtility.TextContent("ModelImporterRigUpdateReferenceClips");
			
			public Styles ()
			{
				helpText.normal.background = null;
				helpText.alignment = TextAnchor.MiddleLeft;
				helpText.padding = new RectOffset (0,0,0,0);
			}
		}
		static Styles styles;
		
		public void OnEnable ()
		{
			m_AnimationType = serializedObject.FindProperty("m_AnimationType");
			m_AvatarSource = serializedObject.FindProperty("m_LastHumanDescriptionAvatarSource");
			m_OptimizeGameObjects = serializedObject.FindProperty("m_OptimizeGameObjects");

			// Generic bone setup
			m_RootMotionBoneName = serializedObject.FindProperty("m_HumanDescription.m_RootMotionBoneName");
			m_ExposeTransformEditor = new ExposeTransformEditor();

			string[] transformPaths = singleImporter.transformPaths;
			m_RootMotionBoneList = new GUIContent[transformPaths.Length];
			for (int i = 0; i < transformPaths.Length; i++)
				m_RootMotionBoneList[i] = new GUIContent(transformPaths[i]);

			if(m_RootMotionBoneList.Length > 0 )
				m_RootMotionBoneList[0] = new GUIContent("None");

			rootIndex = ArrayUtility.FindIndex(m_RootMotionBoneList, delegate(GUIContent content) { return FileUtil.GetLastPathNameComponent(content.text) == m_RootMotionBoneName.stringValue; });
            rootIndex = rootIndex < 1 ? 0 : rootIndex;

			// Animation
			m_CopyAvatar = serializedObject.FindProperty ("m_CopyAvatar");
			m_LegacyGenerateAnimations = serializedObject.FindProperty ("m_LegacyGenerateAnimations");
			m_AnimationCompression = serializedObject.FindProperty("m_AnimationCompression");

			m_ExposeTransformEditor.OnEnable(singleImporter.transformPaths, serializedObject);

			m_CanMultiEditTransformList = CanMultiEditTransformList();
			
			// Check if avatar definition is same as the one it's copied from
			CheckIfAvatarCopyIsUpToDate ();
		}

		private bool CanMultiEditTransformList()
		{
			string[] transformPaths = singleImporter.transformPaths;
			for (int i = 1; i < targets.Length; ++i)
			{
				ModelImporter modelImporter = targets[i] as ModelImporter;
				if (!ArrayUtility.ArrayEquals(transformPaths, modelImporter.transformPaths))
					return false;
			}

			return true;
		}

		void CheckIfAvatarCopyIsUpToDate ()
		{
            if  ( !(animationType == ModelImporterAnimationType.Human || animationType == ModelImporterAnimationType.Generic) || m_AvatarSource.objectReferenceValue == null)
			{
				m_AvatarCopyIsUpToDate = true;
				return;
			}
			
			// Get SerializedObject of this importer and the importer of the source avatar
			string path = AssetDatabase.GetAssetPath (m_AvatarSource.objectReferenceValue);
			ModelImporter sourceImporter = AssetImporter.GetAtPath (path) as ModelImporter;
			
			m_AvatarCopyIsUpToDate = DoesHumanDescriptionMatch(singleImporter, sourceImporter);
		}
		
		internal override void ResetValues ()
		{
			base.ResetValues ();
			m_Avatar = AssetDatabase.LoadAssetAtPath ((target as ModelImporter).assetPath, typeof (Avatar)) as Avatar;
		}
		
		void LegacyGUI ()
		{
			EditorGUILayout.Popup(m_LegacyGenerateAnimations, styles.AnimationsOpt, styles.AnimLabel);
			// Show warning and fix button for deprecated import formats
			if (m_LegacyGenerateAnimations.intValue == 1 || m_LegacyGenerateAnimations.intValue == 2 || m_LegacyGenerateAnimations.intValue == 3)
				EditorGUILayout.HelpBox ("The animation import setting \""+styles.AnimationsOpt[m_LegacyGenerateAnimations.intValue].text+"\" is deprecated.", MessageType.Warning);
		}

        // Show copy avatar bool as a dropdown
        void AvatarSourceGUI()
        {
            EditorGUI.BeginChangeCheck();
            int copyValue = m_CopyAvatar.boolValue ? 1 : 0;
            EditorGUI.showMixedValue = m_CopyAvatar.hasMultipleDifferentValues;
            copyValue = EditorGUILayout.Popup(styles.AvatarDefinition, copyValue, styles.AvatarDefinitionOpt);
            EditorGUI.showMixedValue = false;
            if (EditorGUI.EndChangeCheck())
                m_CopyAvatar.boolValue = (copyValue == 1);
            
        }
		
		void GenericGUI ()
		{
            AvatarSourceGUI();

            if (!m_CopyAvatar.hasMultipleDifferentValues)
			{
			    if (!m_CopyAvatar.boolValue)
			    {
					// Do not allow multi edit of root node if all rigs doesn't match
			    	bool enabled = GUI.enabled;
					GUI.enabled = m_CanMultiEditTransformList;
			        EditorGUI.BeginChangeCheck();
			        rootIndex = EditorGUILayout.Popup(styles.RootNode, rootIndex, m_RootMotionBoneList);
			    	GUI.enabled = enabled;
			        if (EditorGUI.EndChangeCheck())
			        {
			            if (rootIndex > 0 && rootIndex < m_RootMotionBoneList.Length)
			            {
			                m_RootMotionBoneName.stringValue =
			                    FileUtil.GetLastPathNameComponent(m_RootMotionBoneList[rootIndex].text);
			            }
			            else
			            {
			                m_RootMotionBoneName.stringValue = "";
			            }
			        }
			    }
			    else
			        CopyAvatarGUI();
			}

		}

		void HumanoidGUI ()
		{
            AvatarSourceGUI();
			
			if (!m_CopyAvatar.hasMultipleDifferentValues)
			{
				if (!m_CopyAvatar.boolValue)
					ConfigureAvatarGUI ();
				else
					CopyAvatarGUI ();
			}
			
			EditorGUILayout.Space ();
		}
		
		void ConfigureAvatarGUI ()
		{
			if (targets.Length > 1)
			{
				GUILayout.Label ("Can't configure avatar in multi-editing mode", EditorStyles.helpBox);
				return;
			}

			if (singleImporter.transformPaths.Length <= HumanTrait.RequiredBoneCount)
			{
				GUILayout.Label("Not enough bones to create human avatar (requires " + HumanTrait.RequiredBoneCount + ")", EditorStyles.helpBox);
				return;
			}
			
			// Validation text
			GUIContent validationContent;
			if (m_Avatar && !HasModified ())
			{
				if (m_Avatar.isHuman)
					validationContent = styles.avatarValid;
				else
					validationContent = styles.avatarInvalid;
			}
			else
			{
				validationContent = styles.avatarPending;
				GUILayout.Label ("The avatar can be configured after settings have been applied.", EditorStyles.helpBox);
			}
			
			Rect r = EditorGUILayout.GetControlRect(); 
			const int buttonWidth = 75;
			GUI.Label (new Rect (r.xMax - buttonWidth - 18, r.y, 18, r.height), validationContent, EditorStyles.label);

			// Configure button
			EditorGUI.BeginDisabledGroup (m_Avatar == null);
			if (GUI.Button (new Rect (r.xMax-buttonWidth, r.y+1, buttonWidth, r.height-1), styles.configureAvatar, EditorStyles.miniButton))
			{
				if(!isLocked)
				{
					if (EditorApplication.SaveCurrentSceneIfUserWantsTo ())
					{
						Selection.activeObject = m_Avatar;
						AvatarEditor.s_EditImmediatelyOnNextOpen = true;
					}
				}
				else
					Debug.Log("Cannot configure avatar, inspector is locked");
			}
			EditorGUI.EndDisabledGroup ();
		}

        void CheckAvatar(Avatar sourceAvatar)
        {
            if (sourceAvatar != null)
            {
                if (sourceAvatar.isHuman && (animationType != ModelImporterAnimationType.Human))
                {
                    if (EditorUtility.DisplayDialog("Asigning an Humanoid Avatar on a Generic Rig",
                                                    "Do you want to change Animation Type to Humanoid ?", "Yes", "No"))
                    {
                        animationType = ModelImporterAnimationType.Human;
                    }
					else
					{
						m_AvatarSource.objectReferenceValue = null;
					}
                }
                else if (!sourceAvatar.isHuman && (animationType != ModelImporterAnimationType.Generic))
                {
                    if (EditorUtility.DisplayDialog("Asigning an Generic Avatar on a Humanoid Rig",
                                                    "Do you want to change Animation Type to Generic ?", "Yes", "No"))
                    {
                        animationType = ModelImporterAnimationType.Generic;
                    }
					else
					{
						m_AvatarSource.objectReferenceValue = null;
					}
                }
            }
        }
		
		void CopyAvatarGUI ()
		{
			GUILayout.Label (
@"If you have already created an Avatar for another model with a rig identical to this one, you can copy its Avatar definition.
With this option, this model will not create any avatar but only import animations.", EditorStyles.helpBox);
			
			EditorGUILayout.BeginHorizontal ();

			EditorGUI.BeginChangeCheck ();
			EditorGUILayout.PropertyField (m_AvatarSource, GUIContent.Temp ("Source"));
			var sourceAvatar = m_AvatarSource.objectReferenceValue as Avatar;
			if (EditorGUI.EndChangeCheck ())
			{

			    CheckAvatar(sourceAvatar);

				AvatarSetupTool.ClearAll (serializedObject);

				if (sourceAvatar != null)
					CopyHumanDescriptionFromOtherModel (sourceAvatar);

				m_AvatarCopyIsUpToDate = true;
			}

			if (sourceAvatar != null && !m_AvatarSource.hasMultipleDifferentValues && !m_AvatarCopyIsUpToDate)
			{
				if (GUILayout.Button (styles.UpdateMuscleDefinitionFromSource, EditorStyles.miniButton))
				{
					AvatarSetupTool.ClearAll (serializedObject);
					CopyHumanDescriptionFromOtherModel (sourceAvatar);
					m_AvatarCopyIsUpToDate = true;
				}
			}

			EditorGUILayout.EndHorizontal ();
		}

		void ShowUpdateReferenceClip()
		{
			if (targets.Length > 1 || animationType != ModelImporterAnimationType.Human || m_CopyAvatar.boolValue)
				return;

			string[] paths = new string[0];
			ModelImporter importer = target as ModelImporter;
			if (importer.referencedClips.Length > 0)
			{
				foreach (string clipGUID in importer.referencedClips)
					ArrayUtility.Add(ref paths, AssetDatabase.GUIDToAssetPath(clipGUID));
			}

			// Show only button if some clip reference this avatar.
			if (paths.Length > 0 && GUILayout.Button(styles.UpdateReferenceClips, GUILayout.Width(150)))
			{
				foreach (string path in paths)
					SetupReferencedClip(path);

				try
				{
					AssetDatabase.StartAssetEditing();
					foreach (string path in paths)
						AssetDatabase.ImportAsset(path);
				}
				finally
				{
					AssetDatabase.StopAssetEditing();
				}
			}
		}

		public override void OnInspectorGUI ()
		{
			if (styles == null)
				styles = new Styles ();
			
			// Animation type
            EditorGUI.BeginChangeCheck();                        
			EditorGUILayout.Popup(m_AnimationType, styles.AnimationTypeOpt, styles.AnimationType);
            if (EditorGUI.EndChangeCheck())
            {
				m_AvatarSource.objectReferenceValue = null;

				if (animationType == ModelImporterAnimationType.Legacy)
					m_AnimationCompression.intValue = (int)ModelImporterAnimationCompression.KeyframeReduction;
				else if (animationType == ModelImporterAnimationType.Generic || animationType == ModelImporterAnimationType.Human)
					m_AnimationCompression.intValue = (int)ModelImporterAnimationCompression.Optimal;
            }
			
			EditorGUILayout.Space ();
			
			if (!m_AnimationType.hasMultipleDifferentValues)
			{
				// Show GUI depending on animation type
				if (animationType == ModelImporterAnimationType.Human)
					HumanoidGUI ();
				else if (animationType == ModelImporterAnimationType.Generic)
					GenericGUI ();
				else if (animationType == ModelImporterAnimationType.Legacy)
					LegacyGUI ();
			}
			
			if (m_Avatar && m_Avatar.isValid && m_Avatar.isHuman)
				ShowUpdateReferenceClip();

			bool canOptimizeGameObjects = true;
			if (animationType != ModelImporterAnimationType.Human && animationType != ModelImporterAnimationType.Generic)
				canOptimizeGameObjects = false;
			if (m_CopyAvatar.boolValue == true)
				// If you have already created an Avatar for another model with a rig identical to this one, you can copy its Avatar definition.
				// With this option, this model will not create any avatar but only import animations.
				canOptimizeGameObjects = false;

            if (canOptimizeGameObjects)
		    {
		        EditorGUILayout.PropertyField(m_OptimizeGameObjects);
		        if (m_OptimizeGameObjects.boolValue && 
					serializedObject.targetObjects.Length == 1) // SerializedProperty can't handle multiple string arrays properly.
		        {
		            EditorGUILayout.Space();

					// Do not allow multi edit of exposed transform list if all rigs doesn't match
		        	bool enabled = GUI.enabled;
		        	GUI.enabled = m_CanMultiEditTransformList;
		            m_ExposeTransformEditor.OnGUI();
		        	GUI.enabled = enabled;
		        }
		    }

		    ApplyRevertGUI ();
		}


		static SerializedObject GetModelImporterSerializedObject (string assetPath)
		{
			ModelImporter importer = AssetImporter.GetAtPath(assetPath) as ModelImporter;
			if (importer == null)
				return null;
				
			return new SerializedObject(importer);	
		}


		static bool DoesHumanDescriptionMatch (ModelImporter importer, ModelImporter otherImporter)
		{
			SerializedObject so = new SerializedObject (new Object[] { importer, otherImporter });
			SerializedProperty prop = so.FindProperty ("m_HumanDescription");
			bool matches = !prop.hasMultipleDifferentValues;

			so.Dispose();
			
			return matches;
		}
		
		static void CopyHumanDescriptionToDestination (SerializedObject sourceObject, SerializedObject targetObject)
		{
			targetObject.CopyFromSerializedProperty(sourceObject.FindProperty("m_HumanDescription"));
		}

		private void CopyHumanDescriptionFromOtherModel (Avatar sourceAvatar)
		{
			string srcAssetPath = AssetDatabase.GetAssetPath(sourceAvatar);
			SerializedObject srcImporter = GetModelImporterSerializedObject(srcAssetPath);

			CopyHumanDescriptionToDestination(srcImporter, serializedObject);
			srcImporter.Dispose();
		}

		private void SetupReferencedClip(string otherModelImporterPath)
		{
			SerializedObject targetImporter = GetModelImporterSerializedObject(otherModelImporterPath);
			
			// We may receive a path that doesn't have a importer.
			if(targetImporter != null)
			{
			targetImporter.CopyFromSerializedProperty(serializedObject.FindProperty("m_AnimationType"));

			SerializedProperty copyAvatar = targetImporter.FindProperty("m_CopyAvatar");
			if (copyAvatar != null)
				copyAvatar.boolValue = true;

			SerializedProperty avatar = targetImporter.FindProperty("m_LastHumanDescriptionAvatarSource");
			if (avatar != null)
				avatar.objectReferenceValue = m_Avatar;
			
			CopyHumanDescriptionToDestination(serializedObject, targetImporter);
			targetImporter.ApplyModifiedProperties();
			targetImporter.Dispose();
		}
		}
		
		public bool isLocked
		{
			get
			{
				foreach (InspectorWindow i in InspectorWindow.GetAllInspectorWindows())
				{
					ActiveEditorTracker activeEditor = i.GetTracker();
					foreach (Editor e in activeEditor.activeEditors)
					{
						if (e == this)
						{
							return i.isLocked;
						}
					}
				}
				return false;
			}
		}
		
		private struct MappingRelevantSettings
		{
			public bool humanoid;
			public bool copyAvatar;
			public bool hasNoAnimation;
			public bool usesOwnAvatar { get { return humanoid && !copyAvatar; } }
	}	
		
		internal override void Apply ()
		{
			// Store the old mapping relevant settings for each model
			// Note that we need to do this *before* applying the pending modified properties.
			MappingRelevantSettings[] oldModelSettings = new MappingRelevantSettings[targets.Length];
			for (int i=0; i<targets.Length; i++)
			{
				// Find settings of individual model (doesn't include unapplied settings, so we get the "old" settings)
				SerializedObject so = new SerializedObject (targets[i]);
				SerializedProperty animationType = so.FindProperty ("m_AnimationType");
				SerializedProperty copyAvatar = so.FindProperty ("m_CopyAvatar");
				oldModelSettings[i].humanoid = animationType.intValue == (int)ModelImporterAnimationType.Human;
				oldModelSettings[i].hasNoAnimation = animationType.intValue == (int)ModelImporterAnimationType.None;
				oldModelSettings[i].copyAvatar = copyAvatar.boolValue;
}
			
			// Store the new mapping relevant settings for each model
			MappingRelevantSettings[] newModelSettings = new MappingRelevantSettings[targets.Length];
			Array.Copy (oldModelSettings, newModelSettings, targets.Length);
			for (int i=0; i<targets.Length; i++)
			{
				// If the settings have multiple values they can't have been changed, since that causes them to have the same value.
				// So only copy value from SerializedProperty if it does not have multiple values.
				if (!m_AnimationType.hasMultipleDifferentValues)
					newModelSettings[i].humanoid = m_AnimationType.intValue == (int)ModelImporterAnimationType.Human;
				if (!m_CopyAvatar.hasMultipleDifferentValues)
					newModelSettings[i].copyAvatar = m_CopyAvatar.boolValue;
			}
			
			// Now that we have stored the old and new settings, we can apply all modified properties.
			serializedObject.ApplyModifiedProperties ();
			
			// But we might not be done yet!
			// For all models which did not have own humanoid before but should have it now,
			// we need to perform auto-mapping. (For the opposite case we also need to clear the mapping.)
			// Iterate through all the models...
			for (int i=0; i<targets.Length; i++)
			{
				// If this model had its own humanoid avatar before but shouldn't have it now...
				if (oldModelSettings[i].usesOwnAvatar && !newModelSettings[i].usesOwnAvatar)
				{
					// ...then clear auto-setup on this model.
					SerializedObject so = new SerializedObject (targets[i]);
					AvatarSetupTool.ClearAll (so);
					so.ApplyModifiedProperties ();
				}
				
				// If this model should have its own humanoid avatar before and didn't have it before,
				// then we need to perform auto-mapping.
				if (!oldModelSettings[i].usesOwnAvatar && newModelSettings[i].usesOwnAvatar)
				{
					ModelImporter importer = targets[i] as ModelImporter;
					// Special case if the model didn't have animation before...
					if (oldModelSettings[i].hasNoAnimation)
					{
						// We have to do an extra import first, before the automapping works.
						// Because the model doesn't have any skinned meshes when it was last imported with
						// Animation Mode: None. And the auro-mapping relies on information in the skinned meshes.
						AssetDatabase.ImportAsset (importer.assetPath);
					}
					
					// Perform auto-setup on this model.
					SerializedObject so = new SerializedObject (targets[i]);
					GameObject go = AssetDatabase.LoadMainAssetAtPath (importer.assetPath) as GameObject;
					// The character could be optimized right now
					// 'm_OptimizeGameObjects' can't be used to tell if it is optimized, because the user can change this value from UI,
					// and the change hasn't been applied yet.
					Animator animator = go.GetComponent<Animator>();
					bool noTransformHierarchy = animator && !animator.hasTransformHierarchy;
					if (noTransformHierarchy)
					{
						go = Instantiate(go) as GameObject;
						GameObjectUtility.DeoptimizeTransformHierarchy(go);
					}
					AvatarSetupTool.AutoSetupOnInstance(go, so);
					if (noTransformHierarchy)
						DestroyImmediate(go);

					so.ApplyModifiedProperties ();
				}
			}
		}
	}	
}
