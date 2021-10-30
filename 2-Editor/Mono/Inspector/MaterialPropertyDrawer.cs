using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEngine;
using Object = System.Object;

namespace UnityEditor
{

public abstract class MaterialPropertyDrawer
{	
	internal static Dictionary<string, MaterialPropertyDrawer> s_PropertyDrawers = new Dictionary<string, MaterialPropertyDrawer> ();

	// Public API

	public virtual void OnGUI (Rect position, MaterialProperty prop, string label, MaterialEditor editor)
	{
		EditorGUI.LabelField (position, new GUIContent(label), EditorGUIUtility.TempContent ("No GUI Implemented"));
	}
	public virtual float GetPropertyHeight (MaterialProperty prop, string label, MaterialEditor editor)
	{
		return EditorGUI.kSingleLineHeight;
	}
	public virtual void Apply (MaterialProperty prop)
	{
		// empty base implementation
	}
		
	// Private implementation

	private static string GetPropertyString (Shader shader, string name)
	{
		if (shader == null)
			return string.Empty;
		return shader.GetInstanceID() + "_" + name;
	}

	internal static void InvalidatePropertyCache (Shader shader)
	{
		if (shader == null)
			return;
		string keyStart = shader.GetInstanceID() + "_";
		var toDelete = new List<string>();
		foreach (string key in s_PropertyDrawers.Keys) 
		{
			if (key.StartsWith(keyStart))
				toDelete.Add (key);
		}
		foreach (string key in toDelete)
		{
			s_PropertyDrawers.Remove (key);
		}
	}

	private static MaterialPropertyDrawer CreatePropertyDrawer (Type klass, string argsText)
	{
		// no args -> default constructor
		if (string.IsNullOrEmpty (argsText))
			return Activator.CreateInstance (klass) as MaterialPropertyDrawer;

		// split the argument list by commas
		string[] argStrings = argsText.Split (',');
		var args = new object[argStrings.Length];
		for (var i = 0; i < argStrings.Length; ++i)
		{
			float f;
			string arg = argStrings[i].Trim ();

			// if can parse as a float, use the float; otherwise pass the string
			if (float.TryParse (arg, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out f))
			{
				args[i] = f;
			}
			else
			{
				args[i] = arg;
			}
		}
		return Activator.CreateInstance (klass, args) as MaterialPropertyDrawer;
	}
	
	private static MaterialPropertyDrawer GetShaderPropertyDrawer (Shader shader, string name)
	{
		string text = ShaderUtil.GetShaderPropertyAttribute (shader, name);
		if (string.IsNullOrEmpty(text))
			return null;

		string className = text;
		string args = string.Empty;
		Match match = Regex.Match (text,  @"(\w+)\s*\((.*)\)");
		if (match.Success)
		{
			className = match.Groups[1].Value;
			args = match.Groups[2].Value.Trim();
		}

		//Debug.Log ("looking for class " + className + " args '" + args + "'");
		foreach (var klass in EditorAssemblies.SubclassesOf (typeof (MaterialPropertyDrawer)))
		{
			// When you write [Foo] in shader, get Foo, FooDrawer or MaterialFooDrawer class;
			// "kind of" similar to how C# does attributes.
			if (klass.Name == className ||
				klass.Name == className+"Drawer" ||
				klass.Name == "Material"+className+"Drawer") //@TODO: namespaces?
			{
				try
				{
					return CreatePropertyDrawer (klass, args);
				}
				catch (Exception)
				{
					Debug.LogWarning (string.Format("Failed to create material drawer {0} with arguments '{1}'", className, args));
					return null;
				}
			}
		}

		return null;
	}
	
	internal static MaterialPropertyDrawer GetDrawer (Shader shader, string name)
	{
		if (shader == null)
			return null;

		// Use the cached drawer if available
		MaterialPropertyDrawer drawer;
		string key = GetPropertyString (shader, name);
		if (s_PropertyDrawers.TryGetValue (key, out drawer))
			return drawer;

		// Get the drawer for this shader property		
		drawer = GetShaderPropertyDrawer (shader, name);
		//Debug.Log ("drawer " + drawer);

		// Cache the drawer and return. Cache even if it was null, so we can return
		// later requests fast as well.
		s_PropertyDrawers[key] = drawer;
		return drawer;
	}
}


// --------------------------------------------------------------------------
// Built-in drawers below.


internal class MaterialToggleDrawer : MaterialPropertyDrawer
{
	public readonly string keyword;
	public MaterialToggleDrawer ()
	{
	}
	public MaterialToggleDrawer (string keyword)
	{
		this.keyword = keyword;
	}

	static bool IsPropertyTypeSuitable (MaterialProperty prop)
	{
		return prop.type == MaterialProperty.PropType.Float || prop.type == MaterialProperty.PropType.Range;
	}

	void SetKeyword (MaterialProperty prop, bool on)
	{
		// if no keyword is provided, use <uppercase property name> + "_ON"
		string kw = string.IsNullOrEmpty (keyword) ? prop.name.ToUpperInvariant () + "_ON" : keyword;
		// set or clear the keyword
		foreach (Material material in prop.targets)
		{
			if (on)
				material.EnableKeyword (kw);
			else
				material.DisableKeyword (kw);
		}
	}

	public override float GetPropertyHeight (MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (!IsPropertyTypeSuitable(prop))
		{
			return EditorGUI.kSingleLineHeight*2.5f;
		}
		return base.GetPropertyHeight (prop, label, editor);
	}
	public override void OnGUI (Rect position, MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (!IsPropertyTypeSuitable (prop))
		{
			GUIContent c = EditorGUIUtility.TempContent("Toggle used on a non-float property: " + prop.name,
			                              EditorGUIUtility.GetHelpIcon(MessageType.Warning));
			EditorGUI.LabelField (position, c, EditorStyles.helpBox);
			return;
		}

		EditorGUI.BeginChangeCheck ();

		bool value = (Math.Abs(prop.floatValue) > 0.001f);
		EditorGUI.showMixedValue = prop.hasMixedValue;
		value = EditorGUI.Toggle (position, label, value);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck ())
		{
			prop.floatValue = value ? 1.0f : 0.0f;
			SetKeyword(prop, value);
		}
	}
	public override void Apply (MaterialProperty prop)
	{
		base.Apply (prop);
		if (!IsPropertyTypeSuitable (prop))
			return;

		if (prop.hasMixedValue)
			return;

		SetKeyword (prop, (Math.Abs (prop.floatValue) > 0.001f));
	}
}

internal class MaterialPowerSliderDrawer : MaterialPropertyDrawer
{
	public readonly float power;

	public MaterialPowerSliderDrawer (float power)
	{
		this.power = power;
	}

	public override float GetPropertyHeight (MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (prop.type != MaterialProperty.PropType.Range)
		{
			return EditorGUI.kSingleLineHeight * 2.5f;
		}
		return base.GetPropertyHeight (prop, label, editor);
	}
	public override void OnGUI (Rect position, MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (prop.type != MaterialProperty.PropType.Range)
		{
			GUIContent c = EditorGUIUtility.TempContent ("PowerSlider used on a non-range property: " + prop.name,
										  EditorGUIUtility.GetHelpIcon (MessageType.Warning));
			EditorGUI.LabelField (position, c, EditorStyles.helpBox);
			return;
		}

		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = prop.hasMixedValue;
		float newValue = EditorGUI.PowerSlider (position, label, prop.floatValue, prop.rangeLimits.x, prop.rangeLimits.y, power);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
			prop.floatValue = newValue;
	}
}

internal class MaterialKeywordEnumDrawer : MaterialPropertyDrawer
{
	public readonly string[] keywords;

	public MaterialKeywordEnumDrawer (string kw1) : this (new[] { kw1 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2) : this (new[] { kw1, kw2 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3) : this (new[] { kw1, kw2, kw3 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4) : this (new[] { kw1, kw2, kw3, kw4 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4, string kw5) : this (new[] { kw1, kw2, kw3, kw4, kw5 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4, string kw5, string kw6) : this (new[] { kw1, kw2, kw3, kw4, kw5, kw6 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4, string kw5, string kw6, string kw7) : this (new[] { kw1, kw2, kw3, kw4, kw5, kw6, kw7 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4, string kw5, string kw6, string kw7, string kw8) : this (new[] { kw1, kw2, kw3, kw4, kw5, kw6, kw7, kw8 }) { }
	public MaterialKeywordEnumDrawer (string kw1, string kw2, string kw3, string kw4, string kw5, string kw6, string kw7, string kw8, string kw9) : this (new[] { kw1, kw2, kw3, kw4, kw5, kw6, kw7, kw8, kw9 }) { }
	public MaterialKeywordEnumDrawer (params string[] keywords)
	{
		this.keywords = keywords;
	}

	static bool IsPropertyTypeSuitable(MaterialProperty prop)
	{
		return prop.type == MaterialProperty.PropType.Float || prop.type == MaterialProperty.PropType.Range;
	}

	void SetKeyword (MaterialProperty prop, int index)
	{
		for (int i = 0; i < keywords.Length; ++i)
		{
			string keyword = GetKeywordName (prop.name, keywords[i]);
			foreach (Material material in prop.targets)
			{
				if (index == i)
					material.EnableKeyword (keyword);
				else
					material.DisableKeyword (keyword);
			}
		}		
	}

	public override float GetPropertyHeight (MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (!IsPropertyTypeSuitable(prop))
		{
			return EditorGUI.kSingleLineHeight * 2.5f;
		}
		return base.GetPropertyHeight (prop, label, editor);
	}
	public override void OnGUI (Rect position, MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (!IsPropertyTypeSuitable (prop))
		{
			GUIContent c = EditorGUIUtility.TempContent ("KeywordEnum used on a non-float property: " + prop.name,
										  EditorGUIUtility.GetHelpIcon (MessageType.Warning));
			EditorGUI.LabelField (position, c, EditorStyles.helpBox);
			return;
		}

		EditorGUI.BeginChangeCheck ();

		EditorGUI.showMixedValue = prop.hasMixedValue;
		var value = (int)prop.floatValue;
		value = EditorGUI.Popup (position, label, value, keywords);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck ())
		{
			prop.floatValue = value;
			SetKeyword(prop, value);
		}
	}

	public override void Apply (MaterialProperty prop)
	{
		base.Apply (prop);
		if (!IsPropertyTypeSuitable(prop))
			return;

		if (prop.hasMixedValue)
			return;

		SetKeyword(prop, (int)prop.floatValue);
	}

	// Final keyword name: property name + "_" + display name. Uppercased,
	// and spaces replaced with underscores.
	private static string GetKeywordName (string propName, string name)
	{
		string n = propName + "_" + name;
		return n.Replace (' ', '_').ToUpperInvariant ();
	}
}


internal class MaterialEnumDrawer : MaterialPropertyDrawer
{
	public readonly string[] names;
	public readonly int[] values;

	// Single argument: enum type name; entry names & values fetched via reflection
	public MaterialEnumDrawer(string enumName)
	{
		var loadedTypes = AppDomain.CurrentDomain.GetAssemblies().SelectMany(x => AssemblyHelper.GetTypesFromAssembly(x)).ToArray();
		try
		{
			var enumType = loadedTypes.FirstOrDefault (
				x => x.IsSubclassOf (typeof (Enum)) && (x.Name == enumName || x.FullName == enumName)
			);
			names = Enum.GetNames(enumType);
			var enumVals = Enum.GetValues(enumType);
			values = new int[enumVals.Length];
			for (var i = 0; i < enumVals.Length; ++i)
				values[i] = (int)enumVals.GetValue(i);
		}
		catch (Exception)
		{
			Debug.LogWarning (string.Format ("Failed to create MaterialEnum, enum {0} not found", enumName));
			throw;
		}
	}

	// name,value,name,value,... pairs: explicit names & values
	public MaterialEnumDrawer (string n1, float v1) : this (new[] {n1}, new[] {v1}) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2) : this (new[] { n1, n2 }, new[] { v1, v2 }) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2, string n3, float v3) : this (new[] { n1, n2, n3 }, new[] { v1, v2, v3 }) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2, string n3, float v3, string n4, float v4) : this (new[] { n1, n2, n3, n4 }, new[] { v1, v2, v3, v4 }) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2, string n3, float v3, string n4, float v4, string n5, float v5) : this (new[] { n1, n2, n3, n4, n5 }, new[] { v1, v2, v3, v4, v5 }) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2, string n3, float v3, string n4, float v4, string n5, float v5, string n6, float v6) : this (new[] { n1, n2, n3, n4, n5, n6 }, new[] { v1, v2, v3, v4, v5, v6 }) { }
	public MaterialEnumDrawer (string n1, float v1, string n2, float v2, string n3, float v3, string n4, float v4, string n5, float v5, string n6, float v6, string n7, float v7) : this (new[] { n1, n2, n3, n4, n5, n6, n7 }, new[] { v1, v2, v3, v4, v5, v6, v7 }) { }
	public MaterialEnumDrawer (string[] names, float[] vals)
	{
		this.names = names;
		values = new int[vals.Length];
		for (int i = 0; i < vals.Length; ++i)
			values[i] = (int)vals[i];
	}


	public override float GetPropertyHeight (MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (prop.type != MaterialProperty.PropType.Float && prop.type != MaterialProperty.PropType.Range)
		{
			return EditorGUI.kSingleLineHeight * 2.5f;
		}
		return base.GetPropertyHeight (prop, label, editor);
	}
	public override void OnGUI (Rect position, MaterialProperty prop, string label, MaterialEditor editor)
	{
		if (prop.type != MaterialProperty.PropType.Float && prop.type != MaterialProperty.PropType.Range)
		{
			GUIContent c = EditorGUIUtility.TempContent ("Enum used on a non-float property: " + prop.name,
										  EditorGUIUtility.GetHelpIcon (MessageType.Warning));
			EditorGUI.LabelField (position, c, EditorStyles.helpBox);
			return;
		}

		EditorGUI.BeginChangeCheck ();

		EditorGUI.showMixedValue = prop.hasMixedValue;
		var value = (int)prop.floatValue;
		value = EditorGUI.IntPopup(position, label, value, names, values);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck ())
		{
			prop.floatValue = value;
		}
	}
}

}
