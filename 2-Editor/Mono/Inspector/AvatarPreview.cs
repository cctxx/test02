using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Reflection;
using Object = UnityEngine.Object;

namespace UnityEditor
{
	internal class AvatarPreview 
	{
		const string kIkPref = "AvatarpreviewShowIK";
		const string kReferencePref = "AvatarpreviewShowReference";
		const string kSpeedPref = "AvatarpreviewSpeed";
		const float kTimeControlRectHeight = 21;

		public delegate void OnAvatarChange();
		OnAvatarChange m_OnAvatarChangeFunc = null;
		
		public OnAvatarChange OnAvatarChangeFunc
		{
			set { m_OnAvatarChangeFunc = value; }
		}

		public bool IKOnFeet
		{
			get { return m_IKOnFeet; }
		}

		public bool ShowIKOnFeetButton
		{
			get { return m_ShowIKOnFeetButton; }
			set { m_ShowIKOnFeetButton = value; }
		}

		public Animator Animator
		{
			get { return m_PreviewInstance != null ? m_PreviewInstance.GetComponent(typeof(Animator)) as Animator : null; }
		}

		public GameObject PreviewObject
		{
			get { return m_PreviewInstance; }			
		}

		public ModelImporterAnimationType animationClipType
		{
			get { return GetAnimationType (m_SourcePreviewMotion); }			
		}


		public TimeControl timeControl;
		
		// 60 is the default framerate for animations created inside Unity so as good a default as any.
		public int fps = 60;

		private Material     m_FloorMaterial;
		private Material     m_FloorMaterialSmall;
		private Material     m_ShadowMaskMaterial;
		private Material     m_ShadowPlaneMaterial;

		PreviewRenderUtility        m_PreviewUtility;
		GameObject                  m_PreviewInstance;
		GameObject                  m_ReferenceInstance;
		GameObject                  m_DirectionInstance;
		GameObject                  m_PivotInstance;
		GameObject                  m_RootInstance;
		float	                    m_BoundingVolumeScale;
		Motion                      m_SourcePreviewMotion;
		Animator                    m_SourceScenePreviewAnimator;

		private const int           kPreviewLayer = 31;

		const string                s_PreviewStr = "Preview";
		int                         m_PreviewHint = s_PreviewStr.GetHashCode();

		Texture2D                   m_FloorTexture;
		Mesh	                    m_FloorPlane;

		bool						m_ShowReference = false;
		bool						m_IKOnFeet = false;

		bool                        m_ShowIKOnFeetButton = true;
		bool                        m_IsValid;
		int                         m_ModelSelectorId = EditorGUIUtility.GetPermanentControlID ();
		
		
		private const float kFloorFadeDuration = 0.2f;
		private const float kFloorScale = 5;
		private const float kFloorScaleSmall = 0.2f;
		private const float kFloorTextureScale = 4;
		private const float kFloorAlpha = 0.5f;
		private const float kFloorShadowAlpha = 0.3f;
		
		private float m_PrevFloorHeight = 0;
		private float m_NextFloorHeight = 0;
		
		private Vector2 m_PreviewDir = new Vector2 (120, -20);


		private class Styles
		{
			public GUIContent speedScale = EditorGUIUtility.IconContent ("SpeedScale", "Changes animation preview speed");
			public GUIContent pivot = EditorGUIUtility.IconContent ("AvatarPivot", "Displays avatar's pivot and mass center");
			public GUIContent ik = new GUIContent("IK", "Activates feet IK preview");
			public GUIContent avatarIcon = EditorGUIUtility.IconContent ("Avatar Icon", "Changes the model to use for previewing.");
		
			public GUIStyle preButton = "preButton";
			public GUIStyle preSlider = "preSlider";
			public GUIStyle preSliderThumb = "preSliderThumb";
			public GUIStyle preLabel = "preLabel";
		}
		private static Styles s_Styles;

		void SetPreviewCharacterEnabled(bool enabled, bool showReference)
		{
			if (m_PreviewInstance != null)
				GameObjectInspector.SetEnabledRecursive(m_PreviewInstance, enabled);
			GameObjectInspector.SetEnabledRecursive(m_ReferenceInstance, showReference && enabled);
			GameObjectInspector.SetEnabledRecursive(m_DirectionInstance, showReference && enabled);
			GameObjectInspector.SetEnabledRecursive(m_PivotInstance, showReference && enabled);
			GameObjectInspector.SetEnabledRecursive(m_RootInstance, showReference && enabled);
		}

		static AnimationClip GetFirstAnimationClipFromMotion (Motion motion)
		{
			AnimationClip clip = motion as AnimationClip;
			if (clip)
				return clip;

			BlendTree blendTree = motion as BlendTree;
			if (blendTree)
			{
				AnimationClip[] clips = blendTree.GetAnimationClipsFlattened();
				if (clips.Length > 0)
					return  clips[0];
			}
			
			return null;
		}

		static public ModelImporterAnimationType GetAnimationType (GameObject go)
		{
			Animator animator = go.GetComponent<Animator> ();
			if (animator)
			{
                Avatar avatar = animator != null ? animator.avatar : null;
                if (avatar && avatar.isHuman)
					return ModelImporterAnimationType.Human;
				else
					return ModelImporterAnimationType.Generic;
			}
			else if (go.GetComponent<Animation> () != null)
			{
				return ModelImporterAnimationType.Legacy;
			}
			else
				return ModelImporterAnimationType.None;
		}

		static public ModelImporterAnimationType GetAnimationType (Motion motion)
		{
			AnimationClip clip = GetFirstAnimationClipFromMotion (motion);
			if (clip)
				return AnimationUtility.GetAnimationType(clip);
			else
				return ModelImporterAnimationType.None;
		}
		
		static public bool IsValidPreviewGameObject (GameObject target, ModelImporterAnimationType requiredClipType)
		{
			return target != null && GameObjectInspector.HasRenderablePartsRecurse(target) && GetAnimationType(target) == requiredClipType;
		}

		static public GameObject FindBestFittingRenderableGameObjectFromModelAsset (Object asset, ModelImporterAnimationType animationType)
		{
			if(asset == null)
				return null;
			
			ModelImporter importer = AssetImporter.GetAtPath(AssetDatabase.GetAssetPath(asset)) as ModelImporter;
			if (importer == null)
				return null;
			
			string assetPath = importer.CalculateBestFittingPreviewGameObject ();
			GameObject tempGO = AssetDatabase.LoadMainAssetAtPath(assetPath) as GameObject;

			// We should also check for isHumanClip matching the animationclip requiremenets...
			if (IsValidPreviewGameObject(tempGO, animationType))
				return tempGO;
			else
				return null;
		}
		
		static GameObject CalculatePreviewGameObject (Animator selectedAnimator, Motion motion, ModelImporterAnimationType animationType)
		{
			AnimationClip sourceClip = GetFirstAnimationClipFromMotion (motion);
			
			// Use selected preview
			GameObject selected = AvatarPreviewSelection.GetPreview(animationType);
			if (IsValidPreviewGameObject (selected, animationType))
				return selected;

			if (selectedAnimator != null && IsValidPreviewGameObject(selectedAnimator.gameObject, animationType))
				return selectedAnimator.gameObject;

			// Find the best fitting preview game object for the asset we are viewing (Handles @ convention, will pick base path for you)
			selected = FindBestFittingRenderableGameObjectFromModelAsset (sourceClip, animationType);
			if (selected != null)
				return selected;

			if (animationType == ModelImporterAnimationType.Human)
				return GetHumanoidFallback ();
			else if (animationType == ModelImporterAnimationType.Generic)
				return GetGenericAnimationFallback ();
				
			return null;
		}


		static GameObject GetGenericAnimationFallback ()
		{
			return (GameObject)EditorGUIUtility.Load("Avatar/DefaultGeneric.fbx");
		}
		
		static GameObject GetHumanoidFallback ()
		{
			return (GameObject)EditorGUIUtility.Load("Avatar/DefaultAvatar.fbx");
		}
		
		void InitInstance(Animator scenePreviewObject, Motion motion)
		{
			m_SourcePreviewMotion = motion;
			m_SourceScenePreviewAnimator = scenePreviewObject;
			
			if (m_PreviewInstance == null)
			{
				GameObject go = CalculatePreviewGameObject(scenePreviewObject, motion, animationClipType);
				m_IsValid = go != null && go != GetGenericAnimationFallback ();

				if (go != null)
				{
					m_PreviewInstance = (GameObject)EditorUtility.InstantiateRemoveAllNonAnimationComponents(go, Vector3.zero, Quaternion.identity);

					Bounds bounds = new Bounds (m_PreviewInstance.transform.position, Vector3.zero);
					GameObjectInspector.GetRenderableBoundsRecurse (ref bounds, m_PreviewInstance);

					m_BoundingVolumeScale = Mathf.Max(bounds.size.x, Mathf.Max(bounds.size.y, bounds.size.z));

					GameObjectInspector.InitInstantiatedPreviewRecursive(m_PreviewInstance);
					if (Animator)
					{
						Animator.enabled = false;
						Animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;
						Animator.applyRootMotion = true;
						Animator.logWarnings = false;
                        Animator.fireEvents = false;
					}
				}
			}
			
			if (timeControl == null)
			{
				timeControl = new TimeControl();
			}

			if (m_ReferenceInstance == null)
			{
				GameObject referenceGO = (GameObject)EditorGUIUtility.Load("Avatar/dial_flat.prefab");
				m_ReferenceInstance = (GameObject)Object.Instantiate(referenceGO, Vector3.zero, Quaternion.identity);
				GameObjectInspector.InitInstantiatedPreviewRecursive(m_ReferenceInstance);
			}

			if (m_DirectionInstance == null)
			{
				GameObject directionGO = (GameObject)EditorGUIUtility.Load("Avatar/arrow.fbx");
				m_DirectionInstance = (GameObject)Object.Instantiate(directionGO, Vector3.zero, Quaternion.identity);
				GameObjectInspector.InitInstantiatedPreviewRecursive(m_DirectionInstance);
			}

			if (m_PivotInstance == null)
			{
				GameObject pivotGO = (GameObject)EditorGUIUtility.Load("Avatar/root.fbx");
				m_PivotInstance = (GameObject)Object.Instantiate(pivotGO, Vector3.zero, Quaternion.identity);
				GameObjectInspector.InitInstantiatedPreviewRecursive(m_PivotInstance);
			}

			if (m_RootInstance == null)
			{
				GameObject rootGO = (GameObject)EditorGUIUtility.Load("Avatar/root.fbx");
				m_RootInstance = (GameObject)Object.Instantiate(rootGO, Vector3.zero, Quaternion.identity);
				GameObjectInspector.InitInstantiatedPreviewRecursive(m_RootInstance);
			}
			
			// Load preview settings from prefs
			m_IKOnFeet = EditorPrefs.GetBool (kIkPref, false);
			m_ShowReference = EditorPrefs.GetBool (kReferencePref, true);
			timeControl.playbackSpeed = EditorPrefs.GetFloat (kSpeedPref, 1f);

			SetPreviewCharacterEnabled(false, false);
		}

		private void Init()
		{
			if (m_PreviewUtility == null)
			{
				m_PreviewUtility = new PreviewRenderUtility(true);
				m_PreviewUtility.m_CameraFieldOfView = 30.0f;
				m_PreviewUtility.m_Camera.cullingMask = 1 << kPreviewLayer;
			}

			if (s_Styles == null)
				s_Styles = new Styles ();
			
			if (m_FloorPlane == null)
			{
				m_FloorPlane = Resources.GetBuiltinResource(typeof(Mesh), "New-Plane.fbx") as Mesh;
			}

			if (m_FloorTexture == null)
			{
				m_FloorTexture = (Texture2D)EditorGUIUtility.Load ("Avatar/Textures/AvatarFloor.png");
			}

			if (m_FloorMaterial == null)
			{
				Shader shader = EditorGUIUtility.LoadRequired("Previews/PreviewPlaneWithShadow.shader") as Shader;
				m_FloorMaterial = new Material (shader);
				m_FloorMaterial.mainTexture = m_FloorTexture;
				m_FloorMaterial.mainTextureScale = Vector2.one * kFloorScale * kFloorTextureScale;
				m_FloorMaterial.SetVector ("_Alphas", new Vector4(kFloorAlpha, kFloorShadowAlpha, 0, 0));
				m_FloorMaterial.hideFlags = HideFlags.DontSave;
				
				m_FloorMaterialSmall = new Material (m_FloorMaterial);
				m_FloorMaterialSmall.mainTextureScale = Vector2.one * kFloorScaleSmall * kFloorTextureScale;
				m_FloorMaterialSmall.hideFlags = HideFlags.DontSave;
			}

			if (m_ShadowMaskMaterial == null)
			{
				Shader shader = EditorGUIUtility.LoadRequired("Previews/PreviewShadowMask.shader") as Shader;
				m_ShadowMaskMaterial = new Material (shader);
				m_ShadowMaskMaterial.hideFlags = HideFlags.DontSave;
			}
			
			if (m_ShadowPlaneMaterial == null)
			{
				Shader shader = EditorGUIUtility.LoadRequired("Previews/PreviewShadowPlaneClip.shader") as Shader;
				m_ShadowPlaneMaterial = new Material (shader);
				m_ShadowPlaneMaterial.hideFlags = HideFlags.DontSave;
			}
		}
		
		public void OnDestroy()
		{
			if (m_PreviewUtility != null)
			{
				m_PreviewUtility.Cleanup();
				m_PreviewUtility = null;
			}

			Object.DestroyImmediate(m_PreviewInstance);
			Object.DestroyImmediate(m_FloorMaterial);
			Object.DestroyImmediate(m_FloorMaterialSmall);
			Object.DestroyImmediate(m_ShadowMaskMaterial);
			Object.DestroyImmediate(m_ShadowPlaneMaterial);
			Object.DestroyImmediate(m_ReferenceInstance);
			Object.DestroyImmediate(m_RootInstance);
			Object.DestroyImmediate(m_PivotInstance);
			Object.DestroyImmediate(m_DirectionInstance);
			
			if (timeControl != null)
				timeControl.OnDisable();
		}

		public void DoSelectionChange ()
		{
			m_OnAvatarChangeFunc ();
		}

		public AvatarPreview (Animator previewObjectInScene, Motion objectOnSameAsset)
		{
			InitInstance(previewObjectInScene, objectOnSameAsset);
		}

		float PreviewSlider(float val, float snapThreshold)
		{
			val = GUILayout.HorizontalSlider(val, 0.1f, 2.0f, s_Styles.preSlider, s_Styles.preSliderThumb, GUILayout.MaxWidth(64));
			if (val > 0.25f - snapThreshold && val < 0.25f + snapThreshold)
				val = 0.25f;
			else if (val > 0.5f - snapThreshold && val < 0.5f + snapThreshold)
				val = 0.5f;
			else if (val > 0.75f - snapThreshold && val < 0.75f + snapThreshold)
				val = 0.75f;
			else if (val > 1.0f - snapThreshold && val < 1.0f + snapThreshold)
				val = 1.0f;
			else if (val > 1.25f - snapThreshold && val < 1.25f + snapThreshold)
				val = 1.25f;
			else if (val > 1.5f - snapThreshold && val < 1.5f + snapThreshold)
				val = 1.5f;
			else if (val > 1.75f - snapThreshold && val < 1.75f + snapThreshold)
				val = 1.75f;

			return val;
		}
		
		public void DoPreviewSettings ()
		{
			Init();
			if(m_ShowIKOnFeetButton)
			{
				EditorGUI.BeginChangeCheck ();
				m_IKOnFeet = GUILayout.Toggle(m_IKOnFeet, s_Styles.ik, s_Styles.preButton);
				if (EditorGUI.EndChangeCheck ())
					EditorPrefs.SetBool (kIkPref, m_IKOnFeet);
			}
			
			EditorGUI.BeginChangeCheck ();
			m_ShowReference = GUILayout.Toggle (m_ShowReference, s_Styles.pivot, s_Styles.preButton);
			if (EditorGUI.EndChangeCheck ())
				EditorPrefs.SetBool (kReferencePref, m_ShowReference);
			
			GUILayout.Box (s_Styles.speedScale, s_Styles.preLabel);
			EditorGUI.BeginChangeCheck ();
			timeControl.playbackSpeed = PreviewSlider(timeControl.playbackSpeed, 0.03f);
			if (EditorGUI.EndChangeCheck ())
				EditorPrefs.SetFloat (kSpeedPref, timeControl.playbackSpeed);
			GUILayout.Label (timeControl.playbackSpeed.ToString ("f2"), s_Styles.preLabel);
		}


		private RenderTexture RenderPreviewShadowmap(Light light, float scale, Vector3 center, Vector3 floorPos, out Matrix4x4 outShadowMatrix)
		{
			// Set ortho camera and position it
			var cam = m_PreviewUtility.m_Camera;
			cam.orthographic = true;
			cam.orthographicSize = scale * 2.0f;
			cam.nearClipPlane = 1 * scale;
			cam.farClipPlane = 25 * scale;
			cam.transform.rotation = light.transform.rotation;
			cam.transform.position = center - light.transform.forward * (scale * 5.5f);

			// Clear to black
			CameraClearFlags oldFlags = cam.clearFlags;
			cam.clearFlags = CameraClearFlags.Color;
			Color oldColor = cam.backgroundColor;
			cam.backgroundColor = new Color(0,0,0,0);

			// Create render target for shadow map
			const int kShadowSize = 256;
			RenderTexture oldRT = cam.targetTexture;
			RenderTexture rt = RenderTexture.GetTemporary (kShadowSize, kShadowSize, 16);
			rt.isPowerOfTwo = true;
			rt.wrapMode = TextureWrapMode.Clamp;
			rt.filterMode = FilterMode.Bilinear;
			cam.targetTexture = rt;

			// Enable character and render with camera into the shadowmap
			SetPreviewCharacterEnabled (true, false);
			m_PreviewUtility.m_Camera.Render ();

			// Draw a quad, with shader that will produce white color everywhere
			// where something was rendered (via inverted depth test)
			RenderTexture.active = rt;
			GL.PushMatrix ();
			GL.LoadOrtho ();
			m_ShadowMaskMaterial.SetPass (0);
			GL.Begin (GL.QUADS);
			GL.Vertex3 (0, 0, -99.0f);
			GL.Vertex3 (1, 0, -99.0f);
			GL.Vertex3 (1, 1, -99.0f);
			GL.Vertex3 (0, 1, -99.0f);
			GL.End ();
			
			// Render floor with black color, to mask out any shadow from character
			// parts that are under the preview plane
			GL.LoadProjectionMatrix (cam.projectionMatrix);
			GL.LoadIdentity();
			GL.MultMatrix (cam.worldToCameraMatrix);
			m_ShadowPlaneMaterial.SetPass(0);
			GL.Begin (GL.QUADS);
			float sc = kFloorScale * scale;
			GL.Vertex (floorPos + new Vector3(-sc,0,-sc));
			GL.Vertex (floorPos + new Vector3( sc,0,-sc));
			GL.Vertex (floorPos + new Vector3( sc,0, sc));
			GL.Vertex (floorPos + new Vector3(-sc,0, sc));
			GL.End ();
			
			GL.PopMatrix ();

			// Shadowmap sampling matrix, from world space into shadowmap space
			Matrix4x4 texMatrix = Matrix4x4.TRS (new Vector3 (0.5f, 0.5f, 0.5f), Quaternion.identity,
			                                     new Vector3 (0.5f, 0.5f, 0.5f));
			outShadowMatrix = texMatrix * cam.projectionMatrix * cam.worldToCameraMatrix;

			// Restore previous camera parameters
			cam.orthographic = false;
			cam.clearFlags = oldFlags;
			cam.backgroundColor = oldColor;
			cam.targetTexture = oldRT;

			return rt;
		}

		
		public Texture DoRenderPreview(Rect previewRect, GUIStyle background)
		{
			m_PreviewUtility.BeginPreview(previewRect, background);

			Quaternion bodyRot;
			Vector3 bodyPos;
			Quaternion rootRot;
			Vector3 rootPos;
			Vector3 pivotPos;
			float scale;

			if (Animator && Animator.isHuman)
			{
                rootRot = Animator.rootRotation;
                rootPos = Animator.rootPosition;
                
                bodyRot = Animator.bodyRotation;
                bodyPos = Animator.bodyPosition;
				
				pivotPos = Animator.pivotPosition;
				scale = Animator.humanScale;
			}
            else if (Animator && Animator.hasRootMotion)
            {
                rootRot = Animator.rootRotation;
                rootPos = Animator.rootPosition;

                bodyRot = Quaternion.identity;
                bodyPos = GameObjectInspector.GetRenderableCenterRecurse(m_PreviewInstance, 2, 8);

                pivotPos = Vector3.zero;
                scale = m_BoundingVolumeScale / 2;
            }
			else
			{
                rootRot = Quaternion.identity;
                rootPos = Vector3.zero;
                				
				bodyRot = Quaternion.identity;
                bodyPos = GameObjectInspector.GetRenderableCenterRecurse(m_PreviewInstance, 2, 8);

				pivotPos = Vector3.zero;				
				scale = m_BoundingVolumeScale / 2;
			}

			bool oldFog = SetupPreviewLightingAndFx ();

			Vector3 direction = bodyRot*Vector3.forward;
			direction[1] = 0;
			Quaternion directionRot = Quaternion.LookRotation(direction);
			Vector3 directionPos = rootPos;

			Quaternion pivotRot = rootRot;

			PositionPreviewObjects (pivotRot, pivotPos, bodyRot, bodyPos, directionRot, rootRot, rootPos, directionPos, scale);

			
			bool dynamicFloorHeight = Mathf.Abs (m_NextFloorHeight - m_PrevFloorHeight) > scale * 0.01f;
			
			// Calculate floor height and alpha
			float mainFloorHeight, mainFloorAlpha;
			if (dynamicFloorHeight)
			{
				float fadeMoment = m_NextFloorHeight < m_PrevFloorHeight ? kFloorFadeDuration : (1 - kFloorFadeDuration);
				mainFloorHeight = timeControl.normalizedTime < fadeMoment ? m_PrevFloorHeight : m_NextFloorHeight;
				mainFloorAlpha = Mathf.Clamp01 (Mathf.Abs (timeControl.normalizedTime - fadeMoment) / kFloorFadeDuration);
			}
			else
			{
				mainFloorHeight = m_PrevFloorHeight;
				mainFloorAlpha = 1;
			}

			Quaternion floorRot = Quaternion.identity;
			Vector3 floorPos = new Vector3 (0, 0, 0);
			floorPos = m_ReferenceInstance.transform.position;
			floorPos.y = mainFloorHeight;
			
			// Render shadow map
			Matrix4x4 shadowMatrix;
			RenderTexture shadowMap = RenderPreviewShadowmap (m_PreviewUtility.m_Light[0], scale, bodyPos, floorPos, out shadowMatrix);

			// Position camera
			m_PreviewUtility.m_Camera.nearClipPlane = 1 * scale;
			m_PreviewUtility.m_Camera.farClipPlane = 25 * scale;
			Quaternion camRot = Quaternion.Euler (-m_PreviewDir.y, -m_PreviewDir.x, 0);
			Vector3 camPos = camRot * (Vector3.forward * -5.5f * scale) + bodyPos;
			m_PreviewUtility.m_Camera.transform.position = camPos;
			m_PreviewUtility.m_Camera.transform.rotation = camRot;

			// Render main floor
			{
				floorPos.y = mainFloorHeight;
				
				Material mat = m_FloorMaterial;
				mat.mainTextureOffset = -mat.mainTextureScale * 0.5f -new Vector2 (floorPos.x, floorPos.z) / scale * 0.1f * kFloorTextureScale;
				mat.SetTexture ("_ShadowTexture", shadowMap);
				mat.SetMatrix ("_ShadowTextureMatrix", shadowMatrix);
				mat.SetVector ("_Alphas", new Vector4 (kFloorAlpha * mainFloorAlpha, kFloorShadowAlpha * mainFloorAlpha, 0, 0));
				Matrix4x4 matrix = Matrix4x4.TRS (floorPos, floorRot, Vector3.one * scale * kFloorScale);
				Graphics.DrawMesh (m_FloorPlane, matrix, mat, kPreviewLayer, m_PreviewUtility.m_Camera, 0);
			}
			
			// Render small floor
			if (dynamicFloorHeight)
			{
				bool topIsNext = m_NextFloorHeight > m_PrevFloorHeight;
				float floorHeight = topIsNext ? m_NextFloorHeight : m_PrevFloorHeight;
				float otherFloorHeight = topIsNext ? m_PrevFloorHeight : m_NextFloorHeight;
				float floorAlpha = (floorHeight == mainFloorHeight ? 1 - mainFloorAlpha : 1) * Mathf.InverseLerp (otherFloorHeight, floorHeight, rootPos.y);
				floorPos.y = floorHeight;
				
				Material mat = m_FloorMaterialSmall;
				mat.mainTextureOffset = -mat.mainTextureScale * 0.5f  -new Vector2 (floorPos.x, floorPos.z) / (scale) * 0.1f * kFloorTextureScale;
				mat.SetTexture ("_ShadowTexture", shadowMap);
				mat.SetMatrix ("_ShadowTextureMatrix", shadowMatrix);
				mat.SetVector ("_Alphas", new Vector4 (kFloorAlpha * floorAlpha, 0, 0, 0));
				Matrix4x4 matrix = Matrix4x4.TRS (floorPos, floorRot, Vector3.one * scale * kFloorScaleSmall);
				Graphics.DrawMesh (m_FloorPlane, matrix, mat, kPreviewLayer, m_PreviewUtility.m_Camera, 0);
			}
			
			SetPreviewCharacterEnabled (true, m_ShowReference);
			m_PreviewUtility.m_Camera.Render();
			SetPreviewCharacterEnabled(false, false);

			TeardownPreviewLightingAndFx(oldFog);
			RenderTexture.ReleaseTemporary(shadowMap);

			return m_PreviewUtility.EndPreview();
		}


		private bool SetupPreviewLightingAndFx ()
		{
			m_PreviewUtility.m_Light[0].intensity = .7f;
			m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (40f, 40f, 0);
			m_PreviewUtility.m_Light[1].intensity = .7f;

			Color amb = new Color (.1f, .1f, .1f, 0);
			InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);
			bool oldFog = RenderSettings.fog;
			Unsupported.SetRenderSettingsUseFogNoDirty (false);
			return oldFog;
		}


		private static void TeardownPreviewLightingAndFx (bool oldFog)
		{
			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			InternalEditorUtility.RemoveCustomLighting ();
		}
		
		private float m_LastNormalizedTime = -1000;
		private float m_LastStartTime = -1000;
		private float m_LastStopTime = -1000;
		private bool m_NextTargetIsForward = true;
		private void PositionPreviewObjects (Quaternion pivotRot, Vector3 pivotPos, Quaternion bodyRot, Vector3 bodyPos,
		                                     Quaternion directionRot, Quaternion rootRot, Vector3 rootPos, Vector3 directionPos,
		                                     float scale)
		{
			m_ReferenceInstance.transform.position = rootPos;
			m_ReferenceInstance.transform.rotation = rootRot;
			m_ReferenceInstance.transform.localScale = Vector3.one * scale * 1.25f;

			m_DirectionInstance.transform.position = directionPos;
			m_DirectionInstance.transform.rotation = directionRot;
			m_DirectionInstance.transform.localScale = Vector3.one * scale * 2;

			m_PivotInstance.transform.position = pivotPos;
			m_PivotInstance.transform.rotation = pivotRot;
			m_PivotInstance.transform.localScale = Vector3.one * scale * 0.1f;

			m_RootInstance.transform.position = bodyPos;
			m_RootInstance.transform.rotation = bodyRot;
			m_RootInstance.transform.localScale = Vector3.one * scale * 0.25f;

			if (Animator)
			{
				float normalizedTime = timeControl.normalizedTime;
				float normalizedDelta = timeControl.deltaTime / (timeControl.stopTime - timeControl.startTime);
				
				// Always set last height to next height after wrapping the time.
				if (normalizedTime-normalizedDelta < 0 || normalizedTime-normalizedDelta >= 1)
					m_PrevFloorHeight = m_NextFloorHeight;
				
				// Check that AvatarPreview is getting reliable info about time and deltaTime.
				if (m_LastNormalizedTime != -1000 && timeControl.startTime == m_LastStartTime && timeControl.stopTime == m_LastStopTime)
				{
					float difference = normalizedTime-normalizedDelta - m_LastNormalizedTime;
					if (difference > 0.5f)
						difference -= 1;
					else if (difference < -0.5f)
						difference += 1;
					if (Mathf.Abs (difference) > Mathf.Max (0.0001f, Mathf.Abs (normalizedDelta * 0.1f)))
						Debug.LogError ("Reported delta does not match with progression of time. Check that the OnInteractivePreviewGUI function only updates the Animator on Repaint events.\n"+
							"lastNormalTime: "+m_LastNormalizedTime+
							", normalizedDelta: "+normalizedDelta+
							", expectedNormalizedTime: "+(Mathf.Repeat (m_LastNormalizedTime+normalizedDelta,1))+
							", normalizedTime: "+normalizedTime+
							", difference: "+difference);
				}
				m_LastNormalizedTime = normalizedTime;
				m_LastStartTime = timeControl.startTime;
				m_LastStopTime = timeControl.stopTime;
				
				// Alternate getting the height for next time and previous time.
				if (m_NextTargetIsForward)
					m_NextFloorHeight = Animator.targetPosition.y;
				else
					m_PrevFloorHeight = Animator.targetPosition.y;
				
				// Flip next target time.
				m_NextTargetIsForward = !m_NextTargetIsForward;
				Animator.SetTarget (AvatarTarget.Root, m_NextTargetIsForward ? 1 : 0);
			}
		}


		public void DoAvatarPreviewDrag(EventType type)
		{
			if (type == EventType.DragUpdated)
			{
				DragAndDrop.visualMode = DragAndDropVisualMode.Link;
			}
			else if (type == EventType.DragPerform)
			{
				DragAndDrop.visualMode = DragAndDropVisualMode.Link;
				GameObject newPreviewObject = DragAndDrop.objectReferences[0] as GameObject;
			
				if (newPreviewObject && GetAnimationType(newPreviewObject) == animationClipType)
				{
					DragAndDrop.AcceptDrag();
					SetPreview(newPreviewObject);
				}
			}
		}

		public void AvatarTimeControlGUI (Rect rect)
		{
			Rect timeControlRect = rect;
			timeControlRect.height = kTimeControlRectHeight;

			timeControl.DoTimeControl(timeControlRect);

			// Show current time in seconds:frame and in percentage
			rect.y = rect.yMax - 20;
			float time = timeControl.currentTime - timeControl.startTime;
			EditorGUI.DropShadowLabel(new Rect(rect.x, rect.y, rect.width, 20),
				string.Format("{0,2}:{1:00} ({2:000.0%})", (int)time, Repeat(Mathf.FloorToInt(time * fps), fps), timeControl.normalizedTime)
			);
		}
		
		enum PreviewPopupOptions { Auto, DefaultModel, Other }
		
		public void DoAvatarPreview(Rect rect, GUIStyle background)
		{
			Init();
			
			Rect choserRect = new Rect (rect.xMax-16, rect.yMax-16, 16, 16);
			if (EditorGUI.ButtonMouseDown (choserRect, GUIContent.none, FocusType.Passive, GUIStyle.none))
			{
				GenericMenu menu = new GenericMenu ();
				menu.AddItem (new GUIContent ("Auto"), false, SetPreviewAvatarOption, PreviewPopupOptions.Auto);
				menu.AddItem (new GUIContent ("Unity Model"), false, SetPreviewAvatarOption, PreviewPopupOptions.DefaultModel);
				menu.AddItem (new GUIContent ("Other..."), false, SetPreviewAvatarOption, PreviewPopupOptions.Other);
				menu.ShowAsContext ();
			}
			
			Rect previewRect = rect;
			previewRect.yMin += kTimeControlRectHeight;
			previewRect.height = Mathf.Max (previewRect.height, 64f);
			
			m_PreviewDir = PreviewGUI.Drag2D (m_PreviewDir, previewRect);
			
			int previewID = GUIUtility.GetControlID(m_PreviewHint, FocusType.Native, previewRect);
			Event evt = Event.current;

			EventType type = evt.GetTypeForControl(previewID);

			if (type == EventType.Repaint && m_IsValid)
			{
				Texture renderedTexture = DoRenderPreview(previewRect, background);
				GUI.DrawTexture(previewRect, renderedTexture, ScaleMode.StretchToFill, false);
			}
			
			AvatarTimeControlGUI (rect);

			GUI.DrawTexture (choserRect, s_Styles.avatarIcon.image);

			if (!m_IsValid)
			{
				Rect warningRect = previewRect;
				warningRect.yMax -= warningRect.height / 2 - 16;
				EditorGUI.DropShadowLabel (
					warningRect,
					"No model is available for preview.\nPlease drag a model into this Preview Area.");
			}

			DoAvatarPreviewDrag (type);
			
			// Check for model selected from ObjectSelector
			if (evt.type == EventType.ExecuteCommand)
			{
				string commandName = evt.commandName;
				if (commandName == "ObjectSelectorUpdated" && ObjectSelector.get.objectSelectorID == m_ModelSelectorId)
				{
					SetPreview (ObjectSelector.GetCurrentObject () as GameObject);
					evt.Use();
				}
			}
		}
		
		void SetPreviewAvatarOption (object obj)
		{
			PreviewPopupOptions option = (PreviewPopupOptions)obj;
			if (option == PreviewPopupOptions.Auto)
			{
				SetPreview(null);
			}
			else if (option == PreviewPopupOptions.DefaultModel)
			{
				SetPreview(GetHumanoidFallback ());
			}
			else if (option == PreviewPopupOptions.Other)
			{
				ObjectSelector.get.Show (null, typeof (GameObject), null, false);
				ObjectSelector.get.objectSelectorID = m_ModelSelectorId;
			}
		}
		
		void SetPreview(GameObject gameObject)
		{
			AvatarPreviewSelection.SetPreview(animationClipType, gameObject);
			
			Object.DestroyImmediate(m_PreviewInstance);
			InitInstance(m_SourceScenePreviewAnimator, m_SourcePreviewMotion);

			if (m_OnAvatarChangeFunc != null)
				m_OnAvatarChangeFunc();
		}
		
		int Repeat (int t, int length)
		{
			// Have to do double modulo in order to work for negative numbers.
			// This is quicker than a branch to test for negative number.
			return ((t % length) + length) % length;
		}
	} // class AvatarPreview
} // namespace UnityEditor
