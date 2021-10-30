using UnityEngine;
using UnityEditor;
using UnityEditorInternal;


namespace UnityEditor
{
	[CustomEditor(typeof(SkinnedCloth))]
	class ClothInspector : Editor
	{
        bool[] m_Selection;
        bool[] m_RectSelection;
        int m_MouseOver = -1;
        int m_DrawMode = 1;
        int m_MeshVerticesPerSelectionVertex = 0;
        Mesh m_SelectionMesh;
        Mesh m_VertexMesh;
        Vector3[] m_LastVertices;
		Vector2 m_SelectStartPoint;
		Vector2 m_SelectMousePoint;
		bool m_RectSelecting = false;
		bool m_DidSelect = false;
		bool m_PaintMaxDistanceEnabled = false;
		bool m_PaintMaxDistanceBiasEnabled = false;
		bool m_PaintCollisionSphereRadiusEnabled = false;
		bool m_PaintCollisionSphereDistanceEnabled = false;
		float m_PaintMaxDistance = 0.2f;
		float m_PaintMaxDistanceBias = 0.0f;
		float m_PaintCollisionSphereRadius = 0.5f;
		float m_PaintCollisionSphereDistance = 0.0f;
		static Material s_SelectionMaterial = null;
        enum RectSelectionMode { Replace, Add, Substract};
        RectSelectionMode m_RectSelectionMode = RectSelectionMode.Add;
        const int clothToolID = 1200; //Unique tool number for cloth tool
		enum ToolMode { Select, Paint, Settings };
		static ToolMode s_ToolMode = ToolMode.Settings;
		static int maxVertices;

		static GUIContent[] s_ToolIcons = {
			EditorGUIUtility.IconContent ("ClothInspector.SelectTool", "Select vertices and edit their cloth coefficients in the inspector."), 
			EditorGUIUtility.IconContent ("ClothInspector.PaintTool", "Paint cloth coefficients on to vertices."), 
			EditorGUIUtility.IconContent ("ClothInspector.SettingsTool", "Set cloth options.")
		};
		static GUIContent s_ViewIcon = EditorGUIUtility.IconContent ("ClothInspector.ViewValue", "Visualize this vertex coefficient value in the scene view.");
		static GUIContent s_PaintIcon = EditorGUIUtility.IconContent ("ClothInspector.PaintValue", "Change this vertex coefficient value by painting in the scene view.");

        bool SelectionMeshDirty()
        {
            SkinnedCloth cloth = (SkinnedCloth)target;
			SkinnedMeshRenderer smr = cloth.GetComponent<SkinnedMeshRenderer>();
            Vector3[] vertices = cloth.vertices;
 			Quaternion rotation = smr.actualRootBone.rotation;
			Vector3 position = smr.actualRootBone.position;
            for (int i = 0; i < m_LastVertices.Length; i++)
            {
                if (m_LastVertices[i] != rotation * vertices[i] + position)
                    return true;
            }
            return false;
        }

        void GenerateSelectionMesh()
        {
            SkinnedCloth cloth = (SkinnedCloth)target;
			SkinnedMeshRenderer smr = cloth.GetComponent<SkinnedMeshRenderer>();
            Vector3[] vertices = cloth.vertices;
            if (m_SelectionMesh != null)
                DestroyImmediate(m_SelectionMesh);
            m_SelectionMesh = new Mesh();
            m_SelectionMesh.hideFlags |= HideFlags.DontSave;
            CombineInstance[] combine = new CombineInstance[vertices.Length];
            m_MeshVerticesPerSelectionVertex = m_VertexMesh.vertices.Length;
            m_LastVertices = new Vector3[vertices.Length];
			Quaternion rotation = smr.actualRootBone.rotation;
			Vector3 position = smr.actualRootBone.position;
            for (int i = 0; i < vertices.Length && i < maxVertices; i++)
            {
                m_LastVertices[i] = rotation * vertices[i] + position;
                combine[i].mesh = m_VertexMesh;
                combine[i].transform = Matrix4x4.TRS(m_LastVertices[i], Quaternion.identity, 0.015f * Vector3.one);
            }
            m_SelectionMesh.CombineMeshes(combine);
            SetupMeshColors();
        }

        void OnEnable()
        {
            if (s_SelectionMaterial == null)
                s_SelectionMaterial = EditorGUIUtility.LoadRequired("SceneView/VertexSelectionMaterial.mat") as Material;
 
            SkinnedCloth cloth = (SkinnedCloth)target;

            ClothSkinningCoefficient[] coefficients = cloth.coefficients;
            m_Selection = new bool[coefficients.Length];
            m_RectSelection = new bool[coefficients.Length];
            m_VertexMesh = (Mesh)Resources.GetBuiltinResource(typeof(Mesh), "Cube.fbx");
			maxVertices = 65536 / m_VertexMesh.vertices.Length;
			if (cloth.vertices.Length >= maxVertices)
			{
				Debug.LogWarning ("The mesh has too many vertices to be able to edit all skin coefficients. Only the first "+maxVertices+" vertices will be displayed");
			}
            GenerateSelectionMesh();
        }

        float GetCoefficient(ClothSkinningCoefficient coefficient)
        {
            switch (m_DrawMode)
            {
                case 1:
                    return coefficient.maxDistance;

                case 2:
                    return coefficient.maxDistanceBias;

                case 3:
                    return coefficient.collisionSphereRadius;

                case 4:
                    return coefficient.collisionSphereDistance;
                
            }
            return 0;
        }

        void SetupMeshColors()
        {
           SkinnedCloth cloth = (SkinnedCloth)target;
           ClothSkinningCoefficient[] coefficients = cloth.coefficients;
           Color[] colors = new Color[m_SelectionMesh.vertices.Length];
           float min = 0;
           float max = 0;
           for (int i = 0; i < coefficients.Length; i++)
           {
               float value = GetCoefficient(coefficients[i]);
               if (value < min)
                   min = value;
               if (value > max)
                   max = value;
           }
           for (int i = 0; i < coefficients.Length && i < maxVertices; i++)
           {
               for (int j = 0; j < m_MeshVerticesPerSelectionVertex; j++)
               {
                   Color color;
                   bool selected = m_Selection[i];
                   if (m_RectSelecting)
                   {
                   		switch (m_RectSelectionMode)
                   		{
                   			case RectSelectionMode.Replace:
                   				selected = m_RectSelection[i];
                   				break;
                   			case RectSelectionMode.Add:
                   				selected |= m_RectSelection[i];
                   				break;
                   			case RectSelectionMode.Substract:
                   				selected = selected && !m_RectSelection[i];
                   				break;
                   		}
                   }
                   
                   if (selected)
                       color = Color.red;
                   else
                   {
						float val;
						if (max - min != 0)
							val = (GetCoefficient(coefficients[i]) - min) / (max - min);
						else
							val = 0.5f;
						if (val < 0.5f)
							color = Color.Lerp(Color.green, Color.yellow, 2 * val);
						else
							color = Color.Lerp(Color.yellow, Color.blue, 2 * val - 1);
                   }
                   colors[i * m_MeshVerticesPerSelectionVertex + j] = color;
               }
           }
           m_SelectionMesh.colors = colors;
        }

        void OnDisable()
        {
             DestroyImmediate(m_SelectionMesh);
        }

        float CoefficientField(string label, float value, bool enabled, int mode)
        {
            bool oldEnabled = GUI.enabled;
            GUILayout.BeginHorizontal();
            if (GUILayout.Toggle(m_DrawMode == mode, s_ViewIcon, "MiniButton", GUILayout.ExpandWidth(false)))
            {
                m_DrawMode = mode;
                SetupMeshColors();
            }
            GUI.enabled = enabled;
            float retVal = EditorGUILayout.FloatField(label, value);
            GUI.enabled = oldEnabled;
            GUILayout.EndHorizontal();
            return retVal;
        }

        float PaintField(string label, float value, ref bool enabled, int mode)
        {
            bool oldEnabled = GUI.enabled;
            GUILayout.BeginHorizontal();
            if (GUILayout.Toggle(m_DrawMode == mode, s_ViewIcon, "MiniButton", GUILayout.ExpandWidth(false)))
            {
                m_DrawMode = mode;
                SetupMeshColors();
            }
			enabled = GUILayout.Toggle(enabled, s_PaintIcon, "MiniButton", GUILayout.ExpandWidth(false));
			GUI.enabled = enabled;
            float retVal = EditorGUILayout.FloatField(label, value);
            GUI.enabled = oldEnabled;
            GUILayout.EndHorizontal();
            return retVal;
        }

		void SelectionGUI ()
		{
			SkinnedCloth cloth = (SkinnedCloth)target;
			Vector3[] vertices = cloth.vertices;
            ClothSkinningCoefficient[] coefficients = cloth.coefficients;

            GUILayout.BeginHorizontal();
			if (GUILayout.Button("Select All"))
			{
				for (int i = 0; i < vertices.Length; i++)
					m_Selection[i] = true;
                SetupMeshColors();
				SceneView.RepaintAll();
			}
			
			if (GUILayout.Button("Select None"))
			{
				for (int i = 0; i < vertices.Length; i++)
					m_Selection[i] = false;
                SetupMeshColors();
				SceneView.RepaintAll();
			}           
			
		    float maxDistance = 0;
            float maxDistanceBias = 0;
            float collisionSphereRadius = 0;
            float collisionSphereDistance = 0;
            int numSelection = 0;

            for (int i = 0; i < coefficients.Length; i++)
            {
                if (m_Selection[i])
                {
                    maxDistance += coefficients[i].maxDistance;
                    maxDistanceBias += coefficients[i].maxDistanceBias;
                    collisionSphereRadius += coefficients[i].collisionSphereRadius;
                    collisionSphereDistance += coefficients[i].collisionSphereDistance;
                    numSelection++;
                }
            }
            GUILayout.Label(numSelection + " selected");
            GUILayout.EndHorizontal();
            GUILayout.Space(5);

            if (numSelection > 0)
            {
                maxDistance /= numSelection;
                maxDistanceBias /= numSelection;
                collisionSphereRadius /= numSelection;
                collisionSphereDistance /= numSelection;
            }

            float maxDistanceNew =  CoefficientField("max Distance", maxDistance, numSelection > 0, 1);
            float maxDistanceBiasNew =  CoefficientField("distance bias", maxDistanceBias, numSelection > 0, 2);
            float collisionSphereRadiusNew =  CoefficientField("collsionSphereRadius", collisionSphereRadius, numSelection > 0, 3);
            float collisionSphereDistanceNew =  CoefficientField("collisionSphereDistance", collisionSphereDistance, numSelection > 0, 4); 
			maxDistanceBiasNew = Mathf.Clamp(maxDistanceBiasNew, -1, 1);
			
            if (maxDistanceNew != maxDistance)
            {
                for (int i = 0; i < coefficients.Length; i++)
                {
                    if (m_Selection[i])
                        coefficients[i].maxDistance = maxDistanceNew;
                }
                cloth.coefficients = coefficients;
                SetupMeshColors();
            }
            if (maxDistanceBiasNew != maxDistanceBias)
            {
                for (int i = 0; i < coefficients.Length; i++)
                {
                    if (m_Selection[i])
                        coefficients[i].maxDistanceBias = maxDistanceBiasNew;
                }
                cloth.coefficients = coefficients;
                SetupMeshColors();
            }
            if (collisionSphereRadiusNew != collisionSphereRadius)
            {
                for (int i = 0; i < coefficients.Length; i++)
                {
                    if (m_Selection[i])
                        coefficients[i].collisionSphereRadius = collisionSphereRadiusNew;
                }
                cloth.coefficients = coefficients;
                SetupMeshColors();
            }
            if (collisionSphereDistanceNew != collisionSphereDistance)
            {
                for (int i = 0; i < coefficients.Length; i++)
                {
                    if (m_Selection[i])
                        coefficients[i].collisionSphereDistance = collisionSphereDistanceNew;
                }
                cloth.coefficients = coefficients;
                SetupMeshColors();
            }
		}
		
		void PaintGUI () 
		{
			m_PaintMaxDistance = PaintField("max Distance", m_PaintMaxDistance, ref m_PaintMaxDistanceEnabled, 1);
            m_PaintMaxDistanceBias = PaintField("distance bias", m_PaintMaxDistanceBias, ref m_PaintMaxDistanceBiasEnabled, 2);
  			m_PaintMaxDistanceBias = Mathf.Clamp(m_PaintMaxDistanceBias, -1, 1);
			m_PaintCollisionSphereRadius = PaintField("collsionSphereRadius", m_PaintCollisionSphereRadius, ref m_PaintCollisionSphereRadiusEnabled, 3);
	        m_PaintCollisionSphereDistance = PaintField("collisionSphereDistance", m_PaintCollisionSphereDistance, ref m_PaintCollisionSphereDistanceEnabled, 4);
	    }
		
        public override void OnInspectorGUI()
		{
			ToolMode oldToolMode = s_ToolMode;
			
			// If someone has changed the tool from the main tool bar
			if (Tools.current != Tool.None)
				s_ToolMode = ToolMode.Settings;
	 		s_ToolMode = (ToolMode)GUILayout.Toolbar ((int)s_ToolMode, s_ToolIcons);
			if (s_ToolMode != oldToolMode)
			{
				// delselect text, so we don't end up having a text field highlighted in the new tab
				GUIUtility.keyboardControl = 0;
				if (s_ToolMode != ToolMode.Settings)
					Tools.current = Tool.None;
                SceneView.RepaintAll();		
				SetupMeshColors ();
			}	
			switch (s_ToolMode)
			{
				case ToolMode.Select:
		            SelectionGUI();
					break;

				case ToolMode.Paint:
		            PaintGUI();
					break;

				case ToolMode.Settings:
		            DrawDefaultInspector();
					break;
			}
 		}

        int GetMouseVertex(Event e)
        {
            SkinnedCloth cloth = (SkinnedCloth)target;
			SkinnedMeshRenderer smr = cloth.GetComponent<SkinnedMeshRenderer>();
            Vector3[] normals = cloth.normals;
            ClothSkinningCoefficient[] coefficients = cloth.coefficients;
            Ray mouseRay = HandleUtility.GUIPointToWorldRay(e.mousePosition);
            float minDistance = 1000;
            int found = -1;
			Quaternion rotation = smr.actualRootBone.rotation;
			bool selectBackFacing = false;
			if (SceneView.lastActiveSceneView != null && SceneView.lastActiveSceneView.renderMode == DrawCameraMode.Wireframe)
				selectBackFacing = true;
            for (int i = 0; i < coefficients.Length; i++)
            {
                Vector3 dir = m_LastVertices[i] - mouseRay.origin;
                float sqrDistance = Vector3.Cross(dir, mouseRay.direction).sqrMagnitude;
                bool forwardFacing = Vector3.Dot(rotation * normals[i], Camera.current.transform.forward) <= 0;
                if ((forwardFacing || selectBackFacing) && sqrDistance < minDistance && sqrDistance < 0.05f * 0.05f)
                {
                    minDistance = sqrDistance;
                    found = i;
                }
            }
            return found;
        }

        void DrawVertices()
        {
            if (SelectionMeshDirty())
                GenerateSelectionMesh();

            for (int i=0; i<s_SelectionMaterial.passCount ; i++)
            {
                s_SelectionMaterial.SetPass(i);
                Graphics.DrawMeshNow(m_SelectionMesh, Matrix4x4.identity);
            }

            if (m_MouseOver != -1)
            {
                //SkinnedCloth cloth = (SkinnedCloth)target;
                
                Matrix4x4 m = Matrix4x4.TRS(m_LastVertices[m_MouseOver], Quaternion.identity, 0.02f * Vector3.one);
                s_SelectionMaterial.color = m_SelectionMesh.colors[m_MouseOver * m_MeshVerticesPerSelectionVertex];
                for (int i = 0; i < s_SelectionMaterial.passCount; i++)
                {
                    s_SelectionMaterial.SetPass(i);
                    Graphics.DrawMeshNow(m_VertexMesh, m);
                }
                s_SelectionMaterial.color = Color.white;
            }
        }

		bool UpdateRectSelection()
		{
			bool selectionChanged = false;
            SkinnedCloth cloth = (SkinnedCloth)target;
			SkinnedMeshRenderer smr = cloth.GetComponent<SkinnedMeshRenderer>();
			Vector3[] normals = cloth.normals;
			ClothSkinningCoefficient[] coefficients = cloth.coefficients;
            
            float minX = Mathf.Min(m_SelectStartPoint.x, m_SelectMousePoint.x);
            float maxX = Mathf.Max(m_SelectStartPoint.x, m_SelectMousePoint.x);
            float minY = Mathf.Min(m_SelectStartPoint.y, m_SelectMousePoint.y);
            float maxY = Mathf.Max(m_SelectStartPoint.y, m_SelectMousePoint.y);
            Ray topLeft = HandleUtility.GUIPointToWorldRay(new Vector2(minX, minY));
            Ray topRight = HandleUtility.GUIPointToWorldRay(new Vector2(maxX, minY));
            Ray botLeft = HandleUtility.GUIPointToWorldRay(new Vector2(minX, maxY));
            Ray botRight = HandleUtility.GUIPointToWorldRay(new Vector2(maxX, maxY));

            Plane top = new Plane(topRight.origin + topRight.direction, topLeft.origin + topLeft.direction, topLeft.origin);
            Plane bottom = new Plane(botLeft.origin + botLeft.direction, botRight.origin + botRight.direction, botRight.origin);
            Plane left = new Plane(topLeft.origin + topLeft.direction, botLeft.origin + botLeft.direction, botLeft.origin);
            Plane right = new Plane(botRight.origin + botRight.direction, topRight.origin + topRight.direction, topRight.origin);
            
			Quaternion rotation = smr.actualRootBone.rotation;

			bool selectBackFacing = false;
			if (SceneView.lastActiveSceneView != null && SceneView.lastActiveSceneView.renderMode == DrawCameraMode.Wireframe)
				selectBackFacing = true;
            for (int i = 0; i < coefficients.Length; i++)
            {
				Vector3 v = m_LastVertices[i];
				bool forwardFacing = Vector3.Dot(rotation * normals[i], Camera.current.transform.forward) <= 0;
				bool selected = top.GetSide(v) && bottom.GetSide(v) && left.GetSide(v) && right.GetSide(v);
				selected = selected && (selectBackFacing || forwardFacing);
				if (m_RectSelection[i] != selected)
                {
	                m_RectSelection[i] = selected;
	                selectionChanged = true;
                }
            }
            return selectionChanged;
		}
		
		void ApplyRectSelection()
		{
            SkinnedCloth cloth = (SkinnedCloth)target;
            ClothSkinningCoefficient[] coefficients = cloth.coefficients;
            
            for (int i = 0; i < coefficients.Length; i++)
            {
	            switch (m_RectSelectionMode)
	            {
	            	case RectSelectionMode.Replace:
		            	m_Selection[i] = m_RectSelection[i];
		            	break;

	            	case RectSelectionMode.Add:
		            	m_Selection[i] |= m_RectSelection[i];
		            	break;

	            	case RectSelectionMode.Substract:
		            	m_Selection[i] = m_Selection[i] && !m_RectSelection[i];
		            	break;
	            }		
            }
		}
		
		bool RectSelectionModeFromEvent ()
		{
			Event e = Event.current;
			RectSelectionMode mode = RectSelectionMode.Replace;
			if (e.shift)
				mode = RectSelectionMode.Add;
			if (e.alt)
				mode = RectSelectionMode.Substract;
			if (m_RectSelectionMode != mode)
			{
				m_RectSelectionMode = mode;
				return true;
			}
			return false;
		}
		
	    internal void SendCommandsOnModifierKeys () 
	    {
			SceneView.lastActiveSceneView.SendEvent (EditorGUIUtility.CommandEvent("ModifierKeysChanged"));
	    }

		void SelectionPreSceneGUI (int id)
		{
			Event e = Event.current;
			switch (e.GetTypeForControl (id))
            {
               case EventType.MouseDown:
                    if (e.alt || e.control || e.command || e.button != 0)
                        break;
					GUIUtility.hotControl = id;
                    int found = GetMouseVertex(e);
                    if (found != -1)
                    {
                        if (e.shift)
                            m_Selection[found] = !m_Selection[found];
                        else
                        {
                            for (int i = 0; i < m_Selection.Length; i++)
                                m_Selection[i] = false;
                            m_Selection[found] = true;
                        }
                        m_DidSelect = true;
                        SetupMeshColors();
                        Repaint();
                    }
                    else
                		m_DidSelect = false;

           			m_SelectStartPoint = e.mousePosition;
           			e.Use();
                    break;

                case EventType.MouseDrag:
            		if (GUIUtility.hotControl == id)
            		{
	 	 				if (!m_RectSelecting && (e.mousePosition - m_SelectStartPoint).magnitude > 2f)
	 	 				{
		                    if (!(e.alt || e.control || e.command))
		                    {
	       						EditorApplication.modifierKeysChanged += SendCommandsOnModifierKeys;
								m_RectSelecting = true;
								RectSelectionModeFromEvent();
								SetupMeshColors();
		                    }
	 	 				}
						if (m_RectSelecting)
						{
							m_SelectMousePoint = new Vector2(Mathf.Max(e.mousePosition.x, 0), Mathf.Max(e.mousePosition.y, 0));
							if (RectSelectionModeFromEvent() || UpdateRectSelection ())
		                        SetupMeshColors();
							e.Use ();               	
						}
            		}
                	break;
                
		 		case EventType.ExecuteCommand:
					if (m_RectSelecting && e.commandName == "ModifierKeysChanged")
					{
						if (RectSelectionModeFromEvent() || UpdateRectSelection ())
	                        SetupMeshColors();	
					}
					break;
					
               case EventType.MouseUp:
              		if (GUIUtility.hotControl == id && e.button == 0)
					{
						GUIUtility.hotControl = 0;
	
						if (m_RectSelecting)
						{
							EditorApplication.modifierKeysChanged -= SendCommandsOnModifierKeys;                
							m_RectSelecting = false;
							RectSelectionModeFromEvent();
							ApplyRectSelection();
						}
						else if (!m_DidSelect)
						{
							if (!(e.alt || e.control || e.command))
							{
								// If nothing was clicked, deselect all
					            SkinnedCloth cloth = (SkinnedCloth)target;
					            ClothSkinningCoefficient[] coefficients = cloth.coefficients;
					            for (int i = 0; i < coefficients.Length; i++)
					                m_Selection[i] = false;
							}
						}
	                    SetupMeshColors();
	                    Repaint();
					}
                	break;
            }  
		}
		
 		void PaintPreSceneGUI (int id)
		{
			Event e = Event.current;
			EventType type = e.GetTypeForControl (id);
			if (type == EventType.MouseDown || type == EventType.MouseDrag)
			{
	            SkinnedCloth cloth = (SkinnedCloth)target;
	            ClothSkinningCoefficient[] coefficients = cloth.coefficients;
				if (GUIUtility.hotControl != id && (e.alt || e.control || e.command || e.button != 0))
					return;
				if (type == EventType.MouseDown)
					GUIUtility.hotControl = id;
				int found = GetMouseVertex(e);
				if (found != -1)
				{
					bool changed = false;
					if (m_PaintMaxDistanceEnabled && coefficients[found].maxDistance != m_PaintMaxDistance)
					{
						coefficients[found].maxDistance = m_PaintMaxDistance;
						changed = true;
					}
					if (m_PaintMaxDistanceBiasEnabled && coefficients[found].maxDistanceBias != m_PaintMaxDistanceBias)
					{
						coefficients[found].maxDistanceBias = m_PaintMaxDistanceBias;
						changed = true;
					}
					if (m_PaintCollisionSphereRadiusEnabled && coefficients[found].collisionSphereRadius != m_PaintCollisionSphereRadius)
					{
						coefficients[found].collisionSphereRadius = m_PaintCollisionSphereRadius;
						changed = true;
					}
					if (m_PaintCollisionSphereDistanceEnabled && coefficients[found].collisionSphereDistance != m_PaintCollisionSphereDistance)
					{
						coefficients[found].collisionSphereDistance = m_PaintCollisionSphereDistance;
						changed = true;
					}
					if (changed)
					{
						cloth.coefficients = coefficients;
                        SetupMeshColors();
                        Repaint();
					}
				}
				e.Use();
			}
			else if (type == EventType.MouseUp)
			{
          		if (GUIUtility.hotControl == id && e.button == 0)
          		{
					GUIUtility.hotControl = 0;
					e.Use();
          		}
			}
		}
		
		public void OnPreSceneGUI ()
        {
     		if (s_ToolMode == ToolMode.Settings)
				return;
				
			Handles.BeginGUI ();
			int id = GUIUtility.GetControlID (FocusType.Passive);

			Event e = Event.current;
			switch (e.GetTypeForControl (id))
            {
		 		case EventType.Layout:
					HandleUtility.AddDefaultControl (id);
					break;
	
                case EventType.MouseMove:
                    int oldMouseOver = m_MouseOver;
                    m_MouseOver = GetMouseVertex(e);
                    if (m_MouseOver != oldMouseOver)
                        SceneView.RepaintAll();
                    break;
            }  
            
 	   		switch (s_ToolMode)
			{
				case ToolMode.Select:
		            SelectionPreSceneGUI(id);
					break;

				case ToolMode.Paint:
		            PaintPreSceneGUI(id);
					break;
			}
   			Handles.EndGUI ();  		

        }
        
        public void OnSceneGUI ()
        {
    		if (s_ToolMode == ToolMode.Settings)
				return;

			if (Event.current.type == EventType.Repaint)
				DrawVertices();

	 		Handles.BeginGUI ();
			if (m_RectSelecting && s_ToolMode == ToolMode.Select && Event.current.type == EventType.Repaint)
				EditorStyles.selectionRect.Draw(EditorGUIExt.FromToRect(m_SelectStartPoint, m_SelectMousePoint), GUIContent.none, false, false, false, false);
	 		Handles.EndGUI ();
       }
	}	
}


