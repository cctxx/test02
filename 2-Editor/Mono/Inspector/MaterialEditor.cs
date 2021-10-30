
using System;
using UnityEngine;
using UnityEditorInternal;
using Object = UnityEngine.Object;

namespace UnityEditor
{


[CustomEditor(typeof(Material))]
[CanEditMultipleObjects]
public class MaterialEditor : Editor
{
	private bool m_IsVisible;
	const int kSpacingUnderTexture = 6;

	private enum PreviewType {
		Mesh = 0,
		Plane = 1
	}

	private PreviewType GetPreviewType (Material mat)
	{
		if (mat == null)
			return PreviewType.Mesh;
		string tag = mat.GetTag("PreviewType", false, string.Empty).ToLower();
		if (tag == "plane")
			return PreviewType.Plane;
		return PreviewType.Mesh;
	}

	private bool DoesPreviewAllowRotation (PreviewType type)
	{
		return type != PreviewType.Plane;
	}
	
	public bool isVisible { get { return m_IsVisible; } }

	SerializedProperty m_Shader;

	private string m_InfoMessage;
	private Vector2 m_PreviewDir = new Vector2 (0,-20);
	private int m_SelectedMesh;
	private int m_TimeUpdate;
	private int m_LightMode = 1;

	public void SetShader (Shader shader)
	{
		SetShader(shader, true);
	}

	public void SetShader(Shader shader, bool registerUndo)
	{
		var inspectorNeedsRebuild = false;
		var oldShader = m_Shader.objectReferenceValue as Shader;

		// If new shader uses different custom editor, then we have to rebuild inspector
		if (oldShader != null && oldShader.customEditor != shader.customEditor)
			inspectorNeedsRebuild = true;

		// Assign the shader through the serialization system.
		m_Shader.objectReferenceValue = shader;
		// We have to apply the changes to the serialization first,
		// otherwise other changes get overridden once we do it.
		serializedObject.ApplyModifiedProperties ();

		// We also have to assign the shader to each target explicitly,
		// since the setter makes sure that the property list includes
		// the properties used by the shader.
		foreach (Material material in targets)
		{
			if (material.shader.customEditor != shader.customEditor)
				inspectorNeedsRebuild = true;
			Undo.RecordObject(material, "Assign shader");	
			material.shader = shader;
			EditorMaterialUtility.ResetDefaultTextures (material, false);
			ApplyMaterialPropertyDrawers(material);
		}

		// Rebuild inspector if custom editor class has changed
		if (inspectorNeedsRebuild && ActiveEditorTracker.sharedTracker != null)
		{
			ActiveEditorTracker.sharedTracker.ForceRebuild ();
		}
	}

	private void OnSelectedShaderPopup (string command, Shader shader)
	{
		serializedObject.Update ();
		
		if (shader != null)
			SetShader (shader);
		
		PropertiesChanged ();
	}
	
	private void ShaderPopup (GUIStyle style)
	{
		bool wasEnabled = GUI.enabled;
		
		Shader shader = m_Shader.objectReferenceValue as Shader;

		Rect position = EditorGUILayout.GetControlRect ();
		position = EditorGUI.PrefixLabel (position, 47385, EditorGUIUtility.TempContent ("Shader"));
		EditorGUI.showMixedValue = m_Shader.hasMultipleDifferentValues;

		GUIContent buttonContent = EditorGUIUtility.TempContent(shader != null ? shader.name : "No Shader Selected");
		if (EditorGUI.ButtonMouseDown (position, buttonContent, EditorGUIUtility.native, style)) 
		{
			EditorGUI.showMixedValue = false;
			Vector2 pos = GUIUtility.GUIToScreenPoint(new Vector2(position.x, position.y));
			
			// @TODO: SetupShaderPopupMenu in ShaderMenu.cpp needs to be able to accept null
			// so no choice is selected when the shaders are different for the selected materials.
			InternalEditorUtility.SetupShaderMenu (target as Material);
			EditorUtility.Internal_DisplayPopupMenu (new Rect (pos.x, pos.y, position.width, position.height), "CONTEXT/ShaderPopup", this, 0);
			Event.current.Use();
		}
		EditorGUI.showMixedValue = false;
		
		GUI.enabled = wasEnabled;
	}
	
	public virtual void Awake ()
	{
		m_IsVisible = InternalEditorUtility.GetIsInspectorExpanded(target);
	}

	public override void OnInspectorGUI ()
	{
		serializedObject.Update ();

		if (m_IsVisible && !m_Shader.hasMultipleDifferentValues && m_Shader.objectReferenceValue != null)
		{
			// Show Material properties
			if (PropertiesGUI ())
				PropertiesChanged ();
		}
	}
	
	// A minimal list of settings to be shown in the Asset Store preview inspector
	internal override void OnAssetStoreInspectorGUI ()
	{
		OnInspectorGUI();
	}
		
	public void PropertiesChanged ()
	{
		// @TODO: Show performance warnings for multi-selections too?
		m_InfoMessage = null;
		if (targets.Length == 1)
			m_InfoMessage = Utils.PerformanceChecks.CheckMaterial (target as Material, EditorUserBuildSettings.activeBuildTarget);
	}
	
	protected override void OnHeaderGUI ()
	{
		Rect titleRect = DrawHeaderGUI (this, targetTitle);
		
		int foldoutID = GUIUtility.GetControlID (45678, FocusType.Passive);
		bool newVisible = EditorGUI.DoObjectFoldout (foldoutID, titleRect, targets, m_IsVisible);
		if (newVisible != m_IsVisible)
		{
			m_IsVisible = newVisible;
			InternalEditorUtility.SetIsInspectorExpanded(target, newVisible);
		}
	}
	
	internal override void OnHeaderControlsGUI ()
	{
		serializedObject.Update ();
		
		EditorGUI.BeginDisabledGroup (!IsEnabled ());
		
		EditorGUIUtility.labelWidth = 50;
		
		// Shader selection dropdown
		ShaderPopup ("MiniPulldown");
		
		// Edit button for custom shaders
		if (!m_Shader.hasMultipleDifferentValues &&
			m_Shader.objectReferenceValue != null &&
			(m_Shader.objectReferenceValue.hideFlags & HideFlags.DontSave) == 0)
		{
			if (GUILayout.Button ("Edit...", EditorStyles.miniButton, GUILayout.ExpandWidth (false)))
				AssetDatabase.OpenAsset (m_Shader.objectReferenceValue);
		}
		
		EditorGUI.EndDisabledGroup ();
	}


	// -------- obsolete helper functions to get/set material values

	[System.Obsolete ("Use GetMaterialProperty instead.")]
	public float GetFloat (string propertyName, out bool hasMixedValue)
	{
		hasMixedValue = false;
		float f = (targets[0] as Material).GetFloat (propertyName);
		for (int i=1; i<targets.Length; i++)
			if ((targets[i] as Material).GetFloat (propertyName) != f) { hasMixedValue = true; break; }
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetFloat (string propertyName, float value)
	{
		foreach (Material material in targets)
			material.SetFloat (propertyName, value);
	}

	[System.Obsolete ("Use GetMaterialProperty instead.")]
	public Color GetColor (string propertyName, out bool hasMixedValue)
	{
		hasMixedValue = false;
		Color f = (targets[0] as Material).GetColor (propertyName);
		for (int i=1; i<targets.Length; i++)
			if ((targets[i] as Material).GetColor (propertyName) != f) { hasMixedValue = true; break; }
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetColor (string propertyName, Color value)
	{
		foreach (Material material in targets)
			material.SetColor (propertyName, value);
	}

	[System.Obsolete ("Use GetMaterialProperty instead.")]
	public Vector4 GetVector (string propertyName, out bool hasMixedValue)
	{
		hasMixedValue = false;
		Vector4 f = (targets[0] as Material).GetVector (propertyName);
		for (int i=1; i<targets.Length; i++)
			if ((targets[i] as Material).GetVector (propertyName) != f) { hasMixedValue = true; break; }
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetVector (string propertyName, Vector4 value)
	{
		foreach (Material material in targets)
			material.SetVector (propertyName, value);
	}

	[System.Obsolete ("Use GetMaterialProperty instead.")]
	public Texture GetTexture(string propertyName, out bool hasMixedValue)
	{
		hasMixedValue = false;
		Texture f = (targets[0] as Material).GetTexture (propertyName);
		for (int i=1; i<targets.Length; i++)
			if ((targets[i] as Material).GetTexture (propertyName) != f) { hasMixedValue = true; break; }
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetTexture (string propertyName, Texture value)
	{
		foreach (Material material in targets)
			material.SetTexture (propertyName, value);
	}

	[System.Obsolete ("Use MaterialProperty instead.")]	
	public Vector2 GetTextureScale(string propertyName, out bool hasMixedValueX, out bool hasMixedValueY)
	{
		hasMixedValueX = false;
		hasMixedValueY = false;
		Vector2 f = (targets[0] as Material).GetTextureScale (propertyName);
		for (int i=1; i<targets.Length; i++)
		{
			Vector2 f2 = (targets[i] as Material).GetTextureScale (propertyName);
			if (f2.x != f.x) { hasMixedValueX = true; }
			if (f2.y != f.y) { hasMixedValueY = true; }
			if (hasMixedValueX && hasMixedValueY)
				break;
		}
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public Vector2 GetTextureOffset(string propertyName, out bool hasMixedValueX, out bool hasMixedValueY)
	{
		hasMixedValueX = false;
		hasMixedValueY = false;
		Vector2 f = (targets[0] as Material).GetTextureOffset (propertyName);
		for (int i=1; i<targets.Length; i++)
		{
			Vector2 f2 = (targets[i] as Material).GetTextureOffset (propertyName);
			if (f2.x != f.x) { hasMixedValueX = true; }
			if (f2.y != f.y) { hasMixedValueY = true; }
			if (hasMixedValueX && hasMixedValueY)
				break;
		}
		return f;
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetTextureScale(string propertyName, Vector2 value, int coord)
	{
		foreach (Material material in targets)
		{
			Vector2 f = material.GetTextureScale (propertyName);
			f[coord] = value[coord];
			material.SetTextureScale (propertyName, f);
		}
	}

	[System.Obsolete ("Use MaterialProperty instead.")]
	public void SetTextureOffset(string propertyName, Vector2 value, int coord)
	{
		foreach (Material material in targets)
		{
			Vector2 f = material.GetTextureOffset (propertyName);
			f[coord] = value[coord];
			material.SetTextureOffset (propertyName, f);
		}
	}


	// -------- helper functions to display common material controls

	public float RangeProperty(MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect (prop, label, true);
		return RangeProperty(r, prop, label);
	}

	public float RangeProperty(Rect position, MaterialProperty prop, string label)
	{
		EditorGUI.BeginChangeCheck ();

		EditorGUI.showMixedValue = prop.hasMixedValue;
		float power = (prop.name == "_Shininess") ? 5f : 1f;
		float newValue = EditorGUI.PowerSlider (position, label, prop.floatValue, prop.rangeLimits.x, prop.rangeLimits.y, power);
		EditorGUI.showMixedValue = false;

		if (EditorGUI.EndChangeCheck())
			prop.floatValue = newValue;

		return prop.floatValue;
	}

	public float FloatProperty(MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect (prop, label, true);
		return FloatProperty (r, prop, label);
	}

	public float FloatProperty (Rect position, MaterialProperty prop, string label)
	{
		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = prop.hasMixedValue;
		float newValue = EditorGUI.FloatField (position, label, prop.floatValue);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
			prop.floatValue = newValue;

		return prop.floatValue;
	}

	public Color ColorProperty (MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect (prop, label, true);
		return ColorProperty (r, prop, label);
	}

	public Color ColorProperty (Rect position, MaterialProperty prop, string label)
	{
		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = prop.hasMixedValue;
		Color newValue = EditorGUI.ColorField (position, label, prop.colorValue);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
			prop.colorValue = newValue;

		return prop.colorValue;
	}

	public Vector4 VectorProperty(MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect (prop, label, true);
		return VectorProperty (r, prop, label);		
	}

	public Vector4 VectorProperty (Rect position, MaterialProperty prop, string label)
	{
		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = prop.hasMixedValue;
		Vector4 newValue = EditorGUI.Vector4Field (position, label, prop.vectorValue);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
			prop.vectorValue = newValue;

		return prop.vectorValue;
	}

	internal static void TextureScaleOffsetProperty(Rect position, MaterialProperty property)
	{
		EditorGUI.BeginChangeCheck();
		// Mixed value mask is 4 bits for the uv offset & scale (First bit is for the texture itself)
		int mixedValuemask = property.mixedValueMask >> 1;
		Vector4 scaleAndOffset = TextureScaleOffsetProperty(position, property.textureScaleAndOffset, mixedValuemask);

		if (EditorGUI.EndChangeCheck())
			property.textureScaleAndOffset = scaleAndOffset;
	}
	

	private Texture TexturePropertyBody (Rect position, MaterialProperty prop, string label)
	{
		position.xMin = position.xMax - EditorGUI.kObjectFieldThumbnailHeight;

		m_DesiredTexdim = prop.textureDimension;
		System.Type t;
		switch (m_DesiredTexdim)
		{
			case MaterialProperty.TexDim.Tex2D:
				t = typeof(Texture);
				break;
			case MaterialProperty.TexDim.Cube:
				t = typeof(Cubemap);
				break;
			case MaterialProperty.TexDim.Tex3D:
				t = typeof (Texture3D);
				break;
			case MaterialProperty.TexDim.Any:
				t = typeof(Texture);
				break;
			default:// TexDimUnknown, TexDimNone, TexDimDeprecated1D
				t = null;
				break;
		}
		
		// Why are we disabling the GUI in Animation Mode here?
		// If it's because object references can't be changed, shouldn't it be done in ObjectField instead?
		bool wasEnabled = GUI.enabled;
					
		EditorGUI.BeginChangeCheck ();
		if ((prop.flags & MaterialProperty.PropFlags.PerRendererData) != 0)
			GUI.enabled = false;

		EditorGUI.showMixedValue = prop.hasMixedValue;
		Texture newValue = EditorGUI.DoObjectField (position, position, GUIUtility.GetControlID (12354, EditorGUIUtility.native, position), prop.textureValue, t, null, TextureValidator, false) as Texture;
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
			prop.textureValue = newValue;

		GUI.enabled = wasEnabled;
		return prop.textureValue;
	}

	public Texture TextureProperty(MaterialProperty prop, string label)
	{
		return TextureProperty(prop, label, true);
	}

	public Texture TextureProperty(MaterialProperty prop, string label, bool scaleOffset)
	{
		Rect r = GetPropertyRect (prop, label, true);
		return TextureProperty (r, prop, label, scaleOffset);
	}

	private static bool DoesNeedNormalMapFix(MaterialProperty prop)
	{
		if (prop.name != "_BumpMap")
			return false;
		foreach (Material material in prop.targets)
		{
			if (InternalEditorUtility.BumpMapTextureNeedsFixing(material))
				return true;
		}
		return false;
	}

	public Texture TextureProperty (Rect position, MaterialProperty prop, string label, bool scaleOffset)
	{
		//@TOD: WTF the code below ends up not actually doing anything?!?
		if (target is ProceduralMaterial)
		{
			// Don't show Substance Texture slots.
			// We skips these because they are handled specially by the ProceduralMaterialEditor.
			// We need to pair each material with it's own GetTexture call for the check to work correctly.
			foreach (Material material in targets)
			{
				if (SubstanceImporter.IsProceduralTextureSlot (material, material.GetTexture (prop.name), prop.name))
				{
					break;
				}
			}
		}
		
		EditorGUI.PrefixLabel (position, EditorGUIUtility.TempContent (label));
		
		float messageHeight = position.height - EditorGUI.kObjectFieldThumbnailHeight - kSpacingUnderTexture;
		position.height = EditorGUI.kObjectFieldThumbnailHeight;
		
		Rect texPos = position;
		texPos.xMin = texPos.xMax - EditorGUIUtility.fieldWidth;
		Texture value = TexturePropertyBody (texPos, prop, label);
		
		if (scaleOffset)
		{
			EditorGUI.indentLevel++;
			Rect scaleOffsetRect = position;
			scaleOffsetRect.yMin += EditorGUI.kSingleLineHeight;
			scaleOffsetRect.xMax -= EditorGUIUtility.fieldWidth + 2;
			scaleOffsetRect = EditorGUI.IndentedRect (scaleOffsetRect);
			EditorGUI.indentLevel--;
			TextureScaleOffsetProperty (scaleOffsetRect, prop);
		}

		if (DoesNeedNormalMapFix(prop))
		{
			Rect pos = position;
			pos.y += pos.height;
			pos.height = messageHeight;
			pos.yMin += 4;
			EditorGUI.indentLevel++;
			pos = EditorGUI.IndentedRect (pos);
			EditorGUI.indentLevel--;

			GUI.Box(pos, GUIContent.none, EditorStyles.helpBox);
			pos = EditorStyles.helpBox.padding.Remove (pos);
			Rect r = pos;
			r.width -= 60;
			GUI.Label (r, EditorGUIUtility.TextContent ("MaterialInspector.BumpMapFixingWarning"), EditorStyles.wordWrappedMiniLabel);
			r = pos;
			r.xMin += r.width - 60;
			r.y += 2;
			r.height -= 4;
			if (GUI.Button(r, EditorGUIUtility.TextContent("MaterialInspector.BumpMapFixingButton")))
			{
				foreach (Material material in targets)
				{
					if (InternalEditorUtility.BumpMapTextureNeedsFixing(material))
						InternalEditorUtility.FixNormalmapTexture(material);
				}
			}
		}

		return value;
	}

	internal static Vector4 TextureScaleOffsetProperty(Rect position, Vector4 scaleOffset, int mixedValueMask)
	{
		const float labelWidth = 10;
		const float space = 2;
		
		Rect row0, row1, row2, col0, col1, col2;
		row0 = row1 = row2 = col0 = col1 = col2 = new Rect ();
		
		position.y = position.yMax - 3 * 16 + 3;
		
		col0.x = position.x;
		col0.width = labelWidth;
		col1.x = col0.xMax + space;
		col1.width = (position.xMax - col1.x - space) / 2;
		col2.x = col1.xMax + space;
		col2.xMax = position.xMax;
		
		row0.y = position.y;
		row0.height = 16;
		row1 = row0;
		row1.y += 16;
		row2 = row1;
		row2.y += 16;
		
		GUI.Label (new Rect (col0.x, row1.y, col0.width, row1.height), "x", EditorStyles.miniLabel);
		GUI.Label (new Rect (col0.x, row2.y, col0.width, row2.height), "y", EditorStyles.miniLabel);
		GUI.Label (new Rect (col1.x, row0.y, col1.width, row0.height), "Tiling", EditorStyles.miniLabel);
		GUI.Label (new Rect (col2.x, row0.y, col2.width, row0.height), "Offset", EditorStyles.miniLabel);
		
		for (int c=0; c<2; c++)
		{
			int index = c + 0;
			EditorGUI.showMixedValue = (mixedValueMask & (1 << index)) != 0;
			Rect row = (c == 0 ? row1 : row2);
			scaleOffset[index] = EditorGUI.FloatField (new Rect (col1.x, row.y, col1.width, row.height), scaleOffset[index], EditorStyles.miniTextField);
		}
		for (int c=0; c<2; c++)
		{
			int index = c + 2;
			EditorGUI.showMixedValue = (mixedValueMask & (1 << index)) != 0;
			Rect row = (c == 0 ? row1 : row2);
			scaleOffset[index] = EditorGUI.FloatField (new Rect (col2.x, row.y, col2.width, row.height), scaleOffset[index], EditorStyles.miniTextField);
		}
		
		return scaleOffset;
	}

	public float GetPropertyHeight(MaterialProperty prop)
	{
		return GetPropertyHeight(prop, prop.displayName);
	}

	public float GetPropertyHeight(MaterialProperty prop, string label)
	{
		// has custom drawer?
		MaterialPropertyDrawer drawer = MaterialPropertyDrawer.GetDrawer ((target as Material).shader, prop.name);
		if (drawer != null)
			return drawer.GetPropertyHeight(prop, label ?? prop.displayName, this);

		// otherwise, return height
		return GetDefaultPropertyHeight(prop);
	}

	public static float GetDefaultPropertyHeight(MaterialProperty prop)
	{
		if (prop.type == MaterialProperty.PropType.Vector)
			return EditorGUI.kStructHeaderLineHeight + EditorGUI.kSingleLineHeight;
		if (prop.type == MaterialProperty.PropType.Texture)
		{
			float h = EditorGUI.kObjectFieldThumbnailHeight + kSpacingUnderTexture;
			if (DoesNeedNormalMapFix(prop))
				h += EditorGUI.kSingleLineHeight * 2 + 4;
			return h;
		}
		return EditorGUI.kSingleLineHeight;
	}

	private Rect GetPropertyRect(MaterialProperty prop, string label, bool ignoreDrawer)
	{
		if (!ignoreDrawer)
		{
			MaterialPropertyDrawer drawer = MaterialPropertyDrawer.GetDrawer ((target as Material).shader, prop.name);
			if (drawer != null)
				return EditorGUILayout.GetControlRect (true, drawer.GetPropertyHeight (prop, label ?? prop.displayName, this), EditorStyles.layerMaskField);
		}

		return EditorGUILayout.GetControlRect (true, GetDefaultPropertyHeight (prop), EditorStyles.layerMaskField);
	}


	public void ShaderProperty(MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect(prop, label, false);
		ShaderProperty(r, prop, label);
	}

	public void ShaderProperty (Rect position, MaterialProperty prop, string label)
	{
		// Use custom property drawer if needed
		MaterialPropertyDrawer drawer = MaterialPropertyDrawer.GetDrawer ((target as Material).shader, prop.name);
		if (drawer != null)
		{
			// Remember look and widths
			float oldLabelWidth = EditorGUIUtility.labelWidth;
			float oldFieldWidth = EditorGUIUtility.fieldWidth;
			// Draw with custom drawer
			drawer.OnGUI (position, prop, label ?? prop.displayName, this);
			
			EditorGUIUtility.labelWidth = oldLabelWidth;
			EditorGUIUtility.fieldWidth = oldFieldWidth;
			return;
		}

		DefaultShaderProperty (position, prop, label);
	}

	public void DefaultShaderProperty(MaterialProperty prop, string label)
	{
		Rect r = GetPropertyRect (prop, label, true);
		DefaultShaderProperty (r, prop, label);
	}

	public void DefaultShaderProperty (Rect position, MaterialProperty prop, string label)
	{
		switch (prop.type)
		{
			case MaterialProperty.PropType.Range: // float ranges
				RangeProperty(position, prop, label);
				break;
			case MaterialProperty.PropType.Float: // floats
				FloatProperty(position, prop, label);
				break;
			case MaterialProperty.PropType.Color: // colors
				ColorProperty(position, prop, label);
				break;
			case MaterialProperty.PropType.Texture: // textures
				TextureProperty(position, prop, label, true);
				break;
			case MaterialProperty.PropType.Vector: // vectors
				VectorProperty(position, prop, label);
				break;
			default:
				GUI.Label(position, "Unknown property type: " + prop.name + ": " + (int)prop.type);
				break;
		}
	}


	// -------- obsolete versions of common controls


	[System.Obsolete ("Use RangeProperty with MaterialProperty instead.")]
	public float RangeProperty (string propertyName, string label, float v2, float v3)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return RangeProperty (prop, label);
	}
	[System.Obsolete ("Use FloatProperty with MaterialProperty instead.")]
	public float FloatProperty (string propertyName, string label)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return FloatProperty (prop, label);
	}
	[System.Obsolete ("Use ColorProperty with MaterialProperty instead.")]
	public Color ColorProperty (string propertyName, string label)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return ColorProperty (prop, label);
	}
	[System.Obsolete ("Use VectorProperty with MaterialProperty instead.")]
	public Vector4 VectorProperty (string propertyName, string label)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return VectorProperty (prop, label);
	}
	[System.Obsolete ("Use TextureProperty with MaterialProperty instead.")]
	public Texture TextureProperty (string propertyName, string label, ShaderUtil.ShaderPropertyTexDim texDim)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return TextureProperty (prop, label, true);
	}
	[System.Obsolete ("Use TextureProperty with MaterialProperty instead.")]
	public Texture TextureProperty (string propertyName, string label, ShaderUtil.ShaderPropertyTexDim texDim, bool scaleOffset)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyName);
		return TextureProperty (prop, label, scaleOffset);
	}
	[System.Obsolete ("Use ShaderProperty that takes MaterialProperty parameter instead.")]
	public void ShaderProperty (Shader shader, int propertyIndex)
	{
		MaterialProperty prop = GetMaterialProperty (targets, propertyIndex);
		ShaderProperty (prop, prop.displayName);
	}


	// -------- other functionality

	public static MaterialProperty[] GetMaterialProperties (Object[] mats)
	{
		return ShaderUtil.GetMaterialProperties(mats);
	}

	public static MaterialProperty GetMaterialProperty (Object[] mats, string name)
	{
		return ShaderUtil.GetMaterialProperty(mats, name);
	}

	public static MaterialProperty GetMaterialProperty (Object[] mats, int propertyIndex)
	{
		return ShaderUtil.GetMaterialProperty_Index (mats, propertyIndex);
	}

	class ForwardApplyMaterialModification
	{
		Renderer renderer;
		
		public ForwardApplyMaterialModification (Renderer r)
		{
			renderer = r;
		}

		public bool DidModifyAnimationModeMaterialProperty(MaterialProperty property, int changedMask, object previousValue)
		{
				return MaterialAnimationUtility.ApplyMaterialModificationToAnimationRecording (property, changedMask, renderer, previousValue);
		}
	}
	
	static Renderer GetAssociatedRenderFromInspector ()
	{
		if (InspectorWindow.s_CurrentInspectorWindow)
		{
			Editor[] editors = InspectorWindow.s_CurrentInspectorWindow.GetTracker().activeEditors;
			foreach (Editor editor in editors)
			{
				Renderer renderer = editor.target as Renderer;
				if (renderer)
					return renderer;
			}
		}	
		return null;
	}

	Renderer PrepareMaterialPropertiesForAnimationMode (MaterialProperty[] properties)
	{
		if (!AnimationMode.InAnimationMode())
			return null;
			
		Renderer renderer = GetAssociatedRenderFromInspector ();
		if (renderer != null)
		{
			ForwardApplyMaterialModification callback = new ForwardApplyMaterialModification(renderer);
			MaterialPropertyBlock block = new MaterialPropertyBlock();
			renderer.GetPropertyBlock(block);
			foreach (MaterialProperty prop in properties)
			{
				prop.ReadFromMaterialPropertyBlock (block);
				prop.applyPropertyCallback = callback.DidModifyAnimationModeMaterialProperty;
			}
		}
		return renderer;
	}
		

	public bool PropertiesGUI()
	{
		EditorGUIUtility.fieldWidth = EditorGUI.kObjectFieldThumbnailHeight;
		EditorGUIUtility.labelWidth = GUIClip.visibleRect.width - EditorGUIUtility.fieldWidth - 17;

		EditorGUI.BeginChangeCheck();

		if (m_InfoMessage != null)
			EditorGUILayout.HelpBox(m_InfoMessage, MessageType.Info);

		MaterialProperty[] props = GetMaterialProperties (targets);
		
		Renderer associatedRenderer = PrepareMaterialPropertiesForAnimationMode (props);
		
		// calculate height
		float height = 0.0f;
		for (var i = 0; i < props.Length; i++)
		{
			if ((props[i].flags & MaterialProperty.PropFlags.HideInInspector) != 0)
				continue;
			height += GetPropertyHeight(props[i], props[i].displayName) + EditorGUI.kControlVerticalSpacing;
		}

		// render properties
		Rect r = EditorGUILayout.GetControlRect (true, height, EditorStyles.layerMaskField);

		for (var i = 0; i < props.Length; i++)
		{
			if ((props[i].flags & MaterialProperty.PropFlags.HideInInspector) != 0)
				continue;
			float h = GetPropertyHeight(props[i], props[i].displayName);
			r.height = h;
			
			Color previousColor = GUI.color;
			if (associatedRenderer != null && MaterialAnimationUtility.IsAnimated(props[i], associatedRenderer))
				GUI.color = AnimationMode.animatedPropertyColor;
				
			ShaderProperty(r, props[i], props[i].displayName);
			
			GUI.color = previousColor;
			
			r.y += h + EditorGUI.kControlVerticalSpacing;
		}
		return EditorGUI.EndChangeCheck();
	}

	public static void ApplyMaterialPropertyDrawers(Material material)
	{
		var objs = new Object[] { material };
		ApplyMaterialPropertyDrawers(objs);
	}

	public static void ApplyMaterialPropertyDrawers(Object[] targets)
	{
		if (targets == null || targets.Length == 0)
			return;
		var target = targets[0] as Material;
		if (target == null)
			return;

		var shader = target.shader;
		var props = GetMaterialProperties (targets);
		for (var i = 0; i < props.Length; i++)
		{
			MaterialPropertyDrawer drawer = MaterialPropertyDrawer.GetDrawer (shader, props[i].name);
			if (drawer != null)
				drawer.Apply(props[i]);
		}
	}

	public void RegisterPropertyChangeUndo(string label)
	{
		Undo.RecordObjects (targets, "Modify " + label + " of " + targetTitle);
	}

	private MaterialProperty.TexDim m_DesiredTexdim;
	
	private Object TextureValidator (Object[] references, System.Type objType, SerializedProperty property)
	{
		foreach (Object i in references)
		{
			var t = i as Texture;
			if (t)
			{
				if (ShaderUtil.GetTextureDimension (t) == (int)m_DesiredTexdim || m_DesiredTexdim == MaterialProperty.TexDim.Any)
					return t;
			}
		}
		return null;
	} 

	private PreviewRenderUtility m_PreviewUtility;
	private static Mesh[] s_Meshes = {null, null, null, null};
	private static Mesh s_PlaneMesh = null;
	private static GUIContent[] s_MeshIcons = { null, null, null, null };
	private static GUIContent[] s_LightIcons = { null, null };
	private static GUIContent[] s_TimeIcons = { null, null };
	
	private void Init ()
	{
		if (m_PreviewUtility == null)
		{
			m_PreviewUtility = new PreviewRenderUtility ();
			EditorUtility.SetCameraAnimateMaterials (m_PreviewUtility.m_Camera, true);
		}

		if (s_Meshes[0] == null)
		{
			var handleGo = (GameObject)EditorGUIUtility.LoadRequired ("Previews/PreviewMaterials.fbx");
			// @TODO: temp workaround to make it not render in the scene
			handleGo.SetActive (false);
			foreach (Transform t in handleGo.transform) {
				switch (t.name) {
				case "sphere":
					s_Meshes[0] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;	
					break;
				case "cube":
					s_Meshes[1] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
					break;
				case "cylinder":
					s_Meshes[2] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
					break;
				case "torus":
					s_Meshes[3] = ((MeshFilter)t.GetComponent ("MeshFilter")).sharedMesh;
					break;
				default :
					Debug.Log ("Something is wrong, weird object found: " + t.name);
					break;
				}
			}

			s_MeshIcons[0] = EditorGUIUtility.IconContent ("PreMatSphere");
			s_MeshIcons[1] = EditorGUIUtility.IconContent ("PreMatCube");
			s_MeshIcons[2] = EditorGUIUtility.IconContent ("PreMatCylinder");
			s_MeshIcons[3] = EditorGUIUtility.IconContent ("PreMatTorus");
		
			s_LightIcons[0] = EditorGUIUtility.IconContent ("PreMatLight0");
			s_LightIcons[1] = EditorGUIUtility.IconContent ("PreMatLight1");

			s_TimeIcons[0] = EditorGUIUtility.IconContent ("PlayButton");
			s_TimeIcons[1] = EditorGUIUtility.IconContent ("PauseButton");

			s_PlaneMesh = Resources.GetBuiltinResource(typeof(Mesh), "Quad.fbx") as Mesh;
		}
		
	}
	
	public sealed override void OnPreviewSettings ()
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			return;

		Init ();

		var mat = target as Material;
		var viewType = GetPreviewType(mat);
		if (targets.Length > 1 || viewType == PreviewType.Mesh)
		{
			m_TimeUpdate = PreviewGUI.CycleButton(m_TimeUpdate, s_TimeIcons);
			m_SelectedMesh = PreviewGUI.CycleButton (m_SelectedMesh, s_MeshIcons);
			m_LightMode = PreviewGUI.CycleButton (m_LightMode, s_LightIcons);
		}
	}

	public sealed override Texture2D RenderStaticPreview(string assetPath, Object[] subAssets, int width, int height)
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture) 
			return null;

		Init ();

		m_PreviewUtility.BeginStaticPreview (new Rect(0,0,width,height));
		
		DoRenderPreview();

		return m_PreviewUtility.EndStaticPreview ();
	}


	private void DoRenderPreview()
	{
		if (m_PreviewUtility.m_RenderTexture.width <= 0 || m_PreviewUtility.m_RenderTexture.height <= 0)
			return;

		var mat = target as Material;
		var viewType = GetPreviewType(mat);
		
		m_PreviewUtility.m_Camera.transform.position = -Vector3.forward * 5;
		m_PreviewUtility.m_Camera.transform.rotation = Quaternion.identity;
		Color amb;
		if (m_LightMode == 0)
		{
			m_PreviewUtility.m_Light[0].intensity = .5f;
			m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (30f, 30f, 0);
			m_PreviewUtility.m_Light[1].intensity = 0;
			amb = new Color (.2f, .2f, .2f, 0);
		}
		else
		{
			m_PreviewUtility.m_Light[0].intensity = .5f;
			m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (50f, 50f, 0);
			m_PreviewUtility.m_Light[1].intensity = .5f;
			amb = new Color (.2f, .2f, .2f, 0);
		}
		
		InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);

		Quaternion rot = Quaternion.identity;
		if (DoesPreviewAllowRotation(viewType))
			rot = Quaternion.Euler (m_PreviewDir.y, 0, 0) * Quaternion.Euler (0, m_PreviewDir.x, 0);
		Mesh mesh = s_Meshes[m_SelectedMesh];
		if (viewType == PreviewType.Plane)
			mesh = s_PlaneMesh;

		m_PreviewUtility.DrawMesh (mesh, Vector3.zero, rot, mat, 0);
		bool oldFog = RenderSettings.fog;
		Unsupported.SetRenderSettingsUseFogNoDirty (false);
		m_PreviewUtility.m_Camera.Render ();
		Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
		InternalEditorUtility.RemoveCustomLighting ();
	}

	public sealed override bool HasPreviewGUI()
	{
		return true;
	}

	public override bool RequiresConstantRepaint()
	{
		return m_TimeUpdate == 1;
	}
	
	public override void OnPreviewGUI (Rect r, GUIStyle background)
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture)
		{
			if (Event.current.type == EventType.Repaint)
				EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "Material preview \nnot available");
			return;
		}

		Init ();

		var mat = target as Material;
		var viewType = GetPreviewType(mat);

		if (DoesPreviewAllowRotation(viewType))
			m_PreviewDir = PreviewGUI.Drag2D (m_PreviewDir, r);

		if (Event.current.type != EventType.Repaint)
			return;
		
		m_PreviewUtility.BeginPreview (r,  background);
		
		DoRenderPreview();
		
		Texture renderedTexture = m_PreviewUtility.EndPreview ();
		GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
	}
	
	public virtual void OnEnable ()
	{
		m_Shader = serializedObject.FindProperty ("m_Shader");
		
		Undo.undoRedoPerformed += UndoRedoPerformed;
		
		PropertiesChanged ();
	}
	
	public virtual void UndoRedoPerformed ()
	{
		// Undo could've restored old shader which might lead to change in custom editor class
		// therefore we need to rebuild inspector
		if (ActiveEditorTracker.sharedTracker != null)
			ActiveEditorTracker.sharedTracker.ForceRebuild ();

		PropertiesChanged ();
	}
	public virtual void OnDisable ()
	{
		if (m_PreviewUtility != null)
		{
			m_PreviewUtility.Cleanup ();
			m_PreviewUtility = null;
		}
		
		Undo.undoRedoPerformed -= UndoRedoPerformed;
	}
	
	// Handle dragging of material onto renderers
	internal void OnSceneDrag(SceneView sceneView)
	{
		Event evt = Event.current;

		if (evt.type == EventType.Repaint)
			return;

		var go = HandleUtility.PickGameObject (evt.mousePosition, false);
		
		var canAccept = go && go.renderer;
		if (!canAccept)
			return;
				
		switch (evt.type)
		{
			case EventType.DragUpdated:
				DragAndDrop.visualMode = DragAndDropVisualMode.Copy;

				Undo.RecordObject (go.renderer, "Set Material");
				go.renderer.sharedMaterial = target as Material;
				
				evt.Use();
				break;
				
			case EventType.DragPerform:
				DragAndDrop.AcceptDrag ();

				Undo.RecordObject (go.renderer, "Set Material");
				go.renderer.sharedMaterial = target as Material;
				evt.Use ();
				break;
		}
	}
}

} // namespace UnityEditor
