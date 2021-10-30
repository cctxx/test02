using UnityEngine;

namespace UnityEditor {

internal class TerrainSplatEditor : EditorWindow
{
	private string m_ButtonTitle = string.Empty;
	private Vector2 m_ScrollPosition;
	private Terrain m_Terrain;
	private int m_Index = -1;

	public Texture2D    m_Texture;
	public Texture2D    m_NormalMap;
	private Vector2 m_TileSize;
	private Vector2 m_TileOffset;


	public TerrainSplatEditor()
	{
		position = new Rect(50,50,200,300);
		minSize = new Vector2 (200,300);
	}

	static internal void ShowTerrainSplatEditor (string title, string button, Terrain terrain, int index)
	{
		var editor = GetWindow<TerrainSplatEditor> (true, title);
		editor.m_ButtonTitle = button;
		editor.InitializeData(terrain, index);
	}

	
	void InitializeData (Terrain terrain, int index)
	{
		m_Terrain = terrain;
		m_Index = index;

		SplatPrototype info;
		if (index == -1)
			info = new SplatPrototype();
		else
			info = m_Terrain.terrainData.splatPrototypes[index];

		m_Texture = info.texture;
		m_NormalMap = info.normalMap;
		m_TileSize = info.tileSize;
		m_TileOffset = info.tileOffset;
	}
	

	void ApplyTerrainSplat ()
	{
		if (m_Terrain == null || m_Terrain.terrainData == null)
			return;

		SplatPrototype[] infos = m_Terrain.terrainData.splatPrototypes;
		if (m_Index == -1) {
			var newarray = new SplatPrototype[infos.Length + 1];
			System.Array.Copy (infos, 0, newarray, 0, infos.Length);
			m_Index = infos.Length;
			infos = newarray;
			infos[m_Index] = new SplatPrototype ();
		}
		
		infos[m_Index].texture = m_Texture;
		infos[m_Index].normalMap = m_NormalMap;
		infos[m_Index].tileSize = m_TileSize;
		infos[m_Index].tileOffset = m_TileOffset;
		
		m_Terrain.terrainData.splatPrototypes = infos;
		EditorUtility.SetDirty(m_Terrain);
	}

	private bool ValidateTerrain ()
	{
		if (m_Terrain == null || m_Terrain.terrainData == null)
		{
			EditorGUILayout.HelpBox ("Terrain does not exist", MessageType.Error);
			return false;
		}
		return true;
	}

	private bool ValidateMainTexture (Texture2D tex)
	{
		if (tex == null)
		{
			EditorGUILayout.HelpBox ("Assign a tiling texture", MessageType.Warning);
			return false;
		}
		if (tex.width != Mathf.ClosestPowerOfTwo (tex.width) || tex.height != Mathf.ClosestPowerOfTwo (tex.height))
		{
			// power of two needed for efficient base map generation
			EditorGUILayout.HelpBox ("Texture size must be power of two", MessageType.Warning);
			return false;
		}
		if (tex.mipmapCount <= 1)
		{
			// mip maps needed for efficient base map generation.
			// And without mipmaps the terrain will look & work bad anyway.
			EditorGUILayout.HelpBox ("Texture must have mip maps", MessageType.Warning);
			return false;
		}
		return true;
	}

	private void ShowNormalMapShaderWarning()
	{		
		if (m_NormalMap != null && m_Terrain != null)
		{
			if (m_Terrain.materialTemplate == null || !m_Terrain.materialTemplate.HasProperty ("_Normal0"))
			{
				EditorGUILayout.HelpBox ("Note: in order for normal map to have effect, a custom material with normal mapped terrain shader needs to be used.", MessageType.Info);
			}
		}
	}

	private static void TextureFieldGUI (string label, ref Texture2D texture)
	{
		GUILayout.Space (6);
		GUILayout.BeginVertical (GUILayout.Width (80));
		GUILayout.Label (label);

		System.Type t = typeof (Texture2D);
		Rect r = GUILayoutUtility.GetRect (64, 64, 64, 64, GUILayout.MaxWidth (64));
		texture = EditorGUI.DoObjectField (r, r, EditorGUIUtility.GetControlID (12354, EditorGUIUtility.native, r), texture, t, null, null, false) as Texture2D;

		GUILayout.EndVertical ();
	}

	private static void SplatSizeGUI(ref Vector2 scale, ref Vector2 offset)
	{
		GUILayoutOption kWidth10 = GUILayout.Width (10);
		GUILayoutOption kMinWidth32 = GUILayout.MinWidth (32);

		GUILayout.Space (6);

		GUILayout.BeginHorizontal ();

		GUILayout.BeginVertical ();
		GUILayout.Label("", EditorStyles.miniLabel, kWidth10);
		GUILayout.Label("x", EditorStyles.miniLabel, kWidth10);
		GUILayout.Label("y", EditorStyles.miniLabel, kWidth10);
		GUILayout.EndVertical();
		GUILayout.BeginVertical();
		GUILayout.Label("Size", EditorStyles.miniLabel);
		scale.x = EditorGUILayout.FloatField(scale.x, EditorStyles.miniTextField, kMinWidth32);
		scale.y = EditorGUILayout.FloatField(scale.y, EditorStyles.miniTextField, kMinWidth32);
		GUILayout.EndVertical ();
		GUILayout.BeginVertical();
		GUILayout.Label("Offset", EditorStyles.miniLabel);
		offset.x = EditorGUILayout.FloatField(offset.x, EditorStyles.miniTextField, kMinWidth32);
		offset.y = EditorGUILayout.FloatField(offset.y, EditorStyles.miniTextField, kMinWidth32);
		GUILayout.EndVertical();

		GUILayout.EndHorizontal ();
	}

	private void OnGUI ()
	{
		const float controlSize = 64;
		EditorGUIUtility.labelWidth = Screen.width - controlSize - 20;

		bool isValid = true;

		m_ScrollPosition = EditorGUILayout.BeginVerticalScrollView (m_ScrollPosition, false, GUI.skin.verticalScrollbar, GUI.skin.scrollView);

		isValid &= ValidateTerrain();

		EditorGUI.BeginChangeCheck ();

		// texture & normal map
		GUILayout.BeginHorizontal();
		TextureFieldGUI ("Texture", ref m_Texture);
		TextureFieldGUI ("Normal Map", ref m_NormalMap);
		GUILayout.FlexibleSpace();
		GUILayout.EndHorizontal();
		isValid &= ValidateMainTexture (m_Texture);
		ShowNormalMapShaderWarning ();

		// tiling & offset
		SplatSizeGUI (ref m_TileSize, ref m_TileOffset);

		bool modified = EditorGUI.EndChangeCheck();

		EditorGUILayout.EndScrollView ();

		GUILayout.FlexibleSpace ();

		// button
		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();
		GUI.enabled = isValid;

		if (GUILayout.Button (m_ButtonTitle, GUILayout.MinWidth (100)))
		{
			ApplyTerrainSplat ();
			Close ();
			GUIUtility.ExitGUI ();
		}
		GUI.enabled = true;

		GUILayout.EndHorizontal ();

		if (modified && isValid && m_Index != -1)
		{
			ApplyTerrainSplat ();
		}
	}
}

} //namespace
