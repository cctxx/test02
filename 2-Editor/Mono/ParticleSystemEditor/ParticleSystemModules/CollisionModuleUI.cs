using UnityEngine;
using NUnit.Framework;
using System.Collections.Generic;



namespace UnityEditor
{

class CollisionModuleUI : ModuleUI
{
	// Keep in sync with CollisionModule.h
	const int k_MaxNumPlanes = 6;				
	enum CollisionTypes { Plane = 0, World = 1 }; 
	
	enum PlaneVizType {Grid, Solid};
	string[] m_PlaneVizTypeNames = {"Grid", "Solid"};
			
    SerializedProperty m_Type;
	SerializedProperty[] m_Planes = new SerializedProperty[k_MaxNumPlanes];
    SerializedProperty m_Dampen;
    SerializedProperty m_Bounce;
	SerializedProperty m_LifetimeLossOnCollision;
	SerializedProperty m_MinKillSpeed;
	SerializedProperty m_ParticleRadius;
	SerializedProperty m_CollidesWith;
	SerializedProperty m_Quality;
	SerializedProperty m_VoxelSize;
	SerializedProperty m_CollisionMessages;
	
	PlaneVizType m_PlaneVisualizationType = PlaneVizType.Solid; 	
	SerializedProperty[] m_ShownPlanes;
	float m_ScaleGrid = 1.0f;
	static Transform m_SelectedTransform; // static so to ensure only one selcted Transform across multiple particle systems

	class Texts
	{
		public GUIContent lifetimeLoss = new GUIContent("Lifetime Loss", "When particle collides, it will lose this fraction of its Start Lifetime");
		public GUIContent planes = new GUIContent("Planes", "Planes are defined by assigning a reference to a transform. This transform can be any transform in the scene and can be animated. Multiple planes can be used. Note: the Y-axis is used as the plane normal.");
		public GUIContent createPlane = new GUIContent("", "Create an empty GameObject and assign it as a plane.");
		public GUIContent minKillSpeed = new GUIContent("Min Kill Speed", "When particles collide and their speed is lower than this value, they are killed.");
		public GUIContent dampen = new GUIContent("Dampen", "When particle collides, it will lose this fraction of its speed. Unless this is set to 0.0, particle will become slower after collision.");
		public GUIContent bounce = new GUIContent("Bounce", "When particle collides, the bounce is scaled with this value. The bounce is the upwards motion in the plane normal direction.");
		public GUIContent particleRadius = new GUIContent("Particle Radius", "The estimated size of a particle when colliding (to avoid clipping with collision shape.");
		public GUIContent visualization = new GUIContent("Visualization", "Only used for visualizing the planes: Wireframe or Solid.");
		public GUIContent scalePlane = new GUIContent("Scale Plane", "Resizes the visualization planes.");
		public GUIContent collidesWith = new GUIContent("Collides With", "Collides the particles with colliders included in the layermask.");
		public GUIContent quality = new GUIContent("Collision Quality", "Quality of world collisions. Medium and low quality are approximate and may leak particles.");
		public string[] qualitySettings = { "High", "Medium", "Low" };
		public GUIContent voxelSize = new GUIContent("Voxel Size", "Size of voxels in the collision cache.");
		public GUIContent collisionMessages = new GUIContent("Send Collision Messages", "Send collision callback messages.");
	}
	private static Texts s_Texts;

	public CollisionModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "CollisionModule", displayName)
    {
		m_ToolTip = "Allows you to specify multiple collision planes that the particle can collide with.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Type != null)
			return;

		m_Type = GetProperty("type");
			
		List<SerializedProperty> shownPlanes = new List<SerializedProperty>();	
		for (int i=0; i<m_Planes.Length; ++i)
		{	
			m_Planes[i] = GetProperty("plane" + i);  // Keep name in sync with transfer func in CollisionModule.h
			Assert.That (m_Planes[i] != null);
				
			// Always show the first plane
			if (i==0 || m_Planes[i].objectReferenceValue != null)
				shownPlanes.Add (m_Planes[i]);	
		}
			
		m_ShownPlanes = shownPlanes.ToArray ();
				
		m_Dampen = GetProperty("dampen");
		m_Bounce = GetProperty("bounce");
		m_LifetimeLossOnCollision = GetProperty("energyLossOnCollision");
		m_MinKillSpeed = GetProperty("minKillSpeed");
		m_ParticleRadius = GetProperty("particleRadius");
		
		m_PlaneVisualizationType = (PlaneVizType)EditorPrefs.GetInt ("PlaneColisionVizType", (int)PlaneVizType.Solid);
		m_ScaleGrid = EditorPrefs.GetFloat ("ScalePlaneColision", 1f);

		m_CollidesWith = GetProperty("collidesWith");

		m_Quality = GetProperty("quality");

		m_VoxelSize = GetProperty("voxelSize");

		m_CollisionMessages = GetProperty("collisionMessages");
		
		SyncVisualization ();
	}
	
	protected override void SetVisibilityState (VisibilityState newState)
	{
		base.SetVisibilityState (newState);
			
		// Show tools again when module is not visible
		if (newState != VisibilityState.VisibleAndFoldedOut)
		{	
			Tools.s_Hidden = false;
			m_SelectedTransform = null;
			ParticleEffectUtils.ClearPlanes ();
		}
		else
		{
			SyncVisualization ();
		}
	}
		

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();
	
		string[] types = new string [] {"Planes", "World"};
		CollisionTypes type = (CollisionTypes)GUIPopup("", m_Type, types);

		if (type == CollisionTypes.Plane)
		{			
			DoListOfPlanesGUI ();
			
			EditorGUI.BeginChangeCheck();	
			m_PlaneVisualizationType = (PlaneVizType)GUIPopup (s_Texts.visualization, (int)m_PlaneVisualizationType, m_PlaneVizTypeNames);
			if (EditorGUI.EndChangeCheck ())
			{
				EditorPrefs.SetInt ("PlaneColisionVizType", (int)m_PlaneVisualizationType);
				if (m_PlaneVisualizationType == PlaneVizType.Solid)
					SyncVisualization ();
				else
					ParticleEffectUtils.ClearPlanes ();
			}
			
			EditorGUI.BeginChangeCheck();
			m_ScaleGrid = GUIFloat(s_Texts.scalePlane, m_ScaleGrid, "f2");
			if (EditorGUI.EndChangeCheck())
			{
				m_ScaleGrid = Mathf.Max (0f, m_ScaleGrid);
				EditorPrefs.SetFloat ("ScalePlaneColision", m_ScaleGrid);
				SyncVisualization(); // apply scale
			}
		}
		else
		{
		}

		GUIFloat (s_Texts.dampen, m_Dampen);
		GUIFloat (s_Texts.bounce, m_Bounce);
		GUIFloat (s_Texts.lifetimeLoss, m_LifetimeLossOnCollision);
		GUIFloat (s_Texts.minKillSpeed, m_MinKillSpeed);

		if (type != CollisionTypes.World)
		{
			GUIFloat(s_Texts.particleRadius, m_ParticleRadius);
		}

		if (type == CollisionTypes.World)
		{			
			GUILayerMask(s_Texts.collidesWith, m_CollidesWith);
			GUIPopup(s_Texts.quality, m_Quality, s_Texts.qualitySettings);
			if (m_Quality.intValue>0)
			{
				GUIFloat(s_Texts.voxelSize, m_VoxelSize);
			}
		}

		GUIToggle(s_Texts.collisionMessages, m_CollisionMessages);
	}

	protected override void OnModuleEnable ()
	{
		base.OnModuleEnable ();
		SyncVisualization ();
	}
    
   

	protected override void OnModuleDisable ()
	{
		base.OnModuleDisable ();
		ParticleEffectUtils.ClearPlanes ();
	}
		
	private void SyncVisualization ()
	{
		if (!enabled)
			return;

		if (m_PlaneVisualizationType != PlaneVizType.Solid)
			return;
	
		for (int i=0; i<m_ShownPlanes.Length; ++i)
		{
			Object o = m_ShownPlanes[i].objectReferenceValue;
			if (o == null)
				continue;
			
			Transform transform = o as Transform;
			if (transform == null)
				continue;

			GameObject plane = ParticleEffectUtils.GetPlane (i);
			plane.transform.position = transform.position;
			plane.transform.rotation = transform.rotation;
			plane.transform.localScale = new Vector3 (m_ScaleGrid, m_ScaleGrid, m_ScaleGrid);							
		}	
	}

	private static GameObject CreateEmptyGameObject(string name, ParticleSystem parentOfGameObject)
	{
		GameObject go = new GameObject(name);
		if (go)
		{
			if (parentOfGameObject)
				go.transform.parent = parentOfGameObject.transform;
			return go;
		}
		return null;
		
	}

	private void DoListOfPlanesGUI ()
	{
		int buttonPressedIndex = GUIListOfFloatObjectToggleFields(s_Texts.planes, m_ShownPlanes, null, s_Texts.createPlane, true);	
		if (buttonPressedIndex >= 0)
		{
			GameObject go = CreateEmptyGameObject ("Plane Transform " + (buttonPressedIndex+1), m_ParticleSystemUI.m_ParticleSystem);
			go.transform.localPosition = new Vector3(0, 0, 10 + buttonPressedIndex); // ensure each plane is not at same pos
			go.transform.localEulerAngles =  (new Vector3(-90,0,0));				 // make the plane normal point towards the forward axis of the particle system

			m_ShownPlanes[buttonPressedIndex].objectReferenceValue = go;
			SyncVisualization();
		}

		// Minus button
		Rect rect = GUILayoutUtility.GetRect(0, EditorGUI.kSingleLineHeight); //GUILayoutUtility.GetLastRect();
		rect.x = rect.xMax - kPlusAddRemoveButtonWidth * 2 - kPlusAddRemoveButtonSpacing;
		rect.width = kPlusAddRemoveButtonWidth;
		if (m_ShownPlanes.Length > 1) 
		{
			if (MinusButton (rect))
			{
				m_ShownPlanes[m_ShownPlanes.Length - 1].objectReferenceValue = null;

				List<SerializedProperty> shownPlanes = new List<SerializedProperty>(m_ShownPlanes);			
				shownPlanes.RemoveAt (shownPlanes.Count-1);
				m_ShownPlanes = shownPlanes.ToArray ();
			}
		}			
			
		// Plus button
		if(m_ShownPlanes.Length < k_MaxNumPlanes)
		{
			rect.x += kPlusAddRemoveButtonWidth + kPlusAddRemoveButtonSpacing;
			if (PlusButton (rect))
			{
				List<SerializedProperty> shownPlanes = new List<SerializedProperty>(m_ShownPlanes);			
				shownPlanes.Add ( m_Planes[shownPlanes.Count] );
				m_ShownPlanes = shownPlanes.ToArray ();
			}
		}			
	}
	
	
		
	override public void OnSceneGUI (ParticleSystem s, InitialModuleUI initial)
	{
		Event evt = Event.current;
		EventType oldEventType = evt.type;

		// we want ignored mouse up events to check for dragging off of scene view
		if(evt.type == EventType.Ignore && evt.rawType == EventType.MouseUp)
			oldEventType = evt.rawType;				
			
		Color origCol = Handles.color;
		Color col = new Color (1, 1, 1, 0.5F);
		Handles.color = col;

		CollisionTypes type = (CollisionTypes)m_Type.intValue;
		if (type == CollisionTypes.Plane)
		{
			for (int i=0; i<m_ShownPlanes.Length; ++i)
			{
				Object o = m_ShownPlanes[i].objectReferenceValue;
				if (o != null)
				{
					Transform transform = o as Transform;
					if (transform != null)
					{
						Vector3 position = transform.position;
						Quaternion rotation = transform.rotation;
						Vector3 right = rotation * Vector3.right;
						Vector3 up = rotation * Vector3.up;
						Vector3 forward = rotation * Vector3.forward;
							
						if (Object.ReferenceEquals (m_SelectedTransform, transform))
						{
							Tools.s_Hidden = true;
							EditorGUI.BeginChangeCheck();
							if (Tools.current == Tool.Move)	
								transform.position = Handles.PositionHandle (position, rotation);	
							else if (Tools.current == Tool.Rotate)
								transform.rotation = Handles.RotationHandle (rotation, position);	
							if (EditorGUI.EndChangeCheck ())
							{
								if (m_PlaneVisualizationType == PlaneVizType.Solid)
								{
									GameObject plane = ParticleEffectUtils.GetPlane (i);
									plane.transform.position = position;
									plane.transform.rotation = rotation;
									plane.transform.localScale = new Vector3 (m_ScaleGrid, m_ScaleGrid, m_ScaleGrid);							
								}
								ParticleSystemEditorUtils.PerformCompleteResimulation();
							}
							
						}
						else
						{
							int oldKeyboardControl = GUIUtility.keyboardControl;
							float handleSize = HandleUtility.GetHandleSize(position) * 0.06f;
								
							Handles.FreeMoveHandle(position, Quaternion.identity, handleSize, Vector3.zero, Handles.RectangleCap);
							
							// Detect selected plane (similar to TreeEditor)
							if(oldEventType == EventType.MouseDown && evt.type == EventType.Used && oldKeyboardControl != GUIUtility.keyboardControl)
							{
								m_SelectedTransform = transform;
								oldEventType = EventType.Used;
							}								
						}
							
						if (m_PlaneVisualizationType == PlaneVizType.Grid)
						{
							Color color = Handles.s_ColliderHandleColor * 0.9f;
							if (!enabled)
								color = new Color(0.7f, 0.7f, 0.7f, 0.7f);
							DrawGrid (position, right, forward, up, color, i);	
						}
						else
						{
							DrawSolidPlane (position, rotation, i);
						}
					}
					else
					{
						Debug.LogError ("Not a transform: " + o.GetType ());
					}
				}
			}
		}

		Handles.color = origCol;
	}
	
	
	void DrawSolidPlane (Vector3 pos, Quaternion rot, int planeIndex)
	{
		// Do nothing... planes are rendered as part of the scene
	}
		
	void DrawGrid (Vector3 pos, Vector3 axis1, Vector3 axis2, Vector3 normal, Color color, int planeIndex)
	{
		if (Event.current.type != EventType.repaint)
			return;
		
		HandleUtility.handleWireMaterial.SetPass (0);

		if (color.a > 0)
		{
			GL.Begin (GL.LINES);
			
			float lineLength = 10f;
			int numLines = 11;

			lineLength *= m_ScaleGrid;
			numLines = (int)lineLength;
			numLines = Mathf.Clamp (numLines, 10, 40);
			if (numLines % 2 == 0)
				numLines++;				
											
			float halfLength = lineLength * 0.5f;
			
			float distBetweenLines = lineLength / (numLines -1); 
			Vector3 v1 = axis1 * lineLength;
			Vector3 v2 = axis2 * lineLength;
			Vector3 dist1 = axis1 * distBetweenLines;
			Vector3 dist2 = axis2 * distBetweenLines;
			Vector3 startPos = pos - axis1 * halfLength - axis2 * halfLength;
				
			for (int i = 0; i < numLines; i++) 
			{
				if (i%2 == 0)
					GL.Color (color*0.7f);	
				else
					GL.Color (color);
					
				// Axis1
				GL.Vertex (startPos + i*dist1);
				GL.Vertex (startPos + i*dist1 + v2);					
				
				// Axis2
				GL.Vertex (startPos + i*dist2);
				GL.Vertex (startPos + i*dist2 + v1);					
			}
			
			GL.Color (color);
			GL.Vertex (pos);
			GL.Vertex (pos + normal);					
				
				
			GL.End ();		
		}	
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		text += "\n\tCollision is enabled.";
	}
}

} // namespace UnityEditor
