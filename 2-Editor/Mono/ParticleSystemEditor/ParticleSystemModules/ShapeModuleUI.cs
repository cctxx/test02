using UnityEngine;

namespace UnityEditor
{

class ShapeModuleUI : ModuleUI
{
	SerializedProperty m_Type;
	SerializedProperty m_RandomDirection;
	
	// primitive properties
	SerializedProperty m_Radius;
	SerializedProperty m_Angle;
	SerializedProperty m_Length;
	SerializedProperty m_BoxX;
	SerializedProperty m_BoxY;
	SerializedProperty m_BoxZ;
	
	// mesh properties
	SerializedProperty m_PlacementMode;
	SerializedProperty m_Mesh;

	// internal
	private Material m_Material;	
	private static int s_BoxHash = "BoxColliderEditor".GetHashCode();
	private BoxEditor m_BoxEditor = new BoxEditor(true, s_BoxHash);
	private static Color s_ShapeGizmoColor = new Color (148f/255f, 229f/255f, 1f, 0.9f);
	
	// Keep in sync with enum in ShapeModule
	enum ShapeTypes { Sphere, SphereShell, HemiSphere, HemiSphereShell, Cone, Box, Mesh, ConeShell, ConeVolume, ConeVolumeShell };
	
	private string[] m_GuiNames = new string [] {"Sphere", "HemiSphere", "Cone", "Box", "Mesh"};
	private ShapeTypes[] m_GuiTypes = new [] {ShapeTypes.Sphere, ShapeTypes.HemiSphere, ShapeTypes.Cone,ShapeTypes.Box, ShapeTypes.Mesh};
	private int[] m_TypeToGuiTypeIndex = new [] {0,0,1,1,2,3,4,2,2,2};

	class Texts
	{
		public GUIContent shape = new GUIContent("Shape", "Defines the shape of the volume from which particles can be emitted, and the direction of the start velocity.");
		public GUIContent radius = new GUIContent ("Radius", "Radius of the shape.");
		public GUIContent coneAngle = new GUIContent("Angle", "Angle of the cone.");
		public GUIContent coneLength = new GUIContent("Length", "Length of the cone.");
		public GUIContent boxX = new GUIContent("Box X", "Scale of the box in X Axis.");
		public GUIContent boxY = new GUIContent("Box Y", "Scale of the box in Y Axis.");
		public GUIContent boxZ = new GUIContent("Box Z", "Scale of the box in Z Axis.");
		public GUIContent mesh = new GUIContent("Mesh", "Mesh that the particle system will emit from.");
		public GUIContent randomDirection = new GUIContent("Random Direction", "Randomizes the starting direction of particles.");
		public GUIContent emitFromShell = new GUIContent("Emit from Shell", "Emit from shell of the sphere. If disabled particles will be emitted from the volume of the shape.");
		public GUIContent emitFrom = new GUIContent("Emit from:", "Specifies from where particles are emitted.");
	}
	static Texts s_Texts = new Texts();

	public ShapeModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ShapeModule", displayName, VisibilityState.VisibleAndFolded)
	{
		m_ToolTip = "Shape of the emitter volume, which controls where particles are emitted and their initial direction.";
	}
	
	protected override void Init ()
	{
		if (m_Type != null)
			return;

		if (s_Texts == null)
			s_Texts = new Texts();

		m_Type = GetProperty("type");
		m_Radius = GetProperty("radius");
		m_Angle = GetProperty("angle");
		m_Length = GetProperty("length");
		m_BoxX = GetProperty("boxX");
		m_BoxY = GetProperty("boxY");
		m_BoxZ = GetProperty("boxZ");

		m_PlacementMode = GetProperty("placementMode");
		m_Mesh = GetProperty("m_Mesh");
		m_RandomDirection = GetProperty("randomDirection");

		// @TODO: Use something that uses vertex color + alpha and is transparent (Particles/Alpha blended does this, but need builtin material for it)
		m_Material = EditorGUIUtility.GetBuiltinExtraResource (typeof (Material), "Default-Diffuse.mat") as Material;

		m_BoxEditor.SetAlwaysDisplayHandles(true);		
	}

	public override float GetXAxisScalar()
	{
		return m_ParticleSystemUI.GetEmitterDuration();
	}

	private ShapeTypes ConvertConeEmitFromToConeType(int emitFrom)
	{
		if (emitFrom == 0)
			return ShapeTypes.Cone;
		else if (emitFrom == 1)
			return ShapeTypes.ConeShell;
		else if (emitFrom == 2)
			return ShapeTypes.ConeVolume;
		else
			return ShapeTypes.ConeVolumeShell;
	}

	private int ConvertConeTypeToConeEmitFrom(ShapeTypes shapeType)
	{
		if (shapeType == ShapeTypes.Cone)
			return 0;
		if (shapeType == ShapeTypes.ConeShell)
			return 1;
		if(shapeType == ShapeTypes.ConeVolume)
			return 2;
		if(shapeType == ShapeTypes.ConeVolumeShell)
			return 3;
			
		return 0;
	}
	
	private bool GetUsesShell(ShapeTypes shapeType)
	{
		if((shapeType == ShapeTypes.HemiSphereShell) ||
			(shapeType == ShapeTypes.SphereShell) ||
			(shapeType == ShapeTypes.ConeShell) ||
			(shapeType == ShapeTypes.ConeVolumeShell))
			return true;
	
		return false;
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		int type = m_Type.intValue;
		int index = m_TypeToGuiTypeIndex[(int)type];
		
		bool wasUsingShell = GetUsesShell((ShapeTypes)type);
		int index2 = GUIPopup(s_Texts.shape, index, m_GuiNames);
		ShapeTypes guiType = m_GuiTypes [index2];
		if (index2 != index)
		{
			type = (int)guiType;
		}

		switch (guiType)
		{
			case ShapeTypes.Box:
			{
				GUIFloat(s_Texts.boxX, m_BoxX);
				GUIFloat(s_Texts.boxY, m_BoxY);
				GUIFloat(s_Texts.boxZ, m_BoxZ);
			}
			break;
			
			case ShapeTypes.Cone:
			{
				GUIFloat(s_Texts.coneAngle, m_Angle);
				GUIFloat(s_Texts.radius, m_Radius);
				bool showLength = !((type == (int)ShapeTypes.ConeVolume) || (type == (int)ShapeTypes.ConeVolumeShell));
				EditorGUI.BeginDisabledGroup(showLength); 
					GUIFloat(s_Texts.coneLength, m_Length);
				EditorGUI.EndDisabledGroup();

				string[] types = new string[] { "Base", "Base Shell", "Volume", "Volume Shell" };

				int emitFrom = ConvertConeTypeToConeEmitFrom((ShapeTypes)type);
				emitFrom = GUIPopup(s_Texts.emitFrom, emitFrom, types);
				type = (int)ConvertConeEmitFromToConeType(emitFrom);
			}
			break;

			case ShapeTypes.Mesh:
			{
				string[] types = new string [] {"Vertex", "Edge", "Triangle"};
				GUIPopup ("", m_PlacementMode, types);
				GUIObject (s_Texts.mesh, m_Mesh);
			}
			break;
			
			case ShapeTypes.Sphere:
			{
				// sphere
				GUIFloat(s_Texts.radius, m_Radius);
				bool useShellEmit = GUIToggle(s_Texts.emitFromShell, wasUsingShell);
				type = (int)(useShellEmit ? ShapeTypes.SphereShell : ShapeTypes.Sphere);
			}
			break;
	
			case ShapeTypes.HemiSphere:
			{
				// sphere
				GUIFloat(s_Texts.radius, m_Radius);	
				bool useShellEmit = GUIToggle (s_Texts.emitFromShell, wasUsingShell);
				type = (int)(useShellEmit ? ShapeTypes.HemiSphereShell : ShapeTypes.HemiSphere);
			}
			break;
		}

		m_Type.intValue = type;

		GUIToggle(s_Texts.randomDirection, m_RandomDirection);
	}

	override public void OnSceneGUI (ParticleSystem s, InitialModuleUI initial)
	{
		Color origCol = Handles.color;
		Handles.color = s_ShapeGizmoColor;

		Matrix4x4 orgMatrix = Handles.matrix;
		Matrix4x4 scaleMatrix = new Matrix4x4();
		scaleMatrix.SetTRS(s.transform.position, s.transform.rotation, s.transform.lossyScale);
		Handles.matrix = scaleMatrix;

		EditorGUI.BeginChangeCheck();
		int type = m_Type.intValue;
		
		if (type == (int)ShapeTypes.Sphere || type == (int)ShapeTypes.SphereShell)
		{
			m_Radius.floatValue = Handles.DoSimpleRadiusHandle(Quaternion.identity, Vector3.zero, m_Radius.floatValue, false);
		}
		else if (type == (int)ShapeTypes.HemiSphere || type == (int)ShapeTypes.HemiSphereShell)
		{
			m_Radius.floatValue = Handles.DoSimpleRadiusHandle(Quaternion.identity, Vector3.zero, m_Radius.floatValue, true);
		}
		else if ((type == (int)ShapeTypes.Cone) || (type == (int)ShapeTypes.ConeShell))
		{
			Vector3 radiusAngleRange = new Vector3(m_Radius.floatValue, m_Angle.floatValue, initial.m_Speed.scalar.floatValue);
			radiusAngleRange = Handles.ConeFrustrumHandle(Quaternion.identity, Vector3.zero, radiusAngleRange);
			m_Radius.floatValue = radiusAngleRange.x;
			m_Angle.floatValue = radiusAngleRange.y;
			initial.m_Speed.scalar.floatValue = radiusAngleRange.z;
		}
		else if ((type == (int)ShapeTypes.ConeVolume) || (type == (int)ShapeTypes.ConeVolumeShell))
		{
			Vector3 radiusAngleLength = new Vector3(m_Radius.floatValue, m_Angle.floatValue, m_Length.floatValue);
			radiusAngleLength = Handles.ConeFrustrumHandle(Quaternion.identity, Vector3.zero, radiusAngleLength);
			m_Radius.floatValue = radiusAngleLength.x;
			m_Angle.floatValue = radiusAngleLength.y;
			m_Length.floatValue = radiusAngleLength.z;
		}
		else if (type == (int)ShapeTypes.Box)
		{
			Vector3 center = Vector3.zero;
			Vector3 size = new Vector3 (m_BoxX.floatValue, m_BoxY.floatValue, m_BoxZ.floatValue);
			if (m_BoxEditor.OnSceneGUI (scaleMatrix, s_ShapeGizmoColor, false, ref center, ref size))
			{
				m_BoxX.floatValue = size.x;
				m_BoxY.floatValue = size.y;
				m_BoxZ.floatValue = size.z;
			}
		}
		else if (type == (int)ShapeTypes.Mesh) 
		{
			Mesh mesh = (Mesh)m_Mesh.objectReferenceValue;
			if (mesh)
			{
				bool orgWireframeMode = ShaderUtil.wireframeMode;
				ShaderUtil.wireframeMode = true;
				m_Material.SetPass (0);
				Graphics.DrawMeshNow (mesh, s.transform.localToWorldMatrix);
				ShaderUtil.wireframeMode = orgWireframeMode;
			}
		}
		if (EditorGUI.EndChangeCheck())
			m_ParticleSystemUI.m_ParticleEffectUI.m_Owner.Repaint();

		Handles.color = origCol;
		Handles.matrix = orgMatrix;
	}
}

} // namespace UnityEditor
