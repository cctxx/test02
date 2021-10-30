using System;
using System.Linq;
using System.Collections.Generic;
using System.Reflection;
using UnityEngine;

namespace UnityEditor
{

// Tells a custom [[PropertyDrawer]] which run-time [[Serializable]] class or [[PropertyAttribute]] it's a drawer for.
[System.AttributeUsage (AttributeTargets.Class, Inherited = false, AllowMultiple = true)]
public sealed class CustomPropertyDrawer : Attribute
{
	internal System.Type type;
	internal bool useForChildren;
	
	// Tells a PropertyDrawer class which run-time class or attribute it's a drawer for.
	public CustomPropertyDrawer (System.Type type)
	{
		this.type = type;
	}
		
	// Tells a PropertyDrawer class which run-time class or attribute it's a drawer for.
	public CustomPropertyDrawer (System.Type type, bool useForChildren)
	{
		this.type = type;
		this.useForChildren = useForChildren;
	}
}

// Base class to derive custom property drawers from. Use this to create custom drawers for your own [[Serializable]] classes or for script variables with custom [[PropertyAttribute]]s.
public abstract class PropertyDrawer
{
	internal struct DrawerKeySet
	{
		public Type drawer;
		public Type type;
	}

	// Internal API members
	private PropertyAttribute m_Attribute;
	private FieldInfo m_FieldInfo;
	internal static Stack<PropertyDrawer> s_DrawerStack = new Stack<PropertyDrawer> ();
	internal static Dictionary<Type, DrawerKeySet> s_DrawerTypeForType = null;
	internal static Dictionary<int, PropertyDrawer> s_PropertyDrawers = new Dictionary<int, PropertyDrawer> ();
	internal static Dictionary<string, PropertyAttribute> s_BuiltinAttributes = null;
	
	// Public API
	
	// The [[PropertyAttribute]] for the property. Not applicable for custom class drawers. (RO)
	public PropertyAttribute attribute { get { return m_Attribute; } }
	
	// The reflection FieldInfo for the member this property represents. (RO)
	public FieldInfo fieldInfo { get { return m_FieldInfo; } }
	
	internal void OnGUISafe (Rect position, SerializedProperty property, GUIContent label)
	{
		s_DrawerStack.Push (this);
		OnGUI (position, property, label);
		s_DrawerStack.Pop ();
	}
	
	// Override this method to make your own GUI for the property.
	public virtual void OnGUI (Rect position, SerializedProperty property, GUIContent label)
	{
		EditorGUI.LabelField (position, label, EditorGUIUtility.TempContent ("No GUI Implemented"));
	}
	
	internal float GetPropertyHeightSafe (SerializedProperty property, GUIContent label)
	{
		s_DrawerStack.Push (this);
		float height = GetPropertyHeight (property, label);
		s_DrawerStack.Pop ();
		return height;
	}
	
	// Override this method to specify how tall the GUI for this field is in pixels.
	public virtual float GetPropertyHeight (SerializedProperty property, GUIContent label)
	{
		return EditorGUI.kSingleLineHeight;
	}
	
	// Private implementation
	
	private static void PopulateBuiltinAttributes ()
	{
		s_BuiltinAttributes = new Dictionary<string, PropertyAttribute> ();
		
		AddBuiltinAttribute ("Label", "m_Text", new MultilineAttribute (5));
		// Example: Make Orthographic Size in Camera component be in range between 0 and 1000
		//AddBuiltinAttribute ("Camera", "m_OrthographicSize", new RangeAttribute (0, 1000));
	}
	
	private static void AddBuiltinAttribute (string componentTypeName, string propertyPath, PropertyAttribute attr)
	{
		string key = componentTypeName + "_" + propertyPath;
		s_BuiltinAttributes.Add (key, attr);
	}
	
	private static PropertyAttribute GetBuiltinAttribute (SerializedProperty property)
	{
		if (property.serializedObject.targetObject == null)
			return null;
		Type t = property.serializedObject.targetObject.GetType ();
		if (t == null)
			return null;
		string attrKey = t.Name + "_" + property.propertyPath;
		PropertyAttribute attr = null;
		s_BuiltinAttributes.TryGetValue (attrKey, out attr);
		return attr;
	}
	
	// Called on demand
	private static void BuildDrawerTypeForTypeDictionary ()
	{
		s_DrawerTypeForType = new Dictionary<Type, DrawerKeySet> ();

		var loadedTypes = AppDomain.CurrentDomain.GetAssemblies().SelectMany( x => AssemblyHelper.GetTypesFromAssembly(x)).ToArray();

		foreach (var type in EditorAssemblies.SubclassesOf(typeof(PropertyDrawer)))
		{
		//	Debug.Log("Drawer: " + type);
			object[] attrs = type.GetCustomAttributes (typeof (CustomPropertyDrawer), true);
			foreach (CustomPropertyDrawer editor in attrs)
			{
				//Debug.Log("Base type: " + editor.type);
				s_DrawerTypeForType[editor.type] = new DrawerKeySet()
				{
					drawer = type,
					type = editor.type
				};
				
				if (!editor.useForChildren)
					continue;
				
				var candidateTypes = loadedTypes.Where(x => x.IsSubclassOf(editor.type));
				foreach (var candidateType in candidateTypes)
				{
					//Debug.Log("Candidate Type: "+ candidateType);
					if (s_DrawerTypeForType.ContainsKey(candidateType)
						&& (editor.type.IsAssignableFrom(s_DrawerTypeForType[candidateType].type)))
					{
					//	Debug.Log("skipping");
						continue;
					}

					//Debug.Log("Setting");
					s_DrawerTypeForType[candidateType] = new DrawerKeySet()
					{
						drawer = type,
						type = editor.type
					};
				}
			}
		}
	}

	private static Type GetDrawerTypeForType (Type type)
	{
		if (s_DrawerTypeForType == null)
			BuildDrawerTypeForTypeDictionary ();
		
		DrawerKeySet drawerType;
		s_DrawerTypeForType.TryGetValue (type, out drawerType);
		if (drawerType.drawer != null)
			return drawerType.drawer;

		// now check for base generic versions of the drawers...
		if (type.IsGenericType)
			s_DrawerTypeForType.TryGetValue(type.GetGenericTypeDefinition(), out drawerType);

		return drawerType.drawer;
	}
	
	private static int GetPropertyHash (SerializedProperty property)
	{
		if (property.serializedObject.targetObject == null)
			return 0;
			
		// For efficiency, ignore indices inside brackets [] in order to make array elements share drawers.
		return property.serializedObject.targetObject.GetInstanceID () ^ property.arrayIndexLessPropertyPath.GetHashCode ();
	}
	
	private static PropertyAttribute GetFieldAttribute (FieldInfo field)
	{
		if (field == null)
			return null;
		
		object[] attrs = field.GetCustomAttributes (typeof (PropertyAttribute), true);
		if (attrs != null && attrs.Length > 0)
			return (PropertyAttribute)attrs[0];
		
		return null;
	}
	
	private static FieldInfo GetFieldInfoFromProperty (SerializedProperty property, out Type type)
	{
		var classType = GetScriptTypeFromProperty (property);
		if (classType == null)
		{
			type = null;
			return null;
		}
		return GetFieldInfoFromPropertyPath (classType, property.propertyPath, out type);
	}
	
	private static Type GetScriptTypeFromProperty (SerializedProperty property)
	{
		SerializedProperty scriptProp = property.serializedObject.FindProperty ("m_Script");
		
		if (scriptProp == null)
			return null;
		
		MonoScript script = scriptProp.objectReferenceValue as MonoScript;
		
		if (script == null)
			return null;
		
		return script.GetClass ();
	}
	
	private static FieldInfo GetFieldInfoFromPropertyPath (Type host, string path, out Type type)
	{
		FieldInfo field = null;
		type = host;
		string[] parts = path.Split ('.');
		for (int i=0; i<parts.Length; i++)
		{
			string member = parts[i];
			
			// Special handling of array elements.
			// The "Array" and "data[x]" parts of the propertyPath don't correspond to any types,
			// so they should be skipped by the code that drills down into the types.
			// However, we want to change the type from the type of the array to the type of the array element before we do the skipping.
			if (i < parts.Length-1 && member == "Array" && parts[i+1].StartsWith ("data["))
			{
				if (type.IsArray)
				{
					type = type.GetElementType ();
				}
				else if (type.IsGenericType && type.GetGenericTypeDefinition () == typeof (List<>))
				{
					type = type.GetGenericArguments ()[0];
				}
				
				// Skip rest of handling for this part ("Array") and the next part ("data[x]").
				i++;
				continue;
			}
			
			// GetField on class A will not find private fields in base classes to A,
			// so we have to iterate through the base classes and look there too.
			// Private fields are relevant because they can still be shown in the Inspector,
			// and that applies to private fields in base classes too.
			FieldInfo foundField = null;
			for (Type currentType = type; foundField == null && currentType != null; currentType = currentType.BaseType)
				foundField = currentType.GetField (member, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

			if (foundField == null)
			{
				type = null;
				return null;
			}

			field = foundField;
			type = field.FieldType;
		}
		return field;
	}
	
	internal static PropertyDrawer GetDrawer (SerializedProperty property)
	{
		if (property == null)
			return null;
		
		// Don't use custom drawers for arrays (except strings, which we don't show as arrays).
		// If there's a PropertyAttribute on an array, we want to apply it to the individual array elements instead.
		// This is the only convenient way we can let the user apply attributes to elements inside an array.
		if (property.isArray && property.propertyType != SerializedPropertyType.String)
			return null;
		
		// Don't use custom drawers in debug mode
		if (property.serializedObject.inspectorMode != InspectorMode.Normal)
			return null;
		
		// If the drawer is cached, use the cached drawer
		PropertyDrawer drawer;
		int key = GetPropertyHash (property);
		if (s_PropertyDrawers.TryGetValue (key, out drawer))
		{
			if (s_DrawerStack.Any () && drawer == s_DrawerStack.Peek ())
				return null;
			return drawer;
		}
		
		Type propertyType = null;
		PropertyAttribute attr = null;
		FieldInfo field = null;
		
		// Determine if SerializedObject target is a script or a builtin type
		UnityEngine.Object target = property.serializedObject.targetObject;
		if ((target is MonoBehaviour) || (target is ScriptableObject))
		{
			// For scripts, use reflection to get FieldInfo for the member the property represents
			field = GetFieldInfoFromProperty (property, out propertyType);
			
			// Use reflection to see if this member has an attribute
			attr = GetFieldAttribute (field);
		}
		else
		{
			// For builtin types, look if we hardcoded an attribute for this property
			// First initialize the hardcoded properties if not already done
			if (s_BuiltinAttributes == null)
				PopulateBuiltinAttributes ();
			
			if (attr == null)
				attr = GetBuiltinAttribute (property);
		}
		
		Type drawerType = null;
		
		// If field has a CustomPropertyDrawer attribute, look for its drawer type
		if (attr != null)
			drawerType = GetDrawerTypeForType (attr.GetType ());
		
		// Field has no CustomPropertyDrawer attribute with matching drawer so look for default drawer for field type
		if (drawerType == null && propertyType != null)
			drawerType = GetDrawerTypeForType (propertyType);
		
		// If we found a drawer type, instantiate the drawer, cache it, and return it.
		if (drawerType != null)
		{
			drawer = (PropertyDrawer)System.Activator.CreateInstance (drawerType);
			drawer.m_Attribute = attr; // will be null by design if default type drawer!
			drawer.m_FieldInfo = field;
			s_PropertyDrawers[key] = drawer;
			return drawer;
		}
		
		// If we didn't find a drawer, cache that we didn't so we won't keep looking for it.
		s_PropertyDrawers[key] = null;
		return null;
	}
}

// Built-in drawers below.
// The drawers don't need to be public API, only the PropertyAttributes that enable them.

[CustomPropertyDrawer (typeof (RangeAttribute))]
internal sealed class RangeDrawer : PropertyDrawer
{
	public override void OnGUI (Rect position, SerializedProperty property, GUIContent label)
	{
		RangeAttribute range = (RangeAttribute)attribute;
		if (property.propertyType == SerializedPropertyType.Float)
			EditorGUI.Slider (position, property, range.min, range.max, label);
		else if (property.propertyType == SerializedPropertyType.Integer)
			EditorGUI.IntSlider (position, property, (int)range.min, (int)range.max, label);
		else
			EditorGUI.LabelField (position, label.text, "Use Range with float or int.");
	}
}

[CustomPropertyDrawer (typeof (MultilineAttribute))]
internal sealed class MultilineDrawer : PropertyDrawer
{
	public override void OnGUI (Rect position, SerializedProperty property, GUIContent label)
	{
		if (property.propertyType == SerializedPropertyType.String)
		{
			label = EditorGUI.BeginProperty (position, label, property);
			
			position = EditorGUI.MultiFieldPrefixLabel (position, 0, label, 1);
			
			EditorGUI.BeginChangeCheck ();
			int oldIndent = EditorGUI.indentLevel;
			EditorGUI.indentLevel = 0; // The MultiFieldPrefixLabel already applied indent, so avoid indent of TextArea itself.
			string newValue = EditorGUI.TextArea (position, property.stringValue);
			EditorGUI.indentLevel = oldIndent;
			if (EditorGUI.EndChangeCheck ())
				property.stringValue = newValue;
			
			EditorGUI.EndProperty ();
		}
		else
			EditorGUI.LabelField (position, label.text, "Use Multiline with string.");
	}
	
	public override float GetPropertyHeight (SerializedProperty property, GUIContent label)
	{
		return EditorGUI.kSingleLineHeight * (((MultilineAttribute)attribute).lines + 1);
	}
}

}
