using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{
internal class RendererModuleUI : ModuleUI
{
	// Keep in sync with the one in ParticleSystemRenderer.h
	const int k_MaxNumMeshes = 4;

	// BaseRenderer and Renderer
	SerializedProperty m_CastShadows;
	SerializedProperty m_ReceiveShadows;
	SerializedProperty m_Material;

	// From ParticleSystemRenderer
	SerializedProperty m_RenderMode;
	SerializedProperty[] m_Meshes = new SerializedProperty[k_MaxNumMeshes];
	SerializedProperty[] m_ShownMeshes;

	SerializedProperty m_MaxParticleSize;		//< How large is a particle allowed to be on screen at most? 1 is entire viewport. 0.5 is half viewport.
	SerializedProperty m_CameraVelocityScale;	//< How much the camera motion is factored in when determining particle stretching.
	SerializedProperty m_VelocityScale;			//< When Strech Particles is enabled, defines the length of the particle compared to its velocity.
	SerializedProperty m_LengthScale;			//< When Strech Particles is enabled, defines the length of the particle compared to its width.
	SerializedProperty m_SortMode;				//< What method of particle sorting to use. If none is specified, no sorting will occur.
	SerializedProperty m_SortingFudge;			//< Lower the number, most likely that these particles will appear in front of other transparent objects, including other particles.
	SerializedProperty m_NormalDirection;


	// Keep in sync with ParticleSystemRenderMode in ParticleSystemRenderer.h
	enum RenderMode
	{
		Billboard = 0,
		Stretch3D = 1,
		BillboardFixedHorizontal = 2,
		BillboardFixedVertical = 3,
		Mesh = 4
	};

	class Texts
	{
		public GUIContent renderMode = new GUIContent("Render Mode", "Defines the render mode of the particle renderer.");
		public GUIContent material = new GUIContent("Material", "Defines the material used to render particles.");
		public GUIContent mesh = new GUIContent("Mesh", "Defines the mesh that will be rendered as particle.");
		public GUIContent maxParticleSize = new GUIContent ("Max Particle Size", "How large is a particle allowed to be on screen at most? 1 is entire viewport. 0.5 is half viewport.");
		public GUIContent cameraSpeedScale = new GUIContent("Camera Scale", "How much the camera speed is factored in when determining particle stretching.");
		public GUIContent speedScale = new GUIContent("Speed Scale", "Defines the length of the particle compared to its speed.");
		public GUIContent lengthScale = new GUIContent("Length Scale", "Defines the length of the particle compared to its width.");
		public GUIContent sortingFudge = new GUIContent("Sorting Fudge", "Lower the number and most likely these particles will appear in front of other transparent objects, including other particles.");
		public GUIContent sortMode = new GUIContent("Sort Mode", "The draw order of particles can be sorted by distance, youngest first, or oldest first.");
		public GUIContent rotation = new GUIContent("Rotation", "Set whether the rotation of the particles is defined in Screen or World space.");
		public GUIContent castShadows = new GUIContent("Cast Shadows", "Only opaque materials cast shadows");
		public GUIContent receiveShadows = new GUIContent("Receive Shadows", "Only opaque materials receive shadows");
		public GUIContent normalDirection = new GUIContent("Normal Direction", "Value between 0.0 and 1.0. If 1.0 is used, normals will point towards camera. If 0.0 is used, normals will point out in the corner direction of the particle.");

		public string[] particleTypes = new string[] { "Billboard", "Stretched Billboard", "Horizontal Billboard", "Vertical Billboard", "Mesh" }; // Keep in sync with enum in ParticleSystemRenderer.h
		public string[] sortTypes = new string[] { "None", "By Distance", "Youngest First", "Oldest First" };
	}
	private static Texts s_Texts;

	public RendererModuleUI (ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ParticleSystemRenderer", displayName, VisibilityState.VisibleAndFolded)
	{
		m_ToolTip = "Specifies how the particles are rendered.";
	}


	protected override void Init()
	{
		if (m_CastShadows != null)
			return;
	
		m_CastShadows = GetProperty0 ("m_CastShadows");
		m_ReceiveShadows = GetProperty0("m_ReceiveShadows");
		m_Material = GetProperty0 ("m_Materials.Array.data[0]");
		m_RenderMode = GetProperty0 ("m_RenderMode");
		m_MaxParticleSize = GetProperty0 ("m_MaxParticleSize");			//< How large is a particle allowed to be on screen at most? 1 is entire viewport. 0.5 is half viewport.
		m_CameraVelocityScale = GetProperty0 ("m_CameraVelocityScale");	//< How much the camera motion is factored in when determining particle stretching.
		m_VelocityScale = GetProperty0 ("m_VelocityScale");				//< When Strech Particles is enabled, defines the length of the particle compared to its velocity.
		m_LengthScale = GetProperty0 ("m_LengthScale");					//< When Strech Particles is enabled, defines the length of the particle compared to its width.
		m_SortingFudge = GetProperty0 ("m_SortingFudge");				//< 
		m_SortMode = GetProperty0("m_SortMode");						//< 
		m_NormalDirection = GetProperty0 ("m_NormalDirection");


		m_Meshes[0] = GetProperty0("m_Mesh");
		m_Meshes[1] = GetProperty0("m_Mesh1");
		m_Meshes[2] = GetProperty0("m_Mesh2");
		m_Meshes[3] = GetProperty0("m_Mesh3");
		List<SerializedProperty> shownMeshes = new List<SerializedProperty>();
		for (int i = 0; i < m_Meshes.Length; ++i)
		{
			// Always show the first mesh
			if (i == 0 || m_Meshes[i].objectReferenceValue != null)
				shownMeshes.Add(m_Meshes[i]);
		}
		m_ShownMeshes = shownMeshes.ToArray();
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts ();

		RenderMode oldRenderMode = (RenderMode)m_RenderMode.intValue;
		RenderMode renderMode = (RenderMode)GUIPopup (s_Texts.renderMode, m_RenderMode, s_Texts.particleTypes);
		if (renderMode == RenderMode.Mesh) 
		{
			EditorGUI.indentLevel++;
			DoListOfMeshesGUI();
			EditorGUI.indentLevel--;
			
			if (oldRenderMode != RenderMode.Mesh && m_Meshes[0].objectReferenceInstanceIDValue == 0)
				m_Meshes[0].objectReferenceValue = Resources.GetBuiltinResource(typeof(Mesh), "Cube.fbx");
		}
		else if (renderMode == RenderMode.Stretch3D) 
		{
			EditorGUI.indentLevel++;
			GUIFloat(s_Texts.cameraSpeedScale, m_CameraVelocityScale);
			GUIFloat(s_Texts.speedScale, m_VelocityScale);
			GUIFloat(s_Texts.lengthScale, m_LengthScale);
			EditorGUI.indentLevel--;
		}

		if (renderMode != RenderMode.Mesh)
			GUIFloat(s_Texts.normalDirection, m_NormalDirection);

		if (m_Material != null) // The renderer's material list could be empty
			GUIObject(s_Texts.material, m_Material);
		GUIPopup(s_Texts.sortMode, m_SortMode, s_Texts.sortTypes);
		GUIFloat(s_Texts.sortingFudge, m_SortingFudge);
		GUIToggle(s_Texts.castShadows, m_CastShadows);
		GUIToggle(s_Texts.receiveShadows, m_ReceiveShadows);
		GUIFloat(s_Texts.maxParticleSize, m_MaxParticleSize);
		
	}
	
	private void DoListOfMeshesGUI ()
	{
		GUIListOfFloatObjectToggleFields(s_Texts.mesh, m_ShownMeshes, null, null, false);

		// Minus button
		Rect rect = GUILayoutUtility.GetRect(0, kSingleLineHeight); //GUILayoutUtility.GetLastRect();
		rect.x = rect.xMax - kPlusAddRemoveButtonWidth * 2 - kPlusAddRemoveButtonSpacing;
		rect.width = kPlusAddRemoveButtonWidth;
		if (m_ShownMeshes.Length > 1) 
		{
			if (MinusButton (rect))
			{
				m_ShownMeshes[m_ShownMeshes.Length - 1].objectReferenceValue = null;

				List<SerializedProperty> shownMeshes = new List<SerializedProperty>(m_ShownMeshes);			
				shownMeshes.RemoveAt (shownMeshes.Count-1);
				m_ShownMeshes = shownMeshes.ToArray();
			}
		}

		// Plus button
		if (m_ShownMeshes.Length < k_MaxNumMeshes)
		{
			rect.x += kPlusAddRemoveButtonWidth + kPlusAddRemoveButtonSpacing;
			if (PlusButton (rect))
			{
				List<SerializedProperty> shownMeshes = new List<SerializedProperty>(m_ShownMeshes);
				shownMeshes.Add ( m_Meshes[shownMeshes.Count] );
				m_ShownMeshes = shownMeshes.ToArray();
			}
		}
	}

	public bool IsMeshEmitter ()
	{
		if (m_RenderMode != null)
            return m_RenderMode.intValue == (int)RenderMode.Mesh;
		return false;
	}
}
} // namespace UnityEditor
