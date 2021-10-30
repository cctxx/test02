using System;
using System.IO;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using Object = UnityEngine.Object;

namespace UnityEditor
{
	internal class ModelImporterClipEditor : AssetImporterInspector
	{
		AnimationClipEditor m_AnimationClipEditor;
		ModelImporter singleImporter { get { return targets[0] as ModelImporter; } }
		
		public int m_SelectedClipIndexDoNotUseDirectly = -1;
		public int selectedClipIndex
		{
			get { return m_SelectedClipIndexDoNotUseDirectly; }
			set
			{
				m_SelectedClipIndexDoNotUseDirectly = value;
				if (m_ClipList != null)
					m_ClipList.index = value;
			}
		}
		
		SerializedObject m_DefaultClipsSerializedObject = null;
		
		SerializedProperty m_AnimationType;
	
		SerializedProperty m_ImportAnimation;
		SerializedProperty m_ClipAnimations;
		SerializedProperty m_BakeSimulation;
		SerializedProperty m_AnimationCompression;
		SerializedProperty m_AnimationRotationError;
		SerializedProperty m_AnimationPositionError;
		SerializedProperty m_AnimationScaleError;
		SerializedProperty m_AnimationWrapMode;
		SerializedProperty m_LegacyGenerateAnimations;
				
		ReorderableList m_ClipList;
	
		private ModelImporterAnimationType animationType
		{
			get { return (ModelImporterAnimationType)m_AnimationType.intValue; }
			set { m_AnimationType.intValue = (int)value; } 
		}

		private ModelImporterGenerateAnimations legacyGenerateAnimations
		{
			get { return (ModelImporterGenerateAnimations)m_LegacyGenerateAnimations.intValue; }
			set { m_LegacyGenerateAnimations.intValue = (int)value; } 
		}

		private class Styles
		{
			public GUIContent ImportAnimations = EditorGUIUtility.TextContent("ModelImporterImportAnimations");
			
			public GUIStyle numberStyle = new GUIStyle (EditorStyles.label);

			public GUIContent AnimWrapModeLabel = EditorGUIUtility.TextContent("ModelImporterAnimWrapMode");

			public GUIContent[] AnimWrapModeOpt =
			{
				EditorGUIUtility.TextContent("ModelImporterAnimWrapModeDefault"),
				EditorGUIUtility.TextContent("ModelImporterAnimWrapModeOnce"),
				EditorGUIUtility.TextContent("ModelImporterAnimWrapModeLoop"),
				EditorGUIUtility.TextContent("ModelImporterAnimWrapModePingPong"),
				EditorGUIUtility.TextContent("ModelImporterAnimWrapModeClampForever")
			};

			public GUIContent BakeIK = EditorGUIUtility.TextContent ("ModelImporterAnimBakeIK");
			public GUIContent AnimCompressionLabel = EditorGUIUtility.TextContent("ModelImporterAnimComprSetting");
			public GUIContent[] AnimCompressionOptLegacy =
			{
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingOff"),
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingReduction"),
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingReductionAndCompression")
			};
			public GUIContent[] AnimCompressionOpt =
			{
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingOff"),
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingReduction"),
				EditorGUIUtility.TextContent("ModelImporterAnimComprSettingOptimal")
			};

			public GUIContent AnimRotationErrorLabel = EditorGUIUtility.TextContent("ModelImporterAnimComprRotationError");
			public GUIContent AnimPositionErrorLabel = EditorGUIUtility.TextContent("ModelImporterAnimComprPositionError");
			public GUIContent AnimScaleErrorLabel = EditorGUIUtility.TextContent("ModelImporterAnimComprScaleError");
			public GUIContent AnimationCompressionHelp = EditorGUIUtility.TextContent("ModelImporterAnimComprHelp");
			public GUIContent clipMultiEditInfo = new GUIContent ("Multi-object editing of clips not supported.");

			public GUIContent updateMuscleDefinitionFromSource = EditorGUIUtility.TextContent("ModelImporterRigUpdateMuscleDefinitionFromSource");
			
			public Styles ()
			{
				numberStyle.alignment = TextAnchor.UpperRight;
			}
		}
		static Styles styles;
		
		public void OnEnable ()
		{
			m_ClipAnimations = serializedObject.FindProperty ("m_ClipAnimations");

			m_AnimationType = serializedObject.FindProperty("m_AnimationType");
			m_LegacyGenerateAnimations = serializedObject.FindProperty ("m_LegacyGenerateAnimations");

			// Animation
			m_ImportAnimation = serializedObject.FindProperty ("m_ImportAnimation");
			m_BakeSimulation = serializedObject.FindProperty ("m_BakeSimulation");
			m_AnimationCompression = serializedObject.FindProperty ("m_AnimationCompression");
			m_AnimationRotationError = serializedObject.FindProperty ("m_AnimationRotationError");
			m_AnimationPositionError = serializedObject.FindProperty ("m_AnimationPositionError");
			m_AnimationScaleError = serializedObject.FindProperty ("m_AnimationScaleError");
			m_AnimationWrapMode = serializedObject.FindProperty ("m_AnimationWrapMode");
			
			// Find all serialized property before calling SetupDefaultClips
			if (m_ClipAnimations.arraySize == 0)
				SetupDefaultClips ();

			ValidateClipSelectionIndex ();

			if (m_AnimationClipEditor != null && selectedClipIndex >= 0)
				SyncClipEditor ();
			
			// Automatically select the first clip
			if (selectedClipIndex == -1 && m_ClipAnimations.arraySize != 0)
				SelectClip (0);
		}
		
		void SyncClipEditor ()
		{
			if (m_AnimationClipEditor == null)
				return;
			m_AnimationClipEditor.ShowRange (GetAnimationClipInfoAtIndex (selectedClipIndex));
			m_AnimationClipEditor.referenceTransformPaths = singleImporter.transformPaths;
		}

		private void SetupDefaultClips ()
		{
			// Create dummy SerializedObject where we can add a clip for each
			// take without making any properties show up as changed.
			m_DefaultClipsSerializedObject = new SerializedObject (target);
			m_ClipAnimations = m_DefaultClipsSerializedObject.FindProperty ("m_ClipAnimations");
			m_AnimationType = m_DefaultClipsSerializedObject.FindProperty("m_AnimationType");
			m_ClipAnimations.arraySize = 0;
			
			foreach (TakeInfo takeInfo in singleImporter.importedTakeInfos)
			{
				AddClip (takeInfo);
			}
		}
		
		// When switching to explicitly defined clips, we must fix up the recycleID's to not lose AnimationClip references.
		// When m_ClipAnimations is defined, the clips are identified by the clipName
		// When m_ClipAnimations is not defined, the clips are identified by the takeName
		void PatchDefaultClipTakeNamesToSplitClipNames ()
		{
			foreach (TakeInfo takeInfo in singleImporter.importedTakeInfos)
			{
				PatchImportSettingRecycleID.Patch (serializedObject, 74, takeInfo.name, takeInfo.defaultClipName);
			}
		}
		
		// A dummy SerializedObject is created when there are no explicitly defined clips.
		// When the user modifies any settings these clips must be transferred to the model importer.
		private void TransferDefaultClipsToCustomClips ()
		{
			if (m_DefaultClipsSerializedObject == null)
				return;

			bool wasEmpty = serializedObject.FindProperty ("m_ClipAnimations").arraySize == 0;
			if (!wasEmpty)
				Debug.LogError("Transferring default clips failed, target already has clips");
		
			// Transfer data to main SerializedObject
			serializedObject.CopyFromSerializedProperty (m_ClipAnimations);
			m_ClipAnimations = serializedObject.FindProperty ("m_ClipAnimations");
			
			m_DefaultClipsSerializedObject = null;
			
			PatchDefaultClipTakeNamesToSplitClipNames ();
			
			SyncClipEditor ();
		}
		
		private void ValidateClipSelectionIndex ()
		{
			// selected clip index can be invalid if array was changed and then reverted.
			if (selectedClipIndex > m_ClipAnimations.arraySize - 1)
			{
				selectedClipIndex = -1;
		}
		}
		
		public void OnDestroy ()
		{
			Object.DestroyImmediate(m_AnimationClipEditor);
		}
		
		internal override void ResetValues ()
		{
			base.ResetValues ();
			m_ClipAnimations = serializedObject.FindProperty ("m_ClipAnimations");
			m_AnimationType = serializedObject.FindProperty("m_AnimationType");
			m_DefaultClipsSerializedObject = null;
			if (m_ClipAnimations.arraySize == 0)
				SetupDefaultClips ();

			ValidateClipSelectionIndex ();
			UpdateList ();
			SelectClip (selectedClipIndex);
		}
		
		void AnimationClipGUI ()
		{
			// Show general animation import settings
			AnimationSettings ();
			
			Profiler.BeginSample ("Clip inspector");
			
			EditorGUILayout.Space ();
			
			// Show list of animations and inspector for individual animation
			if (targets.Length == 1)
				AnimationSplitTable ();
			else
				GUILayout.Label (styles.clipMultiEditInfo, EditorStyles.helpBox);
			
			Profiler.EndSample ();
		}
		
		public override void OnInspectorGUI ()
		{
			if (styles == null)
				styles = new Styles ();

			EditorGUI.BeginDisabledGroup (singleImporter.animationType == ModelImporterAnimationType.None);

			EditorGUILayout.PropertyField(m_ImportAnimation, styles.ImportAnimations);
			
			if (m_ImportAnimation.boolValue && !m_ImportAnimation.hasMultipleDifferentValues)
			{
				bool HasNoValidAnimationData = targets.Length == 1 && singleImporter.importedTakeInfos.Length == 0;
				
				if (IsDeprecatedMultiAnimationRootImport())
					EditorGUILayout.HelpBox ("Animation data was imported using a deprecated Generation option in the Rig tab. Please switch to a non-deprecated import mode in the Rig tab to be able to edit the animation import settings.", MessageType.Info);
				else if (HasNoValidAnimationData)
				{
					if (serializedObject.hasModifiedProperties)
						EditorGUILayout.HelpBox ("The animations settings can be edited after clicking Apply.", MessageType.Info);
					else
						EditorGUILayout.HelpBox ("No animation data available in this model.", MessageType.Info);
				}
				else if (m_AnimationType.hasMultipleDifferentValues)
					EditorGUILayout.HelpBox ("The rigs of the selected models have different animation types.", MessageType.Info);
				else if (animationType == ModelImporterAnimationType.None)
					EditorGUILayout.HelpBox ("The rigs is not setup to handle animation. Edit the settings in the Rig tab.", MessageType.Info);
				else
				{
					if (m_ImportAnimation.boolValue && !m_ImportAnimation.hasMultipleDifferentValues)
						AnimationClipGUI ();
				}
			}

			EditorGUI.EndDisabledGroup();
			
			ApplyRevertGUI ();
		}
		
		void AnimationSettings ()
		{
			EditorGUILayout.Space();

			// Bake IK
			bool isBakeIKSupported = true;
			foreach (ModelImporter importer in targets)
				if (!importer.isBakeIKSupported)
					isBakeIKSupported = false;
			EditorGUI.BeginDisabledGroup (!isBakeIKSupported);
			EditorGUILayout.PropertyField (m_BakeSimulation, styles.BakeIK);
			EditorGUI.EndDisabledGroup ();
			
			// Wrap mode
			if (animationType == ModelImporterAnimationType.Legacy)
			{
				EditorGUI.showMixedValue = m_AnimationWrapMode.hasMultipleDifferentValues;
				EditorGUILayout.Popup(m_AnimationWrapMode, styles.AnimWrapModeOpt, styles.AnimWrapModeLabel);
				EditorGUI.showMixedValue = false;

				// Compression
				int[] kCompressionValues = { (int)ModelImporterAnimationCompression.Off, (int)ModelImporterAnimationCompression.KeyframeReduction, (int)ModelImporterAnimationCompression.KeyframeReductionAndCompression };
				EditorGUILayout.IntPopup(m_AnimationCompression, styles.AnimCompressionOptLegacy, kCompressionValues, styles.AnimCompressionLabel);
			}
			else
			{
				// Compression
				int[] kCompressionValues = { (int)ModelImporterAnimationCompression.Off, (int)ModelImporterAnimationCompression.KeyframeReduction, (int)ModelImporterAnimationCompression.Optimal };
				EditorGUILayout.IntPopup(m_AnimationCompression, styles.AnimCompressionOpt, kCompressionValues, styles.AnimCompressionLabel);
			}

			if (m_AnimationCompression.intValue > (int)ModelImporterAnimationCompression.Off)
			{
				// keyframe reduction settings
				EditorGUILayout.PropertyField(m_AnimationRotationError, styles.AnimRotationErrorLabel);
				EditorGUILayout.PropertyField(m_AnimationPositionError, styles.AnimPositionErrorLabel);
				EditorGUILayout.PropertyField(m_AnimationScaleError, styles.AnimScaleErrorLabel);
				GUILayout.Label(styles.AnimationCompressionHelp, EditorStyles.helpBox);
			}
		}
		
		void SelectClip (int selected)
		{
			// If you were editing Clip Name (delayed text field had focus) and then selected a new clip from the clip list,
			// the active string in the delayed text field would get applied to the new selected clip instead of the old.
			// HACK: Calling EndGUI here on the recycled delayed text editor seems to fix this issue.
			// Sometime we should reimplement delayed text field code to not be super confusing and then fix the issue more properly.
			if (EditorGUI.s_DelayedTextEditor != null && Event.current != null)
				EditorGUI.s_DelayedTextEditor.EndGUI (Event.current.type);
			
			Object.DestroyImmediate(m_AnimationClipEditor);
			m_AnimationClipEditor = null;

			selectedClipIndex = selected;
			if (selectedClipIndex < 0 || selectedClipIndex >= m_ClipAnimations.arraySize)
			{
				selectedClipIndex = -1;
				return;
			}
			
			AnimationClipInfoProperties info = GetAnimationClipInfoAtIndex (selected);
			AnimationClip clip = singleImporter.GetPreviewAnimationClipForTake(info.takeName);
			if (clip != null)
			{
				m_AnimationClipEditor = (AnimationClipEditor)Editor.CreateEditor (clip);
				SyncClipEditor ();
			}
		}
		
		void UpdateList ()
		{
			if (m_ClipList == null)
				return;
			List<AnimationClipInfoProperties> clipInfos = new List<AnimationClipInfoProperties> ();
				for (int i=0; i < m_ClipAnimations.arraySize; i++)
					clipInfos.Add (GetAnimationClipInfoAtIndex (i));
			m_ClipList.list = clipInfos;
		}
		
		void AddClipInList (ReorderableList list)
		{
			if (m_DefaultClipsSerializedObject != null)
				TransferDefaultClipsToCustomClips ();
			
			AddClip (singleImporter.importedTakeInfos[0]);
			UpdateList ();
			SelectClip (list.list.Count - 1);
		}
		
		void RemoveClipInList (ReorderableList list)
		{
			TransferDefaultClipsToCustomClips ();
			
			RemoveClip (list.index);
			UpdateList ();
			SelectClip (Mathf.Min (list.index, list.Count - 1));
		}
		
		void SelectClipInList (ReorderableList list)
		{
			SelectClip (list.index);
		}
		
		const int kFrameColumnWidth = 45;
		
		private void DrawClipElement (Rect rect, int index, bool selected, bool focused)
		{
			AnimationClipInfoProperties info = GetAnimationClipInfoAtIndex (index);
			rect.xMax -= kFrameColumnWidth * 2;
			GUI.Label (rect, info.name, EditorStyles.label);
			rect.x = rect.xMax;
			rect.width = kFrameColumnWidth;
			GUI.Label (rect, info.firstFrame.ToString ("0.0"), styles.numberStyle);
			rect.x = rect.xMax;
			GUI.Label (rect, info.lastFrame.ToString ("0.0"), styles.numberStyle);
		}
		
		private void DrawClipHeader (Rect rect)
		{
			rect.xMax -= kFrameColumnWidth * 2;
			GUI.Label (rect, "Clips", EditorStyles.label);
			rect.x = rect.xMax;
			rect.width = kFrameColumnWidth;
			GUI.Label (rect, "Start", styles.numberStyle);
			rect.x = rect.xMax;
			GUI.Label (rect, "End", styles.numberStyle);
		}
		
		void AnimationSplitTable ()
		{
				
			if (m_ClipList == null)
			{
				m_ClipList = new ReorderableList (new List<AnimationClipInfoProperties> (), typeof (string), false, true, true, true);
				m_ClipList.onAddCallback = AddClipInList;
				m_ClipList.onSelectCallback = SelectClipInList;
				m_ClipList.onRemoveCallback = RemoveClipInList;
				m_ClipList.drawElementCallback = DrawClipElement;
				m_ClipList.drawHeaderCallback = DrawClipHeader;
				m_ClipList.elementHeight = 16;
				UpdateList ();
				m_ClipList.index = selectedClipIndex;
			}
			m_ClipList.DoList ();
			
			EditorGUI.BeginChangeCheck ();
			
			// Show selected clip info
			{
				AnimationClipInfoProperties clip = GetSelectedClipInfo ();
				if (clip == null)
					return;
				
				if (m_AnimationClipEditor != null && selectedClipIndex != -1)
				{
					GUILayout.Space (5);
					
					AnimationClip actualClip = m_AnimationClipEditor.target as AnimationClip;

					if(actualClip.isAnimatorMotion)
						GetSelectedClipInfo().AssignToPreviewClip(actualClip);
					
					TakeInfo[] importedTakeInfos = singleImporter.importedTakeInfos;
					string[] takeNames = new string[importedTakeInfos.Length];
					for (int i = 0; i < importedTakeInfos.Length; i++)
						takeNames[i] = importedTakeInfos[i].name;
					
					EditorGUI.BeginChangeCheck ();
					string currentName = clip.name;
					int takeIndex = ArrayUtility.IndexOf (takeNames, clip.takeName);
					m_AnimationClipEditor.takeNames = takeNames;
					m_AnimationClipEditor.takeIndex = ArrayUtility.IndexOf (takeNames, clip.takeName);
					m_AnimationClipEditor.DrawHeader ();
					
					if (EditorGUI.EndChangeCheck ())
					{
						// We renamed the clip name, try to maintain the localIdentifierInFile so we don't lose any data.
						if (clip.name != currentName)
						{
							TransferDefaultClipsToCustomClips ();
							PatchImportSettingRecycleID.Patch (serializedObject, 74, currentName, clip.name);
						}
						
						int newTakeIndex = m_AnimationClipEditor.takeIndex;
						if (newTakeIndex != -1 && newTakeIndex != takeIndex)
						{
							clip.name = MakeUniqueClipName(takeNames[newTakeIndex], -1);
							SetupTakeNameAndFrames(clip, importedTakeInfos[newTakeIndex]);
							GUIUtility.keyboardControl = 0;
							SelectClip (selectedClipIndex);

							// actualClip has been changed by SelectClip
							actualClip = m_AnimationClipEditor.target as AnimationClip;
						}
					}
					
					m_AnimationClipEditor.OnInspectorGUI ();

					if (actualClip.isAnimatorMotion)
						GetSelectedClipInfo ().ExtractFromPreviewClip (actualClip);
				}
			}
			
			if (EditorGUI.EndChangeCheck ())
			{
				TransferDefaultClipsToCustomClips ();
			}
		}
		
		public override bool HasPreviewGUI ()
		{
			return m_AnimationClipEditor != null && m_AnimationClipEditor.HasPreviewGUI ();
		}
		
		public override void OnPreviewSettings ()
		{
			if (m_AnimationClipEditor != null)
				m_AnimationClipEditor.OnPreviewSettings ();
			
		}
		
		bool IsDeprecatedMultiAnimationRootImport ()
		{
			if (animationType == ModelImporterAnimationType.Legacy)
				return  legacyGenerateAnimations == ModelImporterGenerateAnimations.InOriginalRoots || legacyGenerateAnimations == ModelImporterGenerateAnimations.InNodes;
			else
				return false;
		}
		
		public override void OnInteractivePreviewGUI (Rect r, GUIStyle background)
		{
			if (m_AnimationClipEditor)
				m_AnimationClipEditor.OnInteractivePreviewGUI (r, background);
		}
		
		AnimationClipInfoProperties GetAnimationClipInfoAtIndex (int index)
		{
			return new AnimationClipInfoProperties(m_ClipAnimations.GetArrayElementAtIndex (index)); 
		}
		
		AnimationClipInfoProperties GetSelectedClipInfo()
		{
			if (selectedClipIndex >= 0 && selectedClipIndex < m_ClipAnimations.arraySize)
				return GetAnimationClipInfoAtIndex (selectedClipIndex);
			else
				return null;
		}

		string MakeUniqueClipName (string name, int row)
		{
			string newName = name;
		
			// Find a unique name
			int attempt = 0;
			while (true)
			{
				int i=0;
				for (i=0;i<m_ClipAnimations.arraySize;i++)
				{
					AnimationClipInfoProperties clip = GetAnimationClipInfoAtIndex(i);
					if (newName == clip.name && row != i)
					{
						newName = name + attempt.ToString();
						attempt++;
						break;
					}
				}
				if (i == m_ClipAnimations.arraySize)
					break;
			}
			
			return newName;
		}
	
		void RemoveClip (int index)
		{
			m_ClipAnimations.DeleteArrayElementAtIndex(index);
			if (m_ClipAnimations.arraySize == 0)
			{
				SetupDefaultClips ();
				m_ImportAnimation.boolValue = false;
			}
		}
	
		void SetupTakeNameAndFrames (AnimationClipInfoProperties info, TakeInfo takeInfo)
		{
			info.takeName = takeInfo.name;
			info.firstFrame = (int)Mathf.Round(takeInfo.bakeStartTime * takeInfo.sampleRate);
			info.lastFrame = (int)Mathf.Round(takeInfo.bakeStopTime * takeInfo.sampleRate);
		}
		
		void AddClip (TakeInfo takeInfo)
		{
			m_ClipAnimations.InsertArrayElementAtIndex(m_ClipAnimations.arraySize);
			AnimationClipInfoProperties info = GetAnimationClipInfoAtIndex (m_ClipAnimations.arraySize - 1); 
			
			info.name = MakeUniqueClipName(takeInfo.defaultClipName, -1);
			SetupTakeNameAndFrames(info, takeInfo);
			info.wrapMode = (int)WrapMode.Default;
			info.loop = false;
			info.orientationOffsetY = 0;
			info.level = 0;
			info.cycleOffset = 0;
            info.loopTime = false;
            info.loopBlend = false;
			info.loopBlendOrientation = false;
			info.loopBlendPositionY = false;
			info.loopBlendPositionXZ = false;
			info.keepOriginalOrientation = false;
			info.keepOriginalPositionY = true;
			info.keepOriginalPositionXZ = false;
			info.heightFromFeet = false;
			info.mirror = false;
			info.maskType = ClipAnimationMaskType.CreateFromThisModel ;

			AvatarMask defaultMask = new AvatarMask();

			string[] humanTransforms = null;
			SerializedObject so = info.maskTypeProperty.serializedObject;
			ModelImporter modelImporter = so.targetObject as ModelImporter;
			if (animationType == ModelImporterAnimationType.Human)
			{
				humanTransforms = AvatarMaskUtility.GetAvatarHumanTransform(so, modelImporter.transformPaths);
				if (humanTransforms == null)
					return;
			}

			AvatarMaskUtility.UpdateTransformMask(defaultMask, modelImporter.transformPaths, humanTransforms);

			info.MaskToClip(defaultMask);

		    info.ClearEvents();
            info.ClearCurves();

			Object.DestroyImmediate(defaultMask);
		}
	}	
}
