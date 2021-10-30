using UnityEngine;
using System.Collections.Generic;


namespace UnityEditor
{
class SubModuleUI : ModuleUI
{
	SerializedProperty[,] m_SubEmitters;

	private int m_CheckObjectTypeIndex = -1;
	private int m_CheckObjectIndex = -1;

	// Keep in sync with enum in ParticleSystemModules.h

	public enum SubEmitterType {

		None = -1,

		Birth = 0,

		Collision,

		Death,

		TypesMax,

	};

	const int k_MaxSubPerType = 2;
	
	class Texts
	{

		public Texts()

		{

			subEmitterTypeTexts = new GUIContent[(int)SubEmitterType.TypesMax];

			subEmitterTypeTexts[(int)SubEmitterType.Birth] = new GUIContent("Birth", "Start spawning on birth of particle.");

			subEmitterTypeTexts[(int)SubEmitterType.Collision] = new GUIContent("Collision", "Spawn on collision of particle. Sub emitter can only emit as burst.");

			subEmitterTypeTexts[(int)SubEmitterType.Death] = new GUIContent("Death", "Spawn on death of particle. Sub emitter can only emit as burst.");

		}



		public GUIContent[] subEmitterTypeTexts;
		public GUIContent create = new GUIContent("", "Create and assign a Particle System as sub emitter");
	}
	private static Texts s_Texts;


	public SubModuleUI (ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "SubModule", displayName)
	{
		m_ToolTip = "Sub emission of particles. This allows each particle to emit particles in another system.";
		Init (); // Init when created because we need to query if we have any subemitters referenced
	}

	protected override void Init()
	{
		// Already initialized?

		if (m_SubEmitters != null)
			return;



		m_SubEmitters = new SerializedProperty[(int)SubEmitterType.TypesMax, k_MaxSubPerType];

		m_SubEmitters[(int)SubEmitterType.Birth, 0] = GetProperty("subEmitterBirth");

		m_SubEmitters[(int)SubEmitterType.Birth, 1] = GetProperty("subEmitterBirth1");

		m_SubEmitters[(int)SubEmitterType.Collision, 0] = GetProperty("subEmitterCollision");

		m_SubEmitters[(int)SubEmitterType.Collision, 1] = GetProperty("subEmitterCollision1");

		m_SubEmitters[(int)SubEmitterType.Death, 0] = GetProperty("subEmitterDeath");

		m_SubEmitters[(int)SubEmitterType.Death, 1] = GetProperty("subEmitterDeath1");
	}

	override public void Validate ()
	{
	}

	void CreateAndAssignSubEmitter(SerializedProperty objectRefProp, SubModuleUI.SubEmitterType type)
	{
		GameObject subEmitter = m_ParticleSystemUI.m_ParticleEffectUI.CreateParticleSystem (m_ParticleSystemUI.m_ParticleSystem, type);
		subEmitter.name = "SubEmitter";
		objectRefProp.objectReferenceValue = subEmitter.GetComponent<ParticleSystem>();
	}

	void Update()
	{

		if ((m_CheckObjectIndex >= 0) && (m_CheckObjectTypeIndex >= 0))
		{
			// Wait until the ObjectSelector is closed before checking object
			if (!ObjectSelector.isVisible)
			{

				Object obj = m_SubEmitters[m_CheckObjectTypeIndex, m_CheckObjectIndex].objectReferenceValue;
				ParticleSystem newSubEmitter = obj as ParticleSystem;
				if (newSubEmitter != null)
				{
					bool validSubemitter = true;

					if (ValidateSubemitter(newSubEmitter))
					{
						string errorMsg = ParticleSystemEditorUtils.CheckCircularReferences(newSubEmitter, m_ParticleSystemUI.m_ParticleSystem, m_ParticleSystemUI.m_ParticleEffectUI.GetRoot());
						if (errorMsg.Length == 0)
						{
							// Ok there is no circular references, now check if its a child
							CheckIfChild(obj);
						}
						else
						{
							// Circular references detected
							string circularRefErrorMsg = string.Format("'{0}' could not be assigned as subemitter on '{1}' due to circular referencing!\nBacktrace: {2} \n\nReference will be removed.", newSubEmitter.gameObject.name, m_ParticleSystemUI.m_ParticleSystem.gameObject.name, errorMsg);
							EditorUtility.DisplayDialog("Circular References Detected", circularRefErrorMsg, "Ok");
							validSubemitter = false;
						}
					}
					else
					{
						validSubemitter = false;
					}

					if (!validSubemitter)
					{

						m_SubEmitters[m_CheckObjectTypeIndex, m_CheckObjectIndex].objectReferenceValue = null; // remove invalid reference
						m_ParticleSystemUI.ApplyProperties ();
						m_ParticleSystemUI.m_ParticleEffectUI.m_Owner.Repaint();
					}
				}
				// Cleanup
				m_CheckObjectIndex = -1;

				m_CheckObjectTypeIndex = -1;
				EditorApplication.update -= Update;
			}
		}
	}


	internal static bool IsChild (ParticleSystem subEmitter, ParticleSystem root)
	{
		if (subEmitter == null || root == null)
			return false;

		ParticleSystem subRoot = ParticleSystemEditorUtils.GetRoot(subEmitter);
		return subRoot == root;
	}

	private bool ValidateSubemitter(ParticleSystem subEmitter)
	{
		if (subEmitter == null)
			return false;
	
		ParticleSystem root = m_ParticleSystemUI.m_ParticleEffectUI.GetRoot();
		if (root.gameObject.activeInHierarchy && !subEmitter.gameObject.activeInHierarchy)
		{
			string kReparentText = string.Format("The assigned sub emitter is part of a prefab and can therefore not be assigned.");
			EditorUtility.DisplayDialog("Invalid Sub Emitter", kReparentText, "Ok");
			return false;
		}
		
		if (!root.gameObject.activeInHierarchy && subEmitter.gameObject.activeInHierarchy)
		{
			string kReparentText = string.Format("The assigned sub emitter is part of a scene object and can therefore not be assigned to a prefab.");
			EditorUtility.DisplayDialog("Invalid Sub Emitter", kReparentText, "Ok");
			return false;
		}


		return true;
	}

	private void CheckIfChild (Object subEmitter)
	{
		if (subEmitter != null)
		{
			ParticleSystem root = m_ParticleSystemUI.m_ParticleEffectUI.GetRoot();
			if (IsChild(subEmitter as ParticleSystem, root))
			{
				return; 
			}

			string kReparentText = string.Format("The assigned sub emitter is not a child of the current root particle system GameObject: '{0}' and is therefore NOT considered a part of the current effect. Do you want to reparent it?", root.gameObject.name);
			if (EditorUtility.DisplayDialog(
				"Reparent GameObjects",
				kReparentText,
				"Yes, Reparent",
				"No"))
			{
				if (EditorUtility.IsPersistent(subEmitter))
				{
					var newGo = Object.Instantiate(subEmitter) as GameObject;
					if (newGo != null)
					{
						newGo.transform.parent = m_ParticleSystemUI.m_ParticleSystem.transform;
						newGo.transform.localPosition = Vector3.zero;
						newGo.transform.localRotation = Quaternion.identity;
					}
				}
				else
				{
					ParticleSystem ps = subEmitter as ParticleSystem;
					if (ps)
					{
						ps.gameObject.transform.parent = m_ParticleSystemUI.m_ParticleSystem.transform;
					}
				}
			}
		}	
	}

	

	override public void OnInspectorGUI(ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts ();



		Object[,] props = { {m_SubEmitters[0,0].objectReferenceValue, m_SubEmitters[0,1].objectReferenceValue},
		                    {m_SubEmitters[1,0].objectReferenceValue, m_SubEmitters[1,1].objectReferenceValue},
		                    {m_SubEmitters[2,0].objectReferenceValue, m_SubEmitters[2,1].objectReferenceValue}, };



		for (int i = 0; i < (int)SubEmitterType.TypesMax; i++)

		{

			int index = GUIListOfFloatObjectToggleFields(s_Texts.subEmitterTypeTexts[i], new[] { m_SubEmitters[i, 0], m_SubEmitters[i, 1] }, null, s_Texts.create, true);

			if (index != -1)

				CreateAndAssignSubEmitter(m_SubEmitters[i, index], (SubEmitterType)i);

		}



		Object[,] props2 = { {m_SubEmitters[0,0].objectReferenceValue, m_SubEmitters[0,1].objectReferenceValue},
		                     {m_SubEmitters[1,0].objectReferenceValue, m_SubEmitters[1,1].objectReferenceValue},
		                     {m_SubEmitters[2,0].objectReferenceValue, m_SubEmitters[2,1].objectReferenceValue}, };



		for (int i = 0; i < (int)SubEmitterType.TypesMax; i++)

		{

			for (int j = 0; j < k_MaxSubPerType; j++)

			{

				if (props[i,j] != props2[i,j])

				{

					if ((m_CheckObjectIndex == -1) && (m_CheckObjectTypeIndex == -1))

						EditorApplication.update += Update; // ensure its not added more than once



					// We need to let the ObjectSelector finish its SendEvent and therefore delay showing dialog 

					m_CheckObjectTypeIndex = i;

					m_CheckObjectIndex = j;

				}

			}
		}
	}



	override public void UpdateCullingSupportedString(ref string text)

	{

		text += "\n\tSub Emitters are enabled.";

	}


}
} // namespace UnityEditor
