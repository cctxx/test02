using System.Collections.Generic;
using UnityEngine;
using NUnit.Framework;

// The ParticleSystemUI displays and manages the modules of a particle system.

namespace UnityEditor
{

internal class ParticleSystemUI
{
	public ParticleEffectUI m_ParticleEffectUI; // owner
	public ModuleUI[] m_Modules;
	public ParticleSystem m_ParticleSystem;
	public SerializedObject m_ParticleSystemSerializedObject;
	public SerializedObject m_RendererSerializedObject;
	private static string[] s_ModuleNames;

	private static string m_SupportsCullingText;

	// Keep in sync with ParticleSystemEditor.h
	public enum DefaultTypes
	{
		Root,
		SubBirth,
		SubCollision,
		SubDeath,
	};

	protected class Texts
	{
		public GUIContent addModules = new GUIContent ("", "Show/Hide Modules");

		public GUIContent supportsCullingText = new GUIContent("", ParticleSystemStyles.Get().warningIcon);
	}
	private static Texts s_Texts;

	public void Init (ParticleEffectUI owner, ParticleSystem ps)
	{
		if (s_ModuleNames == null)
			s_ModuleNames = GetUIModuleNames();

		m_ParticleEffectUI = owner;
		m_ParticleSystem = ps;
		m_ParticleSystemSerializedObject = new SerializedObject (m_ParticleSystem);
		m_RendererSerializedObject = null;

		m_SupportsCullingText = null;

		m_Modules = CreateUIModules(this, m_ParticleSystemSerializedObject);
		ParticleSystemRenderer particleSystemRenderer = GetParticleSystemRenderer();
		if (particleSystemRenderer != null)
			InitRendererUI();



		UpdateParticleSystemInfoString();
	}

	internal ParticleSystemRenderer GetParticleSystemRenderer()
	{
		// The Particle System could have been deleted (during cleanup we want to reset editorEnabled on all Renderers)
		if (m_ParticleSystem != null)
			return m_ParticleSystem.GetComponent<ParticleSystemRenderer>();
		
		return null;
	}

	internal ModuleUI GetParticleSystemRendererModuleUI ()
	{
		return m_Modules[m_Modules.Length -1];
	}

	private void InitRendererUI ()
	{
		// Ensure we have a renderer
		ParticleSystemRenderer psRenderer = GetParticleSystemRenderer();
		if (psRenderer == null)
		{
			m_ParticleSystem.gameObject.AddComponent ("ParticleSystemRenderer");
		}

		// Create RendererModuleUI
		psRenderer = GetParticleSystemRenderer();
		if (psRenderer != null)
		{
			Assert.That (m_Modules[m_Modules.Length - 1] == null); // If hitting this assert we have either not cleaned up the previous renderer or hitting another module

			m_RendererSerializedObject = new SerializedObject (psRenderer);
			m_Modules[m_Modules.Length - 1] = new RendererModuleUI (this, m_RendererSerializedObject, s_ModuleNames[s_ModuleNames.Length-1]);
			// Sync to state
			EditorUtility.SetSelectedWireframeHidden(psRenderer, !ParticleEffectUI.m_ShowWireframe);
		}
	}

	private void ClearRenderer()
	{
		// Remove renderer component
		m_RendererSerializedObject = null;
		ParticleSystemRenderer psRenderer = GetParticleSystemRenderer();
		if (psRenderer != null)
		{
			Undo.DestroyObjectImmediate (psRenderer);
		}
		m_Modules[m_Modules.Length - 1] = null;
	}

	public string GetName ()
	{
		return m_ParticleSystem.gameObject.name;
	}


	public float GetEmitterDuration ()
	{
		InitialModuleUI m = m_Modules[0] as InitialModuleUI;
		if (m != null)
			return m.m_LengthInSec.floatValue;
		return -1.0f;
	}


	ParticleSystem GetSelectedParticleSystem ()
	{
		return Selection.activeGameObject.GetComponent<ParticleSystem>();
	}


	public void OnGUI (ParticleSystem root, float width, bool fixedWidth)
	{
		if (s_Texts == null)
			s_Texts = new Texts ();

		bool isRepaintEvent = Event.current.type == EventType.repaint;
		// Name of current emitter
		string selectedEmitterName = m_ParticleSystem ? m_ParticleSystem.gameObject.name : null;
		
		if (fixedWidth)
		{
			EditorGUIUtility.labelWidth = width * 0.55f;
			EditorGUILayout.BeginVertical (GUILayout.Width (width));
		}
		else
		{
			// First make sure labelWidth is at default width, then subtract
			EditorGUIUtility.labelWidth = 0;
			EditorGUIUtility.labelWidth = EditorGUIUtility.labelWidth - 4;
			
			EditorGUILayout.BeginVertical ();
		}
		
		{
			for (int i = 0; i < m_Modules.Length; ++i)
			{
				ModuleUI module = m_Modules[i];
				if (module == null)
					continue; 

				bool initialModule = (module == m_Modules[0]);

				// Skip if not visible (except initial module which should always be visible)
				if (!module.visibleUI && !initialModule)
					continue;

				// Module header size
				GUIContent headerLabel = new GUIContent();
				GUIStyle headerStyle;
				Rect moduleHeaderRect;
				if (initialModule)
				{
					moduleHeaderRect = GUILayoutUtility.GetRect(width, 25);
					headerStyle = ParticleSystemStyles.Get().emitterHeaderStyle;
				}
				else
				{
					moduleHeaderRect = GUILayoutUtility.GetRect(width, 15);
					headerStyle = ParticleSystemStyles.Get().moduleHeaderStyle;
				}

				// Module content here to render it below the the header
				if (module.foldout)
				{
					EditorGUI.BeginDisabledGroup (!module.enabled);
					Rect moduleSize = EditorGUILayout.BeginVertical(ParticleSystemStyles.Get().modulePadding);
					{
						moduleSize.y -= 4; // pull background 'up' behind title to fill rounded corners.
						moduleSize.height += 4;
						GUI.Label (moduleSize, GUIContent.none, ParticleSystemStyles.Get().moduleBgStyle);
						module.OnInspectorGUI (m_ParticleSystem);
					}
					EditorGUILayout.EndVertical();
					EditorGUI.EndDisabledGroup();
				}

				// TODO: Get Texture instead of static preview. Render Icon (below titlebar due to rounded corners)
				if (initialModule )
				{
					// Get preview of material or mesh
					ParticleSystemRenderer renderer = GetParticleSystemRenderer ();
					float iconSize = 21;
					Rect iconRect = new Rect(moduleHeaderRect.x + 4, moduleHeaderRect.y + 2, iconSize, iconSize);

					if (isRepaintEvent && renderer != null)
					{
						bool iconRendered = false;
						int instanceID = 0;
						
						RendererModuleUI rendererUI = m_Modules[m_Modules.Length-1] as RendererModuleUI;
						if (rendererUI != null)
						{
							if (rendererUI.IsMeshEmitter())
							{
								if (renderer.mesh != null)
									instanceID = renderer.mesh.GetInstanceID();
							}
							else if (renderer.sharedMaterial != null)
							{
								instanceID = renderer.sharedMaterial.GetInstanceID();
							}

							// If the asset is dirty we ensure to get a updated one by clearing cache of temporary previews
							if (EditorUtility.IsDirty(instanceID))
								AssetPreview.ClearTemporaryAssetPreviews();
						}
						if (instanceID != 0)
						{
							Texture2D icon = AssetPreview.GetAssetPreview(instanceID);
							if (icon != null)
							{
								GUI.DrawTexture(iconRect, icon, ScaleMode.StretchToFill, true);
								iconRendered = true;
							}
						}

						// Fill so we do not see the background when we have no icon (rare case)
						if (!iconRendered)
						{
							GUI.Label(iconRect, GUIContent.none, ParticleSystemStyles.Get().moduleBgStyle);
						}
					}

					// Select gameObject when clicking on icon
					if (EditorGUI.ButtonMouseDown (iconRect, GUIContent.none, FocusType.Passive, GUIStyle.none))
					{
						// Toggle selected particle system from selection
						if (EditorGUI.actionKey)
						{
							List<int> newSelection = new List<int>();
							int instanceID = m_ParticleSystem.gameObject.GetInstanceID();
							newSelection.AddRange(Selection.instanceIDs);
							if (!newSelection.Contains(instanceID) || newSelection.Count != 1)
							{
								if (newSelection.Contains(instanceID))
									newSelection.Remove(instanceID);
								else
									newSelection.Add(instanceID);
							}

							Selection.instanceIDs = newSelection.ToArray();
						}
						else
						{
							Selection.instanceIDs = new int[0];
							Selection.activeInstanceID = m_ParticleSystem.gameObject.GetInstanceID();
						}
					}
				}

				// Button logic for enabledness (see below for UI)
				Rect checkMarkRect = new Rect(moduleHeaderRect.x + 2, moduleHeaderRect.y + 1, 13, 13);
				if (!initialModule && GUI.Button(checkMarkRect, GUIContent.none, GUIStyle.none))
					module.enabled = !module.enabled;

				// Button logic for plus/minus (see below for UI)
				Rect plusRect = new Rect(moduleHeaderRect.x + moduleHeaderRect.width - 10, moduleHeaderRect.y + moduleHeaderRect.height - 10, 10, 10);
				Rect plusRectInteract = new Rect(plusRect.x - 4, plusRect.y - 4, plusRect.width + 4, plusRect.height + 4);

				Rect infoRect = new Rect(plusRect.x - 23, plusRect.y - 3, 16, 16);


				if (initialModule && EditorGUI.ButtonMouseDown (plusRectInteract, s_Texts.addModules, FocusType.Passive, GUIStyle.none))
					ShowAddModuleMenu ();

				// Module header (last to become top most renderered)
				if (!string.IsNullOrEmpty(selectedEmitterName))
					headerLabel.text = initialModule ? selectedEmitterName : module.displayName;
				else
					headerLabel.text = module.displayName;
				headerLabel.tooltip = module.toolTip;
				bool newToggleState = GUI.Toggle (moduleHeaderRect, module.foldout, headerLabel, headerStyle);
				if (newToggleState !=  module.foldout)
				{
					switch (Event.current.button)
					{
					case 0:
						bool newFoldoutState = !module.foldout;
						if (Event.current.control)
						{
							foreach (var moduleUi in m_Modules)
								if (moduleUi != null && moduleUi.visibleUI)
									moduleUi.foldout = newFoldoutState;
						}
						else
						{
							module.foldout = newFoldoutState;
						}
						break;
					case 1:
						if (initialModule)
							ShowEmitterMenu ();
						else
							ShowModuleMenu (i);
						break;
					}
				}

				// Render checkmark on top (logic: see above)
				if (!initialModule)
					GUI.Toggle(checkMarkRect, module.enabled, GUIContent.none, ParticleSystemStyles.Get().checkmark);

				if (isRepaintEvent)
				{
					// Render plus/minus on top
					if (initialModule)
						GUI.Label(plusRect, GUIContent.none, ParticleSystemStyles.Get().plus);
				}



				s_Texts.supportsCullingText.tooltip = m_SupportsCullingText;

				if (initialModule && s_Texts.supportsCullingText.tooltip != null)

					GUI.Label(infoRect, s_Texts.supportsCullingText);

				GUILayout.Space(1); // dist to next module

			} // foreach module
			GUILayout.Space(-1);
		}
		EditorGUILayout.EndVertical(); // end fixed moduleWidth;

		// Apply the property, handle undo
		ApplyProperties();
	}

	public void OnSceneGUI ()
	{
		if (m_Modules == null)
			return;

		// Render bounds
		if (m_ParticleSystem.particleCount > 0)
		{
			ParticleSystemRenderer particleSystemRenderer = GetParticleSystemRenderer();
			EditorUtility.SetSelectedWireframeHidden(particleSystemRenderer, !ParticleEffectUI.m_ShowWireframe);

			if (ParticleEffectUI.m_ShowWireframe)
			{
				ModuleUI rendererUI = GetParticleSystemRendererModuleUI();
				ParticleSystemRenderer psRenderer = GetParticleSystemRenderer();
				if (rendererUI != null && rendererUI.enabled && psRenderer.editorEnabled)
				{
					Vector3 extents = particleSystemRenderer.bounds.extents;
					
					Transform camTrans = Camera.current.transform;
					Vector2 size = new Vector2(0.0f, 0.0f);
					Vector3[] scales = new Vector3[8];
					scales[0] = new Vector3(-1.0f, -1.0f, -1.0f);
					scales[1] = new Vector3(-1.0f, -1.0f,  1.0f);
					scales[2] = new Vector3(-1.0f,  1.0f, -1.0f);
					scales[3] = new Vector3(-1.0f,  1.0f,  1.0f);
					scales[4] = new Vector3( 1.0f, -1.0f, -1.0f);
					scales[5] = new Vector3( 1.0f, -1.0f,  1.0f);
					scales[6] = new Vector3( 1.0f,  1.0f, -1.0f);
					scales[7] = new Vector3( 1.0f,  1.0f,  1.0f);
					
					for(int i = 0; i < 8; i++)
					{
						// @TODO: Make aspect correct
						size.x = Mathf.Max(size.x, Vector3.Dot(Vector3.Scale(scales[i], extents), camTrans.right));
						size.y = Mathf.Max(size.y, Vector3.Dot(Vector3.Scale(scales[i], extents), camTrans.up));
					}
					
					Handles.RectangleCap(0, particleSystemRenderer.bounds.center, Camera.current.transform.rotation, size);
					
				}
			}
		}

		UpdateProperties ();

		InitialModuleUI initial = (InitialModuleUI)m_Modules[0];
		foreach (var module in m_Modules)
		{
			if (module == null || !module.visibleUI || !module.enabled)
				continue;

			if (module.foldout)
			{
				module.OnSceneGUI (m_ParticleSystem, initial);
			}
		}
		// Apply the property, handle undo
		ApplyProperties();
	}

	public void ApplyProperties ()
	{
		bool hasModifiedProperties = m_ParticleSystemSerializedObject.hasModifiedProperties;
		
		m_ParticleSystemSerializedObject.ApplyModifiedProperties ();
		if (hasModifiedProperties)
		{
			// Refresh procedural supported string
			ParticleSystem root = ParticleSystemEditorUtils.GetRoot(m_ParticleSystem);
			if (!ParticleEffectUI.IsStopped(root) && ParticleSystemEditorUtils.editorResimulation)
				ParticleSystemEditorUtils.PerformCompleteResimulation();



			UpdateParticleSystemInfoString();
		}
		if (m_RendererSerializedObject != null) 
			m_RendererSerializedObject.ApplyModifiedProperties();
	}

	void UpdateParticleSystemInfoString()

	{

		string supportsCullingText = "";

		foreach (var module in m_Modules)

		{

			if (module == null || !module.visibleUI || !module.enabled)

				continue;



			module.UpdateCullingSupportedString(ref supportsCullingText);

		}



		if (supportsCullingText != "")

			m_SupportsCullingText = "Automatic culling is disabled because: " + supportsCullingText;

		else

			m_SupportsCullingText = null;

	}

	public void UpdateProperties ()
	{
		m_ParticleSystemSerializedObject.UpdateIfDirtyOrScript();
		if (m_RendererSerializedObject != null)
			m_RendererSerializedObject.UpdateIfDirtyOrScript();
	}

	void ResetModules ()
	{
		// Reset all
		foreach (var module in m_Modules)
			if (module != null)
			{
				module.enabled = false;
				if (!ParticleEffectUI.GetAllModulesVisible())
					module.visibleUI = false;
			}

		// Default setup has a renderer
		if (m_Modules[m_Modules.Length - 1] == null)
			InitRendererUI();

		// Default setup has shape, emission and renderer
		int[] defaultEnabledModulesIndicies = { 1, 2, m_Modules.Length - 1 };
		for (int i = 0; i < defaultEnabledModulesIndicies.Length; ++i)
		{
			int moduleIndex = defaultEnabledModulesIndicies[i];
			if (m_Modules[moduleIndex] != null)
			{
				m_Modules[moduleIndex].enabled = true;
				m_Modules[moduleIndex].visibleUI = true;
			}		
		}
	}

	void ShowAddModuleMenu ()
	{
		// Now create the menu, add items and show it
		GenericMenu menu = new GenericMenu();
		for (int i = 0; i < s_ModuleNames.Length; ++i)
		{
			if (m_Modules[i] == null || !m_Modules[i].visibleUI)
				menu.AddItem(new GUIContent(s_ModuleNames[i]), false, AddModuleCallback, i);
			else
				menu.AddDisabledItem(new GUIContent(s_ModuleNames[i]));
		}

		menu.AddSeparator("");
		menu.AddItem(new GUIContent("Show All Modules"), ParticleEffectUI.GetAllModulesVisible(), AddModuleCallback, 10000);
		menu.ShowAsContext();
		Event.current.Use();
	}
	void AddModuleCallback (object obj)
	{
		int index = (int)obj;
		if (index >= 0 && index < m_Modules.Length)
		{
			if (index == m_Modules.Length - 1)
			{
				InitRendererUI ();
			}
			else
			{
				m_Modules[index].enabled = true;
				m_Modules[index].foldout = true;
			}
		}
		else
		{
			m_ParticleEffectUI.SetAllModulesVisible (!ParticleEffectUI.GetAllModulesVisible());
		}
		ApplyProperties();
	}


	void ModuleMenuCallback (object obj)
	{
		int moduleIndex = (int)obj;
		bool isRendererModule = (moduleIndex == m_Modules.Length - 1);
		if (isRendererModule)
		{
			ClearRenderer ();
		}
		else
		{
			if (!ParticleEffectUI.GetAllModulesVisible())
				m_Modules[moduleIndex].visibleUI = false;

			m_Modules[moduleIndex].enabled = false;
		}
		
	}
	void ShowModuleMenu (int moduleIndex)
	{	
		// Now create the menu, add items and show it
		GenericMenu menu = new GenericMenu();
		
		if (!ParticleEffectUI.GetAllModulesVisible())
			menu.AddItem(new GUIContent("Remove"), false, ModuleMenuCallback, moduleIndex);
		else
			menu.AddDisabledItem(new GUIContent("Remove")); // Do not allow remove module when always show modules is enabled
		menu.ShowAsContext();
		Event.current.Use();
	}

	void EmitterMenuCallback(object obj)
	{
		int userData = (int)obj;
		switch (userData)
		{
			case 0:
				m_ParticleEffectUI.CreateParticleSystem (m_ParticleSystem, SubModuleUI.SubEmitterType.None);
				break;
			
			case 1:
				ResetModules ();
				break;
			
			case 2:
				EditorGUIUtility.PingObject (m_ParticleSystem);
				break;

			default:
				Assert.That ("Enum not handled!".Length == 0);
				break;
		}
	}


	void ShowEmitterMenu ()
	{
		// Now create the menu, add items and show it
		GenericMenu menu = new GenericMenu();

		menu.AddItem(new GUIContent("Show Location"), false, EmitterMenuCallback, 2);
		menu.AddSeparator("");
		if (m_ParticleSystem.gameObject.activeInHierarchy)
			menu.AddItem(new GUIContent("Create Particle System"), false, EmitterMenuCallback, 0);
		else
			menu.AddDisabledItem(new GUIContent("Create new Particle System"));

		menu.AddItem(new GUIContent("Reset"), false, EmitterMenuCallback, 1);
		menu.ShowAsContext();
		Event.current.Use();
	}

	private static ModuleUI[] CreateUIModules(ParticleSystemUI e, SerializedObject so)
	{
		int index = 0;
		// Order should match GetUIModuleNames 
		return new ModuleUI[] {
			new InitialModuleUI (e, so, s_ModuleNames[index++]),
			new EmissionModuleUI (e, so, s_ModuleNames[index++]),
			new ShapeModuleUI (e, so, s_ModuleNames[index++]),
			new VelocityModuleUI (e, so, s_ModuleNames[index++]),
			new ClampVelocityModuleUI (e, so, s_ModuleNames[index++]),
			new ForceModuleUI (e, so, s_ModuleNames[index++]),
			new ColorModuleUI (e, so, s_ModuleNames[index++]),
			new ColorByVelocityModuleUI (e, so, s_ModuleNames[index++]),
			new SizeModuleUI (e, so, s_ModuleNames[index++]),
			new SizeByVelocityModuleUI (e, so, s_ModuleNames[index++]),
			new RotationModuleUI (e, so, s_ModuleNames[index++]),
			new RotationByVelocityModuleUI (e, so, s_ModuleNames[index++]),
			new ExternalForcesModuleUI (e, so, s_ModuleNames[index++]),
            new CollisionModuleUI (e, so, s_ModuleNames[index++]),
			new SubModuleUI (e, so, s_ModuleNames[index++]),
			new UVModuleUI (e, so, s_ModuleNames[index++]),
			null, // RendererModule is created seperately in InitRendererUI (it can be added/removed)
		};
	}

	// Names used when adding modules from a drop list
	public static string[] GetUIModuleNames ()
	{
		// Order should match GetUIModules
		return new string[] {
			"",
			"Emission",
			"Shape", 
			"Velocity over Lifetime", 
			"Limit Velocity over Lifetime",
			"Force over Lifetime", 
			"Color over Lifetime",
			"Color by Speed", 
			"Size over Lifetime", 
			"Size by Speed", 
			"Rotation over Lifetime", 
			"Rotation by Speed", 
			"External Forces", 
			"Collision",
			"Sub Emitters",
			"Texture Sheet Animation",
            "Renderer"
			};
	}
} // class ParticleSystemUI
} // namespace UnityEditor
