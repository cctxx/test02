
/*
GUILayout.TextureGrid number of horiz elements doesnt work
*/

using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor
{
internal enum TerrainTool
{
	None = -1,
	PaintHeight = 0,
	SetHeight,
	SmoothHeight,
	PaintTexture,
	PlaceTree,
	PaintDetail,
	TerrainSettings,
	TerrainToolCount
}

internal class SplatPainter
{
	public int size;
	public float strength;
	public Brush brush;
	public float target;
	public TerrainData terrainData;
	public TerrainTool tool;
	
	float ApplyBrush (float height, float brushStrength)
	{
		if (target > height)
		{
			height += brushStrength;
			height = Mathf.Min (height, target);
			return height;
		}
		else
		{
			height -= brushStrength;
			height = Mathf.Max (height, target);
			return height;
		}
	}
	
	// Normalize the alpha map at pixel x,y.
	// The alpha of splatIndex will be maintained, while all others will be made to fit
	void Normalize (int x, int y, int splatIndex, float[,,] alphamap)
	{
		float newAlpha = alphamap[y,x,splatIndex];
		float totalAlphaOthers = 0.0F;
		int alphaMaps = alphamap.GetLength (2);
		for (int a=0;a<alphaMaps;a++)
		{
			if (a != splatIndex)
				totalAlphaOthers += alphamap[y,x,a];
		}
		
		if (totalAlphaOthers > 0.01)
		{
			float adjust = (1.0F - newAlpha) / totalAlphaOthers;
			for (int a=0;a<alphaMaps;a++)
			{
				if (a != splatIndex)
					alphamap[y,x,a] *= adjust;
			}
		}
		else
		{
			for (int a=0;a<alphaMaps;a++)
			{
				alphamap[y,x,a] = a == splatIndex ? 1.0F : 0.0F;
			}
		}
	}

	public void Paint (float xCenterNormalized, float yCenterNormalized, int splatIndex)
	{
		if (splatIndex >= terrainData.alphamapLayers)
			return;
		int xCenter = Mathf.FloorToInt(xCenterNormalized * terrainData.alphamapWidth);
		int yCenter = Mathf.FloorToInt(yCenterNormalized * terrainData.alphamapHeight);
	
		int intRadius = Mathf.RoundToInt(size) / 2;
		int intFraction = Mathf.RoundToInt(size) % 2;
		
		int xmin = Mathf.Clamp(xCenter - intRadius, 0, terrainData.alphamapWidth - 1);
		int ymin = Mathf.Clamp(yCenter - intRadius, 0, terrainData.alphamapHeight - 1);

		int xmax = Mathf.Clamp(xCenter + intRadius + intFraction, 0, terrainData.alphamapWidth);
		int ymax = Mathf.Clamp(yCenter + intRadius + intFraction, 0, terrainData.alphamapHeight);
		
		int width = xmax - xmin;
		int height = ymax - ymin;
		
		float[,,] alphamap = terrainData.GetAlphamaps (xmin, ymin, width, height);
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				int xBrushOffset = (xmin + x) - (xCenter - intRadius + intFraction);
				int yBrushOffset = (ymin + y) - (yCenter - intRadius + intFraction);
				float brushStrength = brush.GetStrengthInt (xBrushOffset, yBrushOffset);
				
				// Paint with brush
				float newAlpha = ApplyBrush (alphamap[y,x,splatIndex], brushStrength * strength);
				alphamap[y,x,splatIndex] = newAlpha;
				Normalize(x,y,splatIndex,alphamap);
			}
		}

		terrainData.SetAlphamaps (xmin, ymin, alphamap);
	}
}

internal class DetailPainter
{
	public int size;	
	public float opacity;
	public float targetStrength;
	public Brush brush;
	public TerrainData terrainData;
	public TerrainTool tool;
	public bool randomizeDetails;
	public bool clearSelectedOnly;

	public void Paint (float xCenterNormalized, float yCenterNormalized, int detailIndex)
	{
		if (detailIndex >= terrainData.detailPrototypes.Length)
			return;
			
		int xCenter = Mathf.FloorToInt(xCenterNormalized * terrainData.detailWidth);
		int yCenter = Mathf.FloorToInt(yCenterNormalized * terrainData.detailHeight);
	
		int intRadius = Mathf.RoundToInt(size) / 2;
		int intFraction = Mathf.RoundToInt(size) % 2;
		
		int xmin = Mathf.Clamp(xCenter - intRadius, 0, terrainData.detailWidth - 1);
		int ymin = Mathf.Clamp(yCenter - intRadius, 0, terrainData.detailHeight - 1);

		int xmax = Mathf.Clamp(xCenter + intRadius + intFraction, 0, terrainData.detailWidth);
		int ymax = Mathf.Clamp(yCenter + intRadius + intFraction, 0, terrainData.detailHeight);
		
		int width = xmax - xmin;
		int height = ymax - ymin;
		
		int[] layers = { detailIndex };
		if (targetStrength < 0.0F && !clearSelectedOnly)
			layers = terrainData.GetSupportedLayers(xmin, ymin, width, height);			

		for (int i=0;i<layers.Length;i++)
		{
			int[,] alphamap = terrainData.GetDetailLayer (xmin, ymin, width, height, layers[i]);
			
			for (int y=0;y<height;y++)
			{
				for (int x=0;x<width;x++)
				{
					int xBrushOffset = (xmin + x) - (xCenter - intRadius + intFraction);
					int yBrushOffset = (ymin + y) - (yCenter - intRadius + intFraction);
					float opa = opacity * brush.GetStrengthInt (xBrushOffset, yBrushOffset);;

					float t = targetStrength;
					float targetValue = Mathf.Lerp (alphamap[y,x], t, opa);
					alphamap[y,x] = Mathf.RoundToInt (targetValue - .5f + Random.value);
				}
			}
	
			terrainData.SetDetailLayer (xmin, ymin, layers[i], alphamap);
		}
	}
}

internal class TreePainter
{
	public static float brushSize = 40;
	public static float spacing = .8f;
	
	public static float treeColorAdjustment = .4f;
	public static float treeWidth = 1;
	public static float treeHeight = 1;
	public static float treeWidthVariation = .1f;
	public static float treeHeightVariation = .1f;
	public static int selectedTree = 0;
	public static Terrain terrain;
	
	static Color GetTreeColor ()
	{
		Color c = Color.white * Random.Range(1.0F, 1.0F - treeColorAdjustment);
		c.a = 1;
		return c;
	}

	static float GetTreeWidth ()
	{
		return treeWidth * Random.Range(1.0F - treeWidthVariation, 1.0F + treeWidthVariation);
	}

	static float GetTreeHeight ()
	{
		return treeHeight * Random.Range(1.0F - treeHeightVariation, 1.0F + treeHeightVariation);
	}

	public static void PlaceTrees (float xBase, float yBase)
	{
		if (terrain.terrainData.treePrototypes.Length == 0)
			return;
		selectedTree = Mathf.Min(TerrainInspectorUtil.GetPrototypeCount(terrain.terrainData) - 1, selectedTree);

		if (!TerrainInspectorUtil.PrototypeHasMaterials(terrain.terrainData, selectedTree))
			return;

		int placedTreeCount = 0;
		
		// Plant a single tree first. At the location of the mouse
		TreeInstance instance = new TreeInstance();
		instance.position = new Vector3 (xBase,0,yBase);
		instance.color = GetTreeColor();
		instance.lightmapColor = Color.white;
		instance.prototypeIndex = selectedTree;
		instance.widthScale = GetTreeWidth();
		instance.heightScale = GetTreeHeight();

		// When painting single tree 
		// And just clicking we always place it, so you can do overlapping trees

		bool checkTreeDistance = Event.current.type == EventType.MouseDrag || brushSize > 1;
		if (!checkTreeDistance || TerrainInspectorUtil.CheckTreeDistance(terrain.terrainData, instance.position, instance.prototypeIndex, spacing))
		{
			terrain.AddTreeInstance(instance);
			placedTreeCount++;
		}

		Vector3 size = TerrainInspectorUtil.GetPrototypeExtent(terrain.terrainData, selectedTree);
		size.y = 0;
		float treeCountOneAxis = brushSize / (size.magnitude * spacing * .5f);
		int treeCount = (int)((treeCountOneAxis * treeCountOneAxis) * .5f);
		treeCount = Mathf.Clamp(treeCount, 0, 100);
		// Plant a bunch of trees
		for (int i=1;i<treeCount && placedTreeCount < treeCount;i++)
		{
			Vector2 randomOffset = Random.insideUnitCircle;
			randomOffset.x *= brushSize / terrain.terrainData.size.x;
			randomOffset.y *= brushSize / terrain.terrainData.size.z;
			Vector3 position = new Vector3 (xBase + randomOffset.x, 0, yBase + randomOffset.y);
			if (TerrainInspectorUtil.CheckTreeDistance(terrain.terrainData, position, selectedTree, spacing * .5f))
			{
				instance = new TreeInstance();
				
				instance.position = position;

				instance.color = GetTreeColor();
				instance.lightmapColor = Color.white;
				instance.prototypeIndex = selectedTree;
				instance.widthScale = GetTreeWidth();
				instance.heightScale = GetTreeHeight();
				
				terrain.AddTreeInstance(instance);
				placedTreeCount++;
			}
		}
	}

	// Calculate the size of the brush with which we are painting trees
	float GetTreePlacementSize (float treeCount)
	{
		return TerrainInspectorUtil.GetTreePlacementSize(terrain.terrainData, selectedTree, spacing, treeCount);
	}	
	
	public static void RemoveTrees (float xBase, float yBase, bool clearSelectedOnly)
	{	
		float radius = brushSize / terrain.terrainData.size.x;
		terrain.RemoveTrees(new Vector2 (xBase, yBase), radius, clearSelectedOnly ? selectedTree : -1);
	}
	
}

internal class HeightmapPainter
{
	public int size;
	public float strength;
	public float targetHeight;
	public TerrainTool tool;
	public Brush brush;
	public TerrainData terrainData;
	
	float Smooth (int x, int y)
	{
		float h = 0.0F;
		float normalizeScale = 1.0F / terrainData.size.y;
		h += terrainData.GetHeight(x, y) * normalizeScale;
		h += terrainData.GetHeight(x+1, y) * normalizeScale;
		h += terrainData.GetHeight(x-1, y) * normalizeScale;
		h += terrainData.GetHeight(x+1, y+1) * normalizeScale * 0.75F;
		h += terrainData.GetHeight(x-1, y+1) * normalizeScale * 0.75F;
		h += terrainData.GetHeight(x+1, y-1) * normalizeScale * 0.75F;
		h += terrainData.GetHeight(x-1, y-1) * normalizeScale * 0.75F;
		h += terrainData.GetHeight(x, y+1) * normalizeScale;
		h += terrainData.GetHeight(x, y-1) * normalizeScale;
		h /= 8.0F;
		return h;
	}

	float ApplyBrush (float height, float brushStrength, int x, int y)
	{
		if (tool == TerrainTool.PaintHeight)
			return height + brushStrength;
		else if (tool == TerrainTool.SetHeight)
		{
			if (targetHeight > height)
			{
				height += brushStrength;
				height = Mathf.Min (height, targetHeight);
				return height;
			}
			else
			{
				height -= brushStrength;
				height = Mathf.Max (height, targetHeight);
				return height;
			}
		}
		else if (tool == TerrainTool.SmoothHeight)
		{
			return Mathf.Lerp(height, Smooth(x, y), brushStrength);
		}
		else
			return height;
	}

	public void PaintHeight (float xCenterNormalized, float yCenterNormalized)
	{
		int xCenter, yCenter;
		if (size % 2 == 0)
		{
			xCenter = Mathf.CeilToInt(xCenterNormalized * (terrainData.heightmapWidth - 1));	
			yCenter = Mathf.CeilToInt(yCenterNormalized * (terrainData.heightmapHeight - 1));
		}
		else
		{
			xCenter = Mathf.RoundToInt(xCenterNormalized * (terrainData.heightmapWidth - 1));	
			yCenter = Mathf.RoundToInt(yCenterNormalized * (terrainData.heightmapHeight - 1));
		}
		
		int intRadius = size / 2;
		int intFraction = size % 2;

		int xmin = Mathf.Clamp(xCenter - intRadius, 0, terrainData.heightmapWidth - 1);
		int ymin = Mathf.Clamp(yCenter - intRadius, 0, terrainData.heightmapHeight - 1);

		int xmax = Mathf.Clamp(xCenter + intRadius + intFraction, 0, terrainData.heightmapWidth);
		int ymax = Mathf.Clamp(yCenter + intRadius + intFraction, 0, terrainData.heightmapHeight);
		
		int width = xmax - xmin;
		int height = ymax - ymin;

		float[,] heights = terrainData.GetHeights(xmin, ymin, width, height);
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				int xBrushOffset = (xmin + x) - (xCenter - intRadius);
				int yBrushOffset = (ymin + y) - (yCenter - intRadius);
				float brushStrength = brush.GetStrengthInt (xBrushOffset, yBrushOffset);
				//Debug.Log(xBrushOffset + ", " + yBrushOffset + "=" + brushStrength);
				float value = heights[y,x];
				value = ApplyBrush(value, brushStrength * strength, x + xmin, y + ymin);
				heights[y,x] = value;
			}
		}
		
		terrainData.SetHeightsDelayLOD(xmin, ymin, heights);
	}	
}

[CustomEditor(typeof(Terrain))]
internal class TerrainInspector : Editor {

	class Styles {
		public GUIStyle gridList = "GridList";
		public GUIStyle gridListText = "GridListText";
		public GUIStyle label = "RightLabel";
		public GUIStyle largeSquare = "Button";
		public GUIStyle command = "Command";
		public Texture settingsIcon = EditorGUIUtility.IconContent ("SettingsIcon").image;
		// List of tools supported by the editor
		public GUIContent[] toolIcons = {
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolRaise"), 
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolSetHeight"),
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolSmoothHeight"),
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolSplat"), 
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolTrees"),
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolPlants"),
			EditorGUIUtility.IconContent ("TerrainInspector.TerrainToolSettings")
		};

		public GUIContent[] toolNames = {
			EditorGUIUtility.TextContent ("TerrainInspector.RaiseHeightTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.PaintHeightTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.SmoothHeightTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.PaintTextureTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.PlaceTreesTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.PaintDetailsTip"),
			EditorGUIUtility.TextContent ("TerrainInspector.TerrainSettingsTip")
		};
	
		public GUIContent brushSize = EditorGUIUtility.TextContent ("TerrainInspector.BrushSize");	
		public GUIContent opacity = EditorGUIUtility.TextContent ("TerrainInspector.BrushOpacity");	
		public GUIContent settings = EditorGUIUtility.TextContent ("TerrainInspector.Settings");	
		public GUIContent brushes = EditorGUIUtility.TextContent ("TerrainInspector.Brushes");	

			
		// Textures
		public GUIContent textures = EditorGUIUtility.TextContent ("TerrainInspector.Textures.Textures");
		public GUIContent editTextures = EditorGUIUtility.TextContent ("TerrainInspector.Textures.Edit");
		// Trees
		public GUIContent trees = EditorGUIUtility.TextContent ("TerrainInspector.Trees.Trees");		
		public GUIContent noTrees = EditorGUIUtility.TextContent ("TerrainInspector.Trees.NoTrees");	
		public GUIContent editTrees = EditorGUIUtility.TextContent ("TerrainInspector.Trees.EditTrees");	
		public GUIContent treeDensity = EditorGUIUtility.TextContent ("TerrainInspector.Trees.TreeDensity");
		public GUIContent treeColorVar = EditorGUIUtility.TextContent ("TerrainInspector.Trees.ColorVar");	
		public GUIContent treeHeight = EditorGUIUtility.TextContent ("TerrainInspector.Trees.TreeHeight");
		public GUIContent treeHeightVar = EditorGUIUtility.TextContent ("TerrainInspector.Trees.TreeHeightVar");
		public GUIContent treeWidth = EditorGUIUtility.TextContent ("TerrainInspector.Trees.TreeWidth");
		public GUIContent treeWidthVar = EditorGUIUtility.TextContent ("TerrainInspector.Trees.TreeWidthVar");

		// Details
		public GUIContent details = EditorGUIUtility.TextContent ("TerrainInspector.Details.Details");
		public GUIContent editDetails = EditorGUIUtility.TextContent ("TerrainInspector.Details.Edit");
		public GUIContent detailTargetStrength = EditorGUIUtility.TextContent ("TerrainInspector.Details.TargetStrength");

		// Heightmaps
		public GUIContent heightmap = EditorGUIUtility.TextContent("TerrainInspector.Heightmaps.Heightmap");
		public GUIContent importRaw  = EditorGUIUtility.TextContent("TerrainInspector.Heightmaps.ImportRaw");
		public GUIContent exportRaw = EditorGUIUtility.TextContent("TerrainInspector.Heightmaps.ExportRaw");
		public GUIContent flatten = EditorGUIUtility.TextContent("TerrainInspector.Heightmaps.Flatten");

		public GUIContent resolution = EditorGUIUtility.TextContent("TerrainInspector.Resolution");
		public GUIContent refresh = EditorGUIUtility.TextContent("TerrainInspector.Refresh");
		public GUIContent massPlaceTrees = EditorGUIUtility.TextContent("TerrainInspector.MassPlaceTrees");
	}
	static Styles styles;

		static float PercentSlider (GUIContent content, float valueInPercent, float minVal, float maxVal) 
		{
			bool temp = GUI.changed;
			GUI.changed = false;
			float v = EditorGUILayout.Slider (content, Mathf.Round (valueInPercent * 100f), minVal * 100f, maxVal * 100f);
			if (GUI.changed)
			{
				return v / 100f;	
			}
			GUI.changed = temp;
			return valueInPercent;
		}
		internal static PrefKey[] s_ToolKeys = {
			new PrefKey ("Terrain/Raise Height", "#q"),
			new PrefKey ("Terrain/Set Height", "#w"),
			new PrefKey ("Terrain/Smooth Height", "#e"),
			new PrefKey ("Terrain/Texture Paint", "#r"),
			new PrefKey ("Terrain/Tree Brush", "#t"),
			new PrefKey ("Terrain/Detail Brush", "#y")
		};
		internal static PrefKey s_PrevBrush = new PrefKey ("Terrain/Previous Brush", ",");
		internal static PrefKey s_NextBrush = new PrefKey ("Terrain/Next Brush", ".");
		internal static PrefKey s_PrevTexture = new PrefKey ("Terrain/Previous Detail", "#,");
		internal static PrefKey s_NextTexture = new PrefKey ("Terrain/Next Detail", "#.");
	// Source terrain
	Terrain m_Terrain;
	
	
	GUIContent[]	m_TreeContents = null;
	GUIContent[]	m_DetailContents = null;

	SavedFloat m_TargetHeight = new SavedFloat ("TerrainBrushTargetHeight", 0.2F);
	SavedFloat m_Strength = new SavedFloat ("TerrainBrushStrength", 0.5F);
	SavedInt m_Size = new SavedInt ("TerrainBrushSize", 25);
	SavedFloat m_SplatAlpha = new SavedFloat ("TerrainBrushSplatAlpha", 1.0F);
	SavedFloat m_DetailOpacity = new SavedFloat ("TerrainDetailOpacity", 1.0F);
	SavedFloat m_DetailStrength = new SavedFloat ("TerrainDetailStrength", 0.8F);

	const float kHeightmapBrushScale = 0.01F;
	const float kMinBrushStrength = (1.1F / ushort.MaxValue) / kHeightmapBrushScale;
	Brush m_CachedBrush;

	// TODO: Make an option for letting the user add textures to the brush list.
	static Texture2D[]	s_BrushTextures = null;
	int			m_SelectedBrush = 0;
	int			m_SelectedSplat = 0;
	int			m_SelectedDetail = 0;
	static int s_TerrainEditorHash = "TerrainEditor".GetHashCode();
	
	void CheckKeys ()
	{
		if (GUIUtility.textFieldInput)
		{
			return;
		}

		for (int i = 0; i < s_ToolKeys.Length; i++)
		{
			if (s_ToolKeys[i].activated)
			{
				selectedTool = (TerrainTool)i;
				Repaint();
				Event.current.Use();
			}
		}

		if (s_PrevBrush.activated) 
		{
			m_SelectedBrush--;
			if (m_SelectedBrush < 0)
				m_SelectedBrush = s_BrushTextures.Length - 1;
			Repaint ();
			Event.current.Use ();
		}

		if (s_NextBrush.activated) 
		{
			m_SelectedBrush++;
			if (m_SelectedBrush >= s_BrushTextures.Length)
				m_SelectedBrush = 0;
			Repaint ();
			Event.current.Use ();
		}
		int delta = 0;
		if (s_NextTexture.activated)
			delta = 1;
		if (s_PrevTexture.activated)
		   delta = -1;
		
		if (delta != 0)
		{
			switch (selectedTool)
			{
			case TerrainTool.PaintDetail:
				m_SelectedDetail = (int)Mathf.Repeat (m_SelectedDetail + delta, m_Terrain.terrainData.detailPrototypes.Length);
				Event.current.Use ();
				Repaint ();
				break;
			case TerrainTool.PlaceTree:
				TreePainter.selectedTree = (int)Mathf.Repeat (TreePainter.selectedTree + delta, m_TreeContents.Length);
				Event.current.Use ();
				Repaint ();
				break;
			case TerrainTool.PaintTexture:
				m_SelectedSplat = (int)Mathf.Repeat (m_SelectedSplat + delta, m_Terrain.terrainData.splatPrototypes.Length);
				Event.current.Use ();
				Repaint ();
				break;
			}
		}
	}
		
	void LoadBrushIcons ()
	{
		// Load the textures;
		ArrayList arr = new ArrayList ();
		int idx = 1;
		Texture t = null;

		// Load brushes from editor resources
		do
		{
			t = (Texture2D)EditorGUIUtility.Load (EditorResourcesUtility.brushesPath + "builtin_brush_" + idx + ".png");
			if (t) arr.Add (t); 
			idx ++;
		}
		while (t);

		// Load user created brushes from the Assets/Gizmos folder
		idx = 0;
		do
		{
			t = EditorGUIUtility.FindTexture ("brush_" + idx + ".png");
			if (t) arr.Add (t); 
			idx ++;
		}
		while (t);
		
		s_BrushTextures = arr.ToArray(typeof(Texture2D)) as Texture2D[];
	}

	void Initialize ()
	{	
		m_Terrain = target as Terrain;
		// Load brushes
		if (s_BrushTextures == null)
			LoadBrushIcons ();
	}
	
	public void OnDisable ()
	{
		if (m_CachedBrush != null)
			m_CachedBrush.Dispose();
	}
	SavedInt m_SelectedTool = new SavedInt ("TerrainSelectedTool", (int)TerrainTool.PaintHeight);
	TerrainTool selectedTool
	{  
		get { 
			if (Tools.current == Tool.None)
				return (TerrainTool)m_SelectedTool.value;
			return TerrainTool.None;
		}
		set { 
			if (value != TerrainTool.None)
				Tools.current = Tool.None;
			m_SelectedTool.value = (int)value;
		}
	}

	static string IntString (float p)
	{
		int i = (int)p;
		return i.ToString();	
	}
	
	public void MenuButton (GUIContent title, string menuName, int userData)
	{
		GUIContent t = new GUIContent (title.text, styles.settingsIcon, title.tooltip);
		Rect r = GUILayoutUtility.GetRect (t, styles.largeSquare);
		if (GUI.Button (r, t, styles.largeSquare))
		{
			MenuCommand context = new MenuCommand(m_Terrain, userData);			
			EditorUtility.DisplayPopupMenu (new Rect (r.x, r.y, 0,0), menuName, context);
		}
	}

	public static int AspectSelectionGrid (int selected, Texture[] textures, int approxSize, GUIStyle style, string emptyString, out bool doubleClick)
	{
		GUILayout.BeginVertical ("box", GUILayout.MinHeight (10));
		int retval = 0;

		doubleClick = false;

		if (textures.Length != 0)  
		{
			float columns = (Screen.width - 20) / approxSize;
			int rows = (int)Mathf.Ceil (textures.Length / columns);
			Rect r = GUILayoutUtility.GetAspectRect (columns/rows);

			Event evt = Event.current;
			if (evt.type == EventType.MouseDown && evt.clickCount == 2 && r.Contains(evt.mousePosition))
			{
				doubleClick = true;
				evt.Use();
			}

			retval = GUI.SelectionGrid (r, selected, textures, (Screen.width - 20) / approxSize, style);
		} 
		else	
		{
			GUILayout.Label (emptyString);
		}

		GUILayout.EndVertical ();
		return retval;
	}

	static Rect GetBrushAspectRect (int elementCount, int approxSize, int extraLineHeight)
	{
		int xCount = (int)Mathf.Ceil ((Screen.width - 20) / approxSize);
		int yCount = elementCount / xCount;
		if (elementCount % xCount != 0)
			yCount++;
		Rect r1 = GUILayoutUtility.GetAspectRect (xCount / (float)yCount);
		Rect r2 = GUILayoutUtility.GetRect (10, extraLineHeight * yCount);
		r1.height += r2.height;
		return r1;
	}


	public static int AspectSelectionGridImageAndText(int selected, GUIContent[] textures, int approxSize, GUIStyle style, string emptyString, out bool doubleClick)
	{
		EditorGUILayout.BeginVertical(GUIContent.none, EditorStyles.helpBox, GUILayout.MinHeight(10));
		int retval = 0;

		doubleClick = false;

		if (textures.Length != 0)
		{
			Rect rect = GetBrushAspectRect(textures.Length, approxSize, 12);

			Event evt = Event.current;
			if (evt.type == EventType.MouseDown && evt.clickCount == 2 && rect.Contains(evt.mousePosition))
			{
				doubleClick = true;
				evt.Use();
			}

			retval = GUI.SelectionGrid(rect, selected, textures, (int)Mathf.Ceil((Screen.width - 20) / approxSize), style);
		}
		else
		{
			GUILayout.Label(emptyString);
		}

		GUILayout.EndVertical();
		return retval;
	}

	void LoadTreeIcons ()
	{
		// Locate the proto types asset preview textures
		TreePrototype[] trees = m_Terrain.terrainData.treePrototypes;
		
		m_TreeContents = new GUIContent[trees.Length];
		for (int i=0;i<m_TreeContents.Length;i++)
		{
			m_TreeContents[i] = new GUIContent();
			Texture tex = AssetPreview.GetAssetPreview (trees[i].prefab);
			if (tex != null)
				m_TreeContents[i].image = tex;

			if (trees[i].prefab != null)
				m_TreeContents[i].text = trees[i].prefab.name;
			else
				m_TreeContents[i].text = "Missing";
		}
	}					
	
	void LoadDetailIcons () 
	{
		// Locate the proto types asset preview textures
		DetailPrototype[] prototypes = m_Terrain.terrainData.detailPrototypes;
		m_DetailContents = new GUIContent[prototypes.Length];
		for (int i=0;i<m_DetailContents.Length;i++)
		{
			m_DetailContents[i] = new GUIContent();

			if (prototypes[i].usePrototypeMesh)
			{
				Texture tex = AssetPreview.GetAssetPreview (prototypes[i].prototype);
				if (tex != null)
					m_DetailContents[i].image = tex;
					
				if (prototypes[i].prototype != null)
					m_DetailContents[i].text = prototypes[i].prototype.name;
				else
					m_DetailContents[i].text = "Missing";
			}
			else
			{
				Texture tex = prototypes[i].prototypeTexture;
				if (tex != null)
					m_DetailContents[i].image = tex;
				if (tex != null)
					m_DetailContents[i].text = tex.name;
				else
					m_DetailContents[i].text = "Missing";
			}
		}		
	}


	public void ShowTrees ()
	{
		LoadTreeIcons ();

		// Tree picker
		GUI.changed = false;

		GUILayout.Label(styles.trees, EditorStyles.boldLabel);
		bool doubleClick;
		TreePainter.selectedTree = AspectSelectionGridImageAndText (TreePainter.selectedTree, m_TreeContents, 64, styles.gridListText, "No trees defined", out doubleClick);
		if (doubleClick)
		{
			TerrainTreeContextMenus.EditTree (new MenuCommand (m_Terrain, TreePainter.selectedTree));
			GUIUtility.ExitGUI ();
		}

		GUILayout.BeginHorizontal ();
		ShowMassPlaceTrees();
		GUILayout.FlexibleSpace ();
		MenuButton (styles.editTrees, "CONTEXT/TerrainEngineTrees", TreePainter.selectedTree);
		ShowRefreshPrototypes();
		GUILayout.EndHorizontal ();		

		GUILayout.Label (styles.settings, EditorStyles.boldLabel);
		// Placement distance
		TreePainter.brushSize = EditorGUILayout.Slider(styles.brushSize, TreePainter.brushSize, 1, 100); // former string formatting: ""
		float oldDens = (3.3f - TreePainter.spacing) / 3f;
		float newDens = PercentSlider (styles.treeDensity, oldDens, .1f, 1);
		// Only set spacing when value actually changes. Otherwise
		// it will lose precision because we're constantly doing math
		// back and forth with it.
		if (newDens != oldDens)
			TreePainter.spacing = (1.1f - newDens) * 3f;

		GUILayout.Space (5);

		// Color adjustment
		TreePainter.treeColorAdjustment = EditorGUILayout.Slider(styles.treeColorVar, TreePainter.treeColorAdjustment, 0, 1); // former string formatting: "%"

		GUILayout.Space (5);

		// height adjustment
		TreePainter.treeHeight = PercentSlider(styles.treeHeight, TreePainter.treeHeight, 0.5F, 2); 
		TreePainter.treeHeightVariation = PercentSlider(styles.treeHeightVar, TreePainter.treeHeightVariation, 0.0F, 0.3F); 

		GUILayout.Space (5);

		// width adjustment
		TreePainter.treeWidth = PercentSlider(styles.treeWidth, TreePainter.treeWidth, 0.5F, 2);
		TreePainter.treeWidthVariation = PercentSlider(styles.treeWidthVar, TreePainter.treeWidthVariation, 0.0F, 0.3F); 

		GUILayout.Space (5);
	}	


	public void ShowDetails ()
	{
		LoadDetailIcons ();
		ShowBrushes ();
		// Brush size
		
		// Detail picker
		GUI.changed = false;
	
		GUILayout.Label (styles.details, EditorStyles.boldLabel);
		bool doubleClick;
		m_SelectedDetail = AspectSelectionGridImageAndText (m_SelectedDetail, m_DetailContents, 64, styles.gridListText, "No Detail Objects defined", out doubleClick);
		if (doubleClick)
		{
			TerrainDetailContextMenus.EditDetail (new MenuCommand (m_Terrain, m_SelectedDetail));
			GUIUtility.ExitGUI ();
		}

		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();
		MenuButton (styles.editDetails, "CONTEXT/TerrainEngineDetails", m_SelectedDetail);
		ShowRefreshPrototypes();
		GUILayout.EndHorizontal ();

		GUILayout.Label(styles.settings, EditorStyles.boldLabel);

		// Brush size
		m_Size.value = Mathf.RoundToInt(EditorGUILayout.Slider(styles.brushSize, m_Size, 1, 100)); // former string formatting: ""
		m_DetailOpacity.value = EditorGUILayout.Slider(styles.opacity, m_DetailOpacity, 0, 1); // former string formatting: "%"
		
		// Strength
		m_DetailStrength.value = EditorGUILayout.Slider(styles.detailTargetStrength, m_DetailStrength, 0, 1); // former string formatting: "%"
		m_DetailStrength.value = Mathf.Round (m_DetailStrength * 16.0f) / 16.0f;
	}

	public void ShowSettings ()
	{
		TerrainData terrainData = m_Terrain.terrainData;

		EditorGUI.BeginChangeCheck ();
		
		GUILayout.Label ("Base Terrain", EditorStyles.boldLabel);
		m_Terrain.heightmapPixelError = EditorGUILayout.Slider("Pixel Error", m_Terrain.heightmapPixelError, 1, 200); // former string formatting: ""
		m_Terrain.basemapDistance = EditorGUILayout.Slider("Base Map Dist.", m_Terrain.basemapDistance, 0, 2000); // former string formatting: ""
		m_Terrain.castShadows = EditorGUILayout.Toggle ("Cast shadows",m_Terrain.castShadows);

		m_Terrain.materialTemplate = EditorGUILayout.ObjectField ("Material", m_Terrain.materialTemplate, typeof (Material), false) as Material;
        
		// Warn if shader needs tangent basis
        if (m_Terrain.materialTemplate != null)
		{
			Shader s = m_Terrain.materialTemplate.shader;
			if (ShaderUtil.HasTangentChannel(s))
			{
				GUIContent c = EditorGUIUtility.TextContent("TerrainInspector.ShaderWarning");
				EditorGUILayout.HelpBox(c.text, MessageType.Warning, false);
			}
		}
		
		EditorGUI.BeginChangeCheck ();
		PhysicMaterial tempPhysicMaterial = EditorGUILayout.ObjectField ("Physics Material", terrainData.physicMaterial, typeof (PhysicMaterial), false) as PhysicMaterial;
		if (EditorGUI.EndChangeCheck ())
		{
			terrainData.physicMaterial = tempPhysicMaterial;
		}

		GUILayout.Label ("Tree & Detail Objects", EditorStyles.boldLabel);
		m_Terrain.drawTreesAndFoliage = EditorGUILayout.Toggle ("Draw", m_Terrain.drawTreesAndFoliage);
		m_Terrain.detailObjectDistance = EditorGUILayout.Slider("Detail Distance", m_Terrain.detailObjectDistance, 0, 250); // former string formatting: ""
		m_Terrain.detailObjectDensity = EditorGUILayout.Slider ("Detail Density", m_Terrain.detailObjectDensity, 0.0f, 1.0f);
		m_Terrain.treeDistance = EditorGUILayout.Slider ("Tree Distance", m_Terrain.treeDistance, 0, 2000); // former string formatting: ""
		m_Terrain.treeBillboardDistance = EditorGUILayout.Slider("Billboard Start", m_Terrain.treeBillboardDistance, 5, 2000); // former string formatting: ""
		m_Terrain.treeCrossFadeLength = EditorGUILayout.Slider("Fade Length", m_Terrain.treeCrossFadeLength, 0, 200); // former string formatting: ""
		m_Terrain.treeMaximumFullLODCount = EditorGUILayout.IntSlider("Max Mesh Trees", m_Terrain.treeMaximumFullLODCount, 0, 10000);

		if (EditorGUI.EndChangeCheck ())
		{
			EditorApplication.SetSceneRepaintDirty();
			EditorUtility.SetDirty (m_Terrain);
		}
		
		EditorGUI.BeginChangeCheck ();
		
		GUILayout.Label ("Wind Settings", EditorStyles.boldLabel);
		float wavingGrassStrength = EditorGUILayout.Slider("Speed", terrainData.wavingGrassStrength, 0, 1); // former string formatting: "%"
		float wavingGrassSpeed = EditorGUILayout.Slider("Size", terrainData.wavingGrassSpeed, 0, 1); // former string formatting: "%"
		float wavingGrassAmount = EditorGUILayout.Slider("Bending", terrainData.wavingGrassAmount, 0, 1); // former string formatting: "%"
		Color wavingGrassTint = EditorGUILayout.ColorField ("Grass Tint", terrainData.wavingGrassTint);
		
		if (EditorGUI.EndChangeCheck ())
		{
			// Apply terrain settings only when something has changed. Otherwise we needlessly dirty the object and it will show up as modified.
			terrainData.wavingGrassStrength = wavingGrassStrength;
			terrainData.wavingGrassSpeed = wavingGrassSpeed;
			terrainData.wavingGrassAmount = wavingGrassAmount;
			terrainData.wavingGrassTint = wavingGrassTint;
		}

		ShowResolution();
		ShowHeightmaps();
	}
	
	public void ShowRaiseHeight ()
	{
		ShowBrushes ();
		GUILayout.Label (styles.settings, EditorStyles.boldLabel);
		ShowBrushSettings ();
		
	}
	
	public void ShowSmoothHeight ()
	{
		ShowBrushes ();
		GUILayout.Label (styles.settings, EditorStyles.boldLabel);
		ShowBrushSettings ();
	}

	public void ShowTextures ()
	{
		ShowBrushes ();
		
		GUILayout.Label (styles.textures, EditorStyles.boldLabel);
		SplatPrototype[] splats = m_Terrain.terrainData.splatPrototypes;
		Texture2D[] splatmaps = new Texture2D [splats.Length];
		for (int i = 0; i < splats.Length; i++) {
			splatmaps[i] = splats[i].texture;	
		}
		GUI.changed = false;
		bool doubleClick;
		m_SelectedSplat = AspectSelectionGrid(m_SelectedSplat, splatmaps, 64, styles.gridList, "No terrain textures defined.", out doubleClick);
		if (doubleClick)
		{
			TerrainSplatContextMenus.EditSplat (new MenuCommand (m_Terrain, m_SelectedSplat));
			GUIUtility.ExitGUI ();
		}

		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();
		MenuButton (styles.editTextures, "CONTEXT/TerrainEngineSplats", m_SelectedSplat);
		GUILayout.EndHorizontal ();

		// Brush size
		GUILayout.Label (styles.settings, EditorStyles.boldLabel);
		ShowBrushSettings ();
		m_SplatAlpha.value = EditorGUILayout.Slider("Target Strength", m_SplatAlpha, 0.0F, 1.0F); // former string formatting: "%"
	}
	
	public void ShowBrushes () 
	{
		GUILayout.Label (styles.brushes, EditorStyles.boldLabel);
		bool dummy;
		m_SelectedBrush = AspectSelectionGrid(m_SelectedBrush, s_BrushTextures, 32, styles.gridList, "No brushes defined.", out dummy);
	}

	public void ShowHeightmaps()
	{
		GUILayout.Label(styles.heightmap, EditorStyles.boldLabel);
		GUILayout.BeginHorizontal();
		GUILayout.FlexibleSpace();
		if (GUILayout.Button(styles.importRaw))
		{
			TerrainMenus.ImportRaw();
		}
		
		if (GUILayout.Button(styles.exportRaw))
		{
			TerrainMenus.ExportHeightmapRaw();
		}
		GUILayout.EndHorizontal();
	}

	public void ShowResolution()
	{
		GUILayout.Label("Resolution", EditorStyles.boldLabel);

		float terrainWidth = m_Terrain.terrainData.size.x;
		float terrainHeight = m_Terrain.terrainData.size.y;
		float terrainLength = m_Terrain.terrainData.size.z;
		int heightmapResolution = m_Terrain.terrainData.heightmapResolution;
		int detailResolution = m_Terrain.terrainData.detailResolution;
		int detailResolutionPerPatch = m_Terrain.terrainData.detailResolutionPerPatch;
		int controlTextureResolution = m_Terrain.terrainData.alphamapResolution;
		int baseTextureResolution = m_Terrain.terrainData.baseMapResolution;

		EditorGUI.BeginChangeCheck();

		terrainWidth = DelayedFloatField("Terrain Width", terrainWidth);
		if (terrainWidth <= 0) terrainWidth = 1;

		terrainLength = DelayedFloatField("Terrain Length", terrainLength);
		if (terrainLength <= 0) terrainLength = 1;

		terrainHeight = DelayedFloatField("Terrain Height", terrainHeight);
		if (terrainHeight <= 0) terrainHeight = 1;

		heightmapResolution = DelayedIntField("Heightmap Resolution", heightmapResolution);
		heightmapResolution = Mathf.Clamp(heightmapResolution, 33, 2049); // 33 is the minimum that GetAdjustedSize will allow, and 2049 is the highest resolution where the editor is still usable performance-wise
		heightmapResolution = m_Terrain.terrainData.GetAdjustedSize(heightmapResolution);

		detailResolution = DelayedIntField("Detail Resolution", detailResolution);
		detailResolution = Mathf.Clamp(detailResolution, 0, 4048);

		detailResolutionPerPatch = DelayedIntField("Detail Resolution Per Patch", detailResolutionPerPatch);
		detailResolutionPerPatch = Mathf.Clamp(detailResolutionPerPatch, 8, 128);

		controlTextureResolution = DelayedIntField("Control Texture Resolution", controlTextureResolution);
		controlTextureResolution = Mathf.Clamp(Mathf.ClosestPowerOfTwo(controlTextureResolution), 16, 2048);

		baseTextureResolution = DelayedIntField("Base Texture Resolution", baseTextureResolution);
		baseTextureResolution = Mathf.Clamp(Mathf.ClosestPowerOfTwo(baseTextureResolution), 16, 2048);

		if (EditorGUI.EndChangeCheck())
		{
			ArrayList undoObjects = new ArrayList();
			undoObjects.Add(m_Terrain.terrainData);
			undoObjects.AddRange(m_Terrain.terrainData.alphamapTextures);

			Undo.RegisterCompleteObjectUndo(undoObjects.ToArray(typeof(UnityEngine.Object)) as UnityEngine.Object[], "Set Resolution");

			if (m_Terrain.terrainData.heightmapResolution != heightmapResolution)
				m_Terrain.terrainData.heightmapResolution = heightmapResolution;
			m_Terrain.terrainData.size = new Vector3(terrainWidth, terrainHeight, terrainLength);

			if (m_Terrain.terrainData.detailResolution != detailResolution || detailResolutionPerPatch != m_Terrain.terrainData.detailResolutionPerPatch)
				ResizeDetailResolution(m_Terrain.terrainData, detailResolution, detailResolutionPerPatch);

			if (m_Terrain.terrainData.alphamapResolution != controlTextureResolution)
				m_Terrain.terrainData.alphamapResolution = controlTextureResolution;

			if (m_Terrain.terrainData.baseMapResolution != baseTextureResolution)
				m_Terrain.terrainData.baseMapResolution = baseTextureResolution;

			m_Terrain.Flush();
		}

		GUILayout.Label("* Please note that modifying the resolution will clear the heightmap, detail map or splatmap.", EditorStyles.wordWrappedLabel);
	}

	void ResizeDetailResolution(TerrainData terrainData, int resolution, int resolutionPerPatch)
	{
		if (resolution == terrainData.detailResolution)
		{
			var layers = new List<int[,]>();
			for (int i = 0; i < terrainData.detailPrototypes.Length; i++)
				layers.Add(terrainData.GetDetailLayer(0, 0, terrainData.detailWidth, terrainData.detailHeight, i));

			terrainData.SetDetailResolution(resolution, resolutionPerPatch);

			for (int i = 0; i < layers.Count; i++)
				terrainData.SetDetailLayer(0, 0, i, layers[i]);
		}
		else
		{
			terrainData.SetDetailResolution(resolution, resolutionPerPatch);
		}
	}

	internal float DelayedFloatField(string label, float value)
	{
		float oldVal = value;
		float newVal = oldVal;
		
		Rect position = EditorGUILayout.GetControlRect ();
		position = EditorGUI.PrefixLabel (position, EditorGUIUtility.TempContent (label));

		EditorGUI.BeginChangeCheck();
		string floatStr = EditorGUI.DelayedTextField (position, oldVal.ToString(), "inftynaeINFTYNAE0123456789.,-", EditorStyles.numberField);
		if (EditorGUI.EndChangeCheck())
		{
			if (float.TryParse(floatStr, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out newVal) && newVal != oldVal)
			{
				value = newVal;
				GUI.changed = true;
			}
		}
		return newVal;
	}

	internal int DelayedIntField(string label, int value)
	{
		int oldVal = value;
		int newVal = oldVal;

		Rect position = EditorGUILayout.GetControlRect ();
		position = EditorGUI.PrefixLabel (position, EditorGUIUtility.TempContent (label));

		EditorGUI.BeginChangeCheck();
		string intStr = EditorGUI.DelayedTextField (position, oldVal.ToString(), "0123456789-", EditorStyles.numberField);
		if (EditorGUI.EndChangeCheck())
		{
			if (int.TryParse (intStr, out newVal) && newVal != oldVal)
			{
				value = newVal;
				GUI.changed = true;
			}
		}
		return newVal;
	}

	public void ShowRefreshPrototypes()
	{
		if (GUILayout.Button(styles.refresh))
		{
			TerrainMenus.RefreshPrototypes();
		}
	}

	public void ShowMassPlaceTrees()
	{
		EditorGUI.BeginDisabledGroup(m_Terrain.terrainData.treePrototypes == null || m_Terrain.terrainData.treePrototypes.Length <= 0);
		if (GUILayout.Button(styles.massPlaceTrees))
		{
			TerrainMenus.MassPlaceTrees();
		}
		EditorGUI.EndDisabledGroup();
	}

	public void ShowBrushSettings ()
	{
		m_Size.value = Mathf.RoundToInt(EditorGUILayout.Slider (styles.brushSize, m_Size, 1, 100));
		m_Strength.value = PercentSlider (styles.opacity, m_Strength, kMinBrushStrength, 1); // former string formatting: "0.0%"
	}		
	
	public void ShowSetHeight()
	{
		ShowBrushes ();
		GUILayout.Label (styles.settings, EditorStyles.boldLabel);
		ShowBrushSettings ();

		GUILayout.BeginHorizontal();

		GUI.changed = false;
		float val = (m_TargetHeight * m_Terrain.terrainData.size.y);
		val = EditorGUILayout.Slider ("Height", val, 0, m_Terrain.terrainData.size.y);
		if (GUI.changed)
			m_TargetHeight.value = val / m_Terrain.terrainData.size.y;

		if (GUILayout.Button(styles.flatten, GUILayout.ExpandWidth(false)))
		{
			Undo.RegisterCompleteObjectUndo(m_Terrain.terrainData, "Flatten Heightmap");
			HeightmapFilters.Flatten(m_Terrain.terrainData, m_TargetHeight.value);
		}

		GUILayout.EndHorizontal();
	}
	
	void OnInspectorUpdate ()
	{
		if (AssetPreview.HasAnyNewPreviewTexturesAvailable ())
			Repaint ();
	}

	public override void OnInspectorGUI ()
	{
		Initialize ();

		if (styles == null)
		{
			styles = new Styles ();
		}

		if (!m_Terrain.terrainData)
		{
			GUI.enabled = false;
			GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			GUILayout.Toolbar (-1, styles.toolIcons, styles.command);
			GUILayout.FlexibleSpace ();
			GUILayout.EndHorizontal ();
			GUI.enabled = true;
			GUILayout.BeginVertical (EditorStyles.helpBox);
			GUILayout.Label ("Terrain Asset Missing");
			m_Terrain.terrainData = EditorGUILayout.ObjectField ("Assign:", m_Terrain.terrainData, typeof (TerrainData), false) as TerrainData;
			GUILayout.EndVertical ();
			return;
		}

		// Show the master tool selector
		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();
		GUI.changed = false;
		int tool = (int)selectedTool;


		selectedTool = (TerrainTool)GUILayout.Toolbar (tool, styles.toolIcons, styles.command);
		
		if ((int)selectedTool != tool)
		{
			if (Toolbar.get != null)
				Toolbar.get.Repaint ();
		}

		GUILayout.FlexibleSpace ();
		GUILayout.EndHorizontal ();
	
		CheckKeys ();		
		
		GUILayout.BeginVertical (EditorStyles.helpBox);
		if (tool >= 0 && tool < styles.toolIcons.Length)
		{
			GUILayout.Label (styles.toolNames[(int)tool].text);
			GUILayout.Label (styles.toolNames[(int)tool].tooltip, EditorStyles.wordWrappedMiniLabel);
		}
		else
		{
			// TODO: Fix these somehow sensibly
			GUILayout.Label ("No tool selected");
			GUILayout.Label ("Please select a tool", EditorStyles.wordWrappedMiniLabel);
		}
		GUILayout.EndVertical ();
		
		switch ((TerrainTool)tool) {
		case TerrainTool.PaintHeight:
			ShowRaiseHeight ();
			break;
		case TerrainTool.SetHeight:
			ShowSetHeight ();
			break;
		case TerrainTool.SmoothHeight:
			ShowSmoothHeight ();
			break;
		case TerrainTool.PaintTexture:
			ShowTextures ();
			break;
		case TerrainTool.PlaceTree:
			ShowTrees();
			break;
		case TerrainTool.PaintDetail:
			ShowDetails();
			break;
		case TerrainTool.TerrainSettings:
			ShowSettings();
			break;
		}
		
		GUI.changed = false;
		if (GUI.changed)
			EditorUtility.SetDirty(this);
		GUILayout.Space (5);
	}
	
	Brush GetActiveBrush (int size)
	{
		if (m_CachedBrush == null)
			m_CachedBrush  = new Brush ();

		m_CachedBrush.Load(s_BrushTextures[m_SelectedBrush], size);
		
		return m_CachedBrush;
	}
	
	public bool Raycast (out Vector2 uv, out Vector3 pos)
	{
		Ray mouseRay = HandleUtility.GUIPointToWorldRay(Event.current.mousePosition);

		RaycastHit hit;
		if (m_Terrain.collider.Raycast(mouseRay, out hit, Mathf.Infinity))
		{
			uv = hit.textureCoord;
			pos = hit.point;
			return true;
		}

		uv = Vector2.zero;
		pos = Vector3.zero;
		return false;
	}

	public bool HasFrameBounds ()
	{
		// When selecting terrains using Scene search, they may be framed right after selecting, when the editor
		// is not initialized yet. Just return empty bounds in that case.
		return m_Terrain != null;
	}

	public Bounds OnGetFrameBounds ()
	{
		Vector2 uv;
		Vector3 pos;

		// It's possible that something other the scene view invoked OnGetFrameBounds (e.g. double clicking the terrain in the hierarchy)
		// In this case we can't do a raycast, because there is no active camera.
		if (Camera.current && Raycast (out uv, out pos))
		{
			// Use height editing tool for calculating the bounds by default
			Bounds bounds = new Bounds ();
			float brushSize = selectedTool == TerrainTool.PlaceTree ? TreePainter.brushSize : m_Size;
			Vector3 size;
			size.x = brushSize / m_Terrain.terrainData.heightmapWidth * m_Terrain.terrainData.size.x;
			size.z = brushSize / m_Terrain.terrainData.heightmapHeight * m_Terrain.terrainData.size.z;
			size.y = (size.x + size.z) * 0.5F;
			bounds.center = pos;
			bounds.size = size;
			// detail painting needs to be much closer
			if (selectedTool == TerrainTool.PaintDetail && m_Terrain.terrainData.detailWidth != 0)
			{
				size.x = brushSize / m_Terrain.terrainData.detailWidth * m_Terrain.terrainData.size.x * 0.7F;
				size.z = brushSize / m_Terrain.terrainData.detailHeight * m_Terrain.terrainData.size.z * 0.7F;
				size.y = 0;
				bounds.size = size;
			}
			
			return bounds;
		}
		else
		{
			// We don't return bounds from the collider, because apparently they are not immediately
			// updated after changing the position. So if terrain is repositioned, then bounds will
			// still be wrong until after some times PhysX updates them (it gets them through some
			// pruning interface and not directly from heightmap shape).
			//return m_Terrain.collider.bounds;
			
			Vector3 position = m_Terrain.transform.position;
			Vector3 size = m_Terrain.terrainData.size;

			float[,] heights = m_Terrain.terrainData.GetHeights (0, 0, m_Terrain.terrainData.heightmapWidth, m_Terrain.terrainData.heightmapHeight);

			float maxHeight = float.MinValue;
			for (int y = 0; y < m_Terrain.terrainData.heightmapHeight; y++)
				for (int x = 0; x < m_Terrain.terrainData.heightmapWidth; x++)
					maxHeight = Mathf.Max (maxHeight, heights[x, y]);

			size.y = maxHeight * size.y;

			return new Bounds (position + size * 0.5f, size);
		}
	}

	private bool IsModificationToolActive()
	{
		if (!m_Terrain)
			return false;
		TerrainTool st = selectedTool;
		if (st == TerrainTool.TerrainSettings)
			return false;
		if ((int)st < 0 || st >= TerrainTool.TerrainToolCount)
			return false;
		return true;
	}

	bool IsBrushPreviewVisible ()
	{
		if (!IsModificationToolActive())
			return false;
			
		Vector3 pos;
		Vector2 uv;	
		return Raycast (out uv, out pos);
	}
	
	void DisableProjector ()
	{
		if (m_CachedBrush != null)	
			m_CachedBrush.GetPreviewProjector().enabled = false;
	}
	
	void UpdatePreviewBrush ()
	{
		if (!IsModificationToolActive() || m_Terrain.terrainData == null)
		{
			DisableProjector ();
			return;
		}		
		
		Projector projector = GetActiveBrush(m_Size).GetPreviewProjector();
		float size = 1.0F;
		float aspect = m_Terrain.terrainData.size.x / m_Terrain.terrainData.size.z;
						
		Transform tr = projector.transform;
		Vector2 uv;
		Vector3 pos;
		
		bool isValid = true;
		if (Raycast (out uv, out pos))
		{
			if (selectedTool == TerrainTool.PlaceTree)
			{
				projector.material.mainTexture = (Texture2D)EditorGUIUtility.Load (EditorResourcesUtility.brushesPath + "builtin_brush_4.png");
				
				size = TreePainter.brushSize / 0.80f;
				aspect = 1;
			}
			else if (selectedTool == TerrainTool.PaintHeight || selectedTool == TerrainTool.SetHeight || selectedTool == TerrainTool.SmoothHeight)
			{
				if (m_Size % 2 == 0)
				{
					float offset = 0.5F;
					uv.x = (Mathf.Floor(uv.x * (m_Terrain.terrainData.heightmapWidth - 1)) + offset) / (m_Terrain.terrainData.heightmapWidth - 1);
					uv.y = (Mathf.Floor(uv.y * (m_Terrain.terrainData.heightmapHeight - 1)) + offset) / (m_Terrain.terrainData.heightmapHeight - 1);
				}
				else
				{
					uv.x = (Mathf.Round(uv.x * (m_Terrain.terrainData.heightmapWidth - 1))) / (m_Terrain.terrainData.heightmapWidth - 1);
					uv.y = (Mathf.Round(uv.y * (m_Terrain.terrainData.heightmapHeight - 1))) / (m_Terrain.terrainData.heightmapHeight - 1);
				}
				
				pos.x = uv.x * m_Terrain.terrainData.size.x; 
				pos.z = uv.y * m_Terrain.terrainData.size.z; 
				pos += m_Terrain.transform.position;
				
				size = m_Size * 0.5f / m_Terrain.terrainData.heightmapWidth * m_Terrain.terrainData.size.x;
			}
			else if (selectedTool == TerrainTool.PaintTexture || selectedTool == TerrainTool.PaintDetail)
			{
				float offset = m_Size % 2 == 0 ? 0.0F : 0.5F;
				int width, height;
				if (selectedTool == TerrainTool.PaintTexture)
				{
					width = m_Terrain.terrainData.alphamapWidth;
					height = m_Terrain.terrainData.alphamapHeight;
				}
				else
				{
					width = m_Terrain.terrainData.detailWidth;
					height = m_Terrain.terrainData.detailHeight;
				}
				
				if (width == 0 || height == 0)
					isValid = false;
				
				uv.x = (Mathf.Floor(uv.x * width) + offset) / width;
				uv.y = (Mathf.Floor(uv.y * height) + offset) / height;
				
				pos.x = uv.x * m_Terrain.terrainData.size.x; 
				pos.z = uv.y * m_Terrain.terrainData.size.z; 
				pos += m_Terrain.transform.position;
				
				size = m_Size * 0.5f / width * m_Terrain.terrainData.size.x;
				aspect = (float)width / (float)height;
			}
		}
		else
			isValid = false;

		projector.enabled = isValid;
		if (isValid)
		{
			pos.y = m_Terrain.SampleHeight(pos);
			tr.position = pos + new Vector3(0.0f, 50.0f, 0.0f);
		}	
		
		projector.orthographicSize = size / aspect;
		projector.aspectRatio = aspect;
	}	
	
	public void OnSceneGUI ()
	{
		Initialize();

		if (!m_Terrain.terrainData)
			return;
		
		Event e = Event.current;
			
		CheckKeys ();

		int id = GUIUtility.GetControlID(s_TerrainEditorHash, FocusType.Passive);
		switch (e.GetTypeForControl (id))
		{
			case EventType.Layout:
				if (!IsModificationToolActive())
					return;
				HandleUtility.AddDefaultControl (id);
				break;
				
			case EventType.MouseMove:
				if (IsBrushPreviewVisible ())
					HandleUtility.Repaint();
			break;
						
			case EventType.MouseDown:
			case EventType.MouseDrag:
			{			
				// Don't do anything on MouseDrag if we don't own the hotControl.
				if (e.GetTypeForControl (id) == EventType.MouseDrag && EditorGUIUtility.hotControl != id)
					return;
				
				// If user is ALT-dragging, we want to return to main routine
				if (Event.current.alt)
					return;

				// Allow painting with LMB only
				if (e.button != 0)
					return;

				if (!IsModificationToolActive())
					return;
					
				if (HandleUtility.nearestControl != id) 
					return;

				if (e.type == EventType.MouseDown)
					EditorGUIUtility.hotControl = id;

				Vector2 uv;
				Vector3 pos;
				if (Raycast (out uv, out pos))
				{ 
					if (selectedTool == TerrainTool.SetHeight && Event.current.shift)
					{
						m_TargetHeight.value = m_Terrain.terrainData.GetInterpolatedHeight(uv.x, uv.y) / m_Terrain.terrainData.size.y;
						InspectorWindow.RepaintAllInspectors ();
					}
					else if (selectedTool == TerrainTool.PlaceTree)
					{
						if (e.type == EventType.MouseDown)
							Undo.RegisterCompleteObjectUndo (m_Terrain.terrainData, "Place Tree");
						TreePainter.terrain = m_Terrain;				

						if (!Event.current.shift && !Event.current.control)
							TreePainter.PlaceTrees (uv.x, uv.y);
						else
							TreePainter.RemoveTrees (uv.x, uv.y, Event.current.control);
					}
					else if (selectedTool == TerrainTool.PaintTexture)
					{
						if (e.type == EventType.MouseDown)
						{
							var undoableObjects = new List<UnityEngine.Object> ();
							undoableObjects.Add (m_Terrain.terrainData);
							undoableObjects.AddRange (m_Terrain.terrainData.alphamapTextures);
							
							Undo.RegisterCompleteObjectUndo (undoableObjects.ToArray(), "Detail Edit");
						}

						// Setup splat painter
						SplatPainter splatPainter = new SplatPainter();
						splatPainter.size = m_Size;
						splatPainter.strength = m_Strength;
						splatPainter.terrainData = m_Terrain.terrainData;
						splatPainter.brush = GetActiveBrush (splatPainter.size);
						splatPainter.target = m_SplatAlpha;
						splatPainter.tool = selectedTool;

						m_Terrain.editorRenderFlags = TerrainRenderFlags.heightmap;
						splatPainter.Paint (uv.x, uv.y, m_SelectedSplat);
						
												
						// Don't perform basemap calculation while painting
						m_Terrain.terrainData.SetBasemapDirty (false);
					}
					else if (selectedTool == TerrainTool.PaintDetail)
					{
						if (e.type == EventType.MouseDown)
							Undo.RegisterCompleteObjectUndo (m_Terrain.terrainData, "Detail Edit");
						// Setup detail painter
						DetailPainter detailPainter = new DetailPainter();
						detailPainter.size = m_Size;
						detailPainter.targetStrength = m_DetailStrength * 16F;
						detailPainter.opacity = m_DetailOpacity;
						if (Event.current.shift || Event.current.control)
							detailPainter.targetStrength *= -1;
						detailPainter.clearSelectedOnly = Event.current.control;
						detailPainter.terrainData = m_Terrain.terrainData;
						detailPainter.brush = GetActiveBrush (detailPainter.size);
						detailPainter.tool = selectedTool;
						detailPainter.randomizeDetails = true;

						detailPainter.Paint (uv.x, uv.y, m_SelectedDetail);
					}
					else
					{
						if (e.type == EventType.MouseDown)
							Undo.RegisterCompleteObjectUndo (m_Terrain.terrainData, "Heightmap Edit");
					
						// Setup painter
						HeightmapPainter painter = new HeightmapPainter ();
						painter.size = m_Size;
						painter.strength = m_Strength * 0.01F;
						if (selectedTool == TerrainTool.SmoothHeight)
							painter.strength = m_Strength;
						painter.terrainData = m_Terrain.terrainData;
						painter.brush = GetActiveBrush (m_Size);
						painter.targetHeight = m_TargetHeight;
						painter.tool = selectedTool;

						m_Terrain.editorRenderFlags = TerrainRenderFlags.heightmap;
						
						if (selectedTool == TerrainTool.PaintHeight && Event.current.shift)
							painter.strength = -painter.strength;
						
						painter.PaintHeight (uv.x, uv.y);
					}
				}

				e.Use();
			}
			break;
			
			case EventType.MouseUp:
			{
				if (GUIUtility.hotControl != id)
				{
					return;
				}

				// Release hot control
				GUIUtility.hotControl = 0;

				if (!IsModificationToolActive())
					return;
				
				// Perform basemap calculation after all painting has completed!
				if( selectedTool == TerrainTool.PaintTexture )
					m_Terrain.terrainData.SetBasemapDirty(true);
				
				m_Terrain.editorRenderFlags = TerrainRenderFlags.all;
				m_Terrain.ApplyDelayedHeightmapModification();

				e.Use();
			}
			break;

			case EventType.Repaint:
				DisableProjector();
				break;
		}
	}
	
	public void OnPreSceneGUI ()
	{
		if (Event.current.type == EventType.Repaint)
			UpdatePreviewBrush ();
	}

	
}
} //namespace
