using System.Linq;
using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System;

namespace UnityEditor {

internal class AnimationEventPopup : EditorWindow
{   
	GameObject m_Root;
	AnimationClip m_Clip;
	int    m_EventIndex;
	string m_LogicEventName;
    AnimationClipInfoProperties m_ClipInfo;
	private EditorWindow m_Owner;

    public AnimationClipInfoProperties clipInfo { get { return m_ClipInfo; } set { m_ClipInfo = value; } }

	private int eventIndex
	{
		get { return m_EventIndex; }
		set
		{
			if (m_EventIndex != value)
				m_LogicEventName = string.Empty;

			m_EventIndex = value;
		}
	}

  

	public const string kLogicGraphEventFunction = "LogicGraphEvent";
	const string kAmbiguousPostFix = " (Function Is Ambiguous)";
	const string kNotSupportedPostFix = " (Function Not Supported)";
	const string kNoneSelected = "(No Function Selected)";
	
	static internal void InitWindow (AnimationEventPopup popup)
	{
		popup.minSize = new Vector2(400, 140);
		popup.maxSize = new Vector2(400, 140);
		popup.title = EditorGUIUtility.TextContent ("UnityEditor.AnimationEventPopup").text;
	}
	
	static internal void Edit (GameObject root, AnimationClip clip, int index, EditorWindow owner)
	{
		AnimationEventPopup popup;

		UnityEngine.Object[] wins = Resources.FindObjectsOfTypeAll (typeof (AnimationEventPopup));
		popup = wins.Length > 0 ? (AnimationEventPopup)(wins[0]) : null;
		if (popup == null)
		{
			popup = EditorWindow.GetWindow<AnimationEventPopup>(true) as AnimationEventPopup;
			InitWindow(popup);
		}

		popup.m_Root = root;
		popup.m_Clip = clip;
		popup.eventIndex = index;
		popup.m_Owner = owner;
		popup.Repaint();
	}

	static internal void Edit(AnimationClipInfoProperties clipInfo, int index)
    {
        AnimationEventPopup popup;

        UnityEngine.Object[] wins = Resources.FindObjectsOfTypeAll(typeof(AnimationEventPopup));
        popup = wins.Length > 0 ? (AnimationEventPopup)(wins[0]) : null;
        if (popup == null)
        {
            popup = EditorWindow.GetWindow<AnimationEventPopup>(true) as AnimationEventPopup;
            InitWindow(popup);
        }

        popup.m_Root = null;
        popup.m_Clip = null;
        popup.m_ClipInfo = clipInfo;
        popup.eventIndex = index;
		popup.Repaint();
    }

	static internal void UpdateSelection (GameObject root, AnimationClip clip, int index, EditorWindow owner)
	{
		AnimationEventPopup popup;

		// Update only if the window exist
		UnityEngine.Object[] wins = Resources.FindObjectsOfTypeAll (typeof (AnimationEventPopup));		
		popup = wins.Length > 0 ? (AnimationEventPopup)(wins[0]) : null;
		if (popup == null)
			return;

		popup.m_Root = root;
		popup.m_Clip = clip;
		popup.eventIndex = index;
		popup.m_Owner = owner;
		popup.Repaint ();
	}

	static internal int Create (GameObject root, AnimationClip clip, float time, EditorWindow owner)
	{
		AnimationEvent evt = new AnimationEvent ();
		evt.time = time;
		
		// Or add a new one
		AnimationEvent[] events = AnimationUtility.GetAnimationEvents (clip);
		int index = InsertAnimationEvent(ref events, clip, evt);

		AnimationEventPopup popup = EditorWindow.GetWindow<AnimationEventPopup>(true) as AnimationEventPopup;
		InitWindow(popup);
		popup.m_Root = root;
		popup.m_Clip = clip;
		popup.eventIndex = index;
		popup.m_Owner = owner;
		
		return index;
	}
	
	static internal void ClosePopup ()
	{
		AnimationEventPopup popup;

		UnityEngine.Object[] wins = Resources.FindObjectsOfTypeAll (typeof (AnimationEventPopup));
		popup = wins.Length > 0 ? (AnimationEventPopup)(wins[0]) : null;
		if (popup != null)
		{
			popup.Close();
		}
	}
	
	public static string FormatEvent (GameObject root, AnimationEvent evt)
	{
		if (string.IsNullOrEmpty(evt.functionName))
			return kNoneSelected;

		if (IsLogicGraphEvent(evt))
			return FormatLogicGraphEvent(evt);

		foreach (var behaviour in root.GetComponents<MonoBehaviour>())
		{
			var type = behaviour.GetType();
			if (type == typeof (MonoBehaviour) || 
				(type.BaseType != null && type.BaseType.Name == "GraphBehaviour"))
				continue;

			return FormatRegularEvent(type, evt);
		}

		return "Error!";
	}

	private static string FormatLogicGraphEvent(AnimationEvent evt)
	{
		return kLogicGraphEventFunction + FormatEventArguments(new[] {typeof (string)}, evt);
	}

	private static string FormatRegularEvent(Type type, AnimationEvent evt)
	{
		var method = type.GetMethod(evt.functionName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly);
		if (method == null)
			return evt.functionName + kNotSupportedPostFix;

		var name = method.Name;
		if (!IsSupportedMethodName (name))
			return evt.functionName + kNotSupportedPostFix;

		var parameters = from paramInfo in method.GetParameters() select paramInfo.ParameterType;
		return evt.functionName + FormatEventArguments(parameters, evt);
	}

	private static bool IsSupportedMethodName(string name)
	{
		if (name == "Main" || name == "Start" || name == "Awake" || name == "Update")
			return false;

		return true;
	}

	private static string FormatEventArguments(IEnumerable<Type> paramTypes, AnimationEvent evt)
	{
		if (!paramTypes.Any())
			return " ( )";

		if (paramTypes.Count() > 1)
			return kNotSupportedPostFix;

		var paramType = paramTypes.First();

		if (paramType == typeof(string))
			return " ( \"" + evt.stringParameter + "\" )";

		if (paramType == typeof(float))
			return " ( " + evt.floatParameter + " )";

		if (paramType == typeof(int))
			return " ( " + evt.intParameter + " )";

		if (paramType == typeof(int))
			return " ( " + evt.intParameter + " )";

		if (paramType.IsEnum)
			return " ( " + paramType.Name + "." + Enum.GetName(paramType, evt.intParameter) + " )";

		if (paramType == typeof(AnimationEvent))
			return " ( "
				+ evt.floatParameter + " / "
				+ evt.intParameter + " / \""
				+ evt.stringParameter + "\" / "
				+ (evt.objectReferenceParameter == null ? "null" : evt.objectReferenceParameter.name) + " )";

		if (paramType.IsSubclassOf(typeof(UnityEngine.Object)) || paramType == typeof(UnityEngine.Object))
			return " ( " + (evt.objectReferenceParameter == null ? "null" : evt.objectReferenceParameter.name) + " )";

		return kNotSupportedPostFix;
	}

	static void CollectSupportedMethods (GameObject root, out List<string> supportedMethods, out List<Type> supportedMethodsParameter)
	{
		supportedMethods = new List<string>();
		supportedMethodsParameter = new List<Type>();
		MonoBehaviour[] behaviours = root.GetComponents<MonoBehaviour>();
		HashSet<string> ambiguousMethods = new HashSet<string>();
		
		foreach (MonoBehaviour behaviour in behaviours)
		{
			if (behaviour == null)
				continue;
			
			Type type = behaviour.GetType();
			while (type != typeof(MonoBehaviour) && type != null)
			{
				MethodInfo[] methods = type.GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly);
				for (int i=0;i<methods.Length;i++)
				{
					MethodInfo method = methods[i];
					string name = method.Name;

					if (!IsSupportedMethodName(name))
						continue;
					
					ParameterInfo[] parameters = method.GetParameters();
					if (parameters.Length > 1)
						continue;
					
					if (parameters.Length == 1)
					{
						Type paramType = parameters[0].ParameterType;
						if (paramType == typeof(string) || paramType == typeof(float) || paramType == typeof(int) || paramType == typeof(AnimationEvent) || paramType == typeof(UnityEngine.Object) || paramType.IsSubclassOf(typeof(UnityEngine.Object)) || paramType.IsEnum)
							supportedMethodsParameter.Add(paramType);
						else
							continue;
					}
					else
					{
						supportedMethodsParameter.Add(null);
					}
					if (supportedMethods.Contains(name))
					{
						ambiguousMethods.Add(name);
					}
					supportedMethods.Add(name);
				}
				type = type.BaseType;
			}
		}

		// Since AnimationEvents only stores method name, it can't handle functions with multiple overloads 
		// So we remove all the ambiguous methods (overloads) from the list
		foreach (string ambiguousMethod in ambiguousMethods)
		{
			for(int i = 0; i < supportedMethods.Count; i++)
			{
				if(supportedMethods[i].Equals(ambiguousMethod))
				{
					supportedMethods.RemoveAt(i);
					supportedMethodsParameter.RemoveAt(i);
					i--;
				}
			}
		}

#if UNITY_LOGIC_GRAPH
		AddLogicGraphEventFunction(supportedMethods, supportedMethodsParameter);
#endif
	}

	static internal int InsertAnimationEvent (ref AnimationEvent[] events, AnimationClip clip, AnimationEvent evt)
	{
		Undo.RegisterCompleteObjectUndo (clip, "Add Event");
		
		// Or add a new one
		int insertIndex = events.Length;
		for (int i=0;i<events.Length;i++)
		{
			if (events[i].time > evt.time)	
			{
				insertIndex = i;
				break;
			}
		}

		ArrayUtility.Insert(ref events, insertIndex, evt);
		AnimationUtility.SetAnimationEvents (clip, events);
		
		events = AnimationUtility.GetAnimationEvents(clip);
		if (events[insertIndex].time != evt.time || events[insertIndex].functionName != events[insertIndex].functionName)
			Debug.LogError("Failed insertion");
		
		return insertIndex;
	}

	public void OnGUI ()
	{
		AnimationEvent[] events = null;
        if (m_Clip != null)
            events = AnimationUtility.GetAnimationEvents(m_Clip);
        else if (m_ClipInfo != null)
            events = m_ClipInfo.GetEvents();
		
		if (events == null ||eventIndex < 0 || eventIndex >= events.Length)
			return;	
		
		GUI.changed = false;

		var evt = events[eventIndex];

	    if (m_Root)
	    {
	        // Edit function name
	        List<string> methods;
	        List<Type> parameters;
	        CollectSupportedMethods(m_Root, out methods, out parameters);

	        var methodsFormatted = new List<string>(methods.Count);

	        for (int i = 0; i < methods.Count; i++)
	        {
	            string postFix = " ( )";
	            if (parameters[i] != null)
	            {
	                if (parameters[i] == typeof (float))
	                    postFix = " ( float )";
	                else if (parameters[i] == typeof (int))
	                    postFix = " ( int )";
	                else
	                    postFix = string.Format(" ( {0} )", parameters[i].Name);
	            }

	            methodsFormatted.Add(methods[i] + postFix);
	        }


	        int notSupportedIndex = methods.Count;
	        int selected = methods.IndexOf(evt.functionName);
	        if (selected == -1)
	        {
	            selected = methods.Count;
	            methods.Add(evt.functionName);

	            if (string.IsNullOrEmpty(evt.functionName))
	                methodsFormatted.Add(kNoneSelected);
	            else
	                methodsFormatted.Add(evt.functionName + kNotSupportedPostFix);

	            parameters.Add(null);
	        }

			EditorGUIUtility.labelWidth = 130;

	        var wasSelected = selected;
	        selected = EditorGUILayout.Popup("Function: ", selected, methodsFormatted.ToArray());
	        if (wasSelected != selected && selected != -1 && selected != notSupportedIndex)
	        {
	            evt.functionName = methods[selected];
	            evt.stringParameter = IsLogicGraphEvent(evt) ?
	                                      GetEventNameForLogicGraphEvent(events, evt) : string.Empty;
	        }

	        var selectedParameter = parameters[selected];

	        if (selectedParameter != null)
	        {
	            EditorGUILayout.Space();
	            if (selectedParameter == typeof (AnimationEvent))
	                EditorGUILayout.PrefixLabel("Event Data");
	            else
	                EditorGUILayout.PrefixLabel("Parameters");

	            if (IsLogicGraphEvent(evt))
	                DoEditLogicGraphEventParameters(evt);
	            else
	                DoEditRegularParameters(evt, selectedParameter);
	        }
	    }
	    else
	    {
	        evt.functionName = EditorGUILayout.TextField(new GUIContent("Function"), evt.functionName);
	        DoEditRegularParameters(evt, typeof(AnimationEvent));
	    }

	    if (GUI.changed)
	    {
	        if (m_Clip != null)
	        {
				Undo.RegisterCompleteObjectUndo (m_Clip, "Animation Event Change");
	            AnimationUtility.SetAnimationEvents(m_Clip, events);
	        }            
	        else if (m_ClipInfo != null)
            {
                m_ClipInfo.SetEvent(m_EventIndex, evt);
            }            
		}
	}

	static bool EscapePressed()
	{
		return (Event.current.type == EventType.keyDown && Event.current.keyCode == KeyCode.Escape);
	}

	static bool EnterPressed()
	{
		return (Event.current.type == EventType.keyDown && Event.current.keyCode == KeyCode.Return);
	}

	private void DoEditLogicGraphEventParameters (AnimationEvent evt)
	{
		if (string.IsNullOrEmpty(m_LogicEventName))
			m_LogicEventName = evt.stringParameter;

		var enter = EnterPressed ();

		m_LogicEventName = EditorGUILayout.TextField("Event name", m_LogicEventName);

		if (m_LogicEventName == evt.stringParameter || m_LogicEventName.Trim() == string.Empty)
			return;

		GUILayout.BeginHorizontal();
		GUILayout.FlexibleSpace();
		if (GUILayout.Button("Set", EditorStyles.miniButton) || enter)
		{
			RenameAllReferencesToTheLogicGraphAnimationEventInCurrentScene (
				m_Root.GetComponent (typeof (Animation)) as Animation, evt.stringParameter, m_LogicEventName);
			evt.stringParameter = m_LogicEventName;
			LogicGraphEventParameterEditingDone (evt);
			GUI.changed = true;
		}

		if (GUILayout.Button("Cancel", EditorStyles.miniButton) || EscapePressed())
			LogicGraphEventParameterEditingDone(evt);

		GUILayout.EndHorizontal();
	}

	private static void RenameAllReferencesToTheLogicGraphAnimationEventInCurrentScene (Animation animation, string oldName, string newName)
	{
		//FIXME: logic graph references UnityEditor, but not the other way around, so we don't know anything about logic graph here, maybe we should?
		var lgAsm = AppDomain.CurrentDomain.GetAssemblies ().Where (
			asm => asm.GetName ().Name == "UnityEditor.Graphs.LogicGraph").FirstOrDefault();
		if (lgAsm == null)
			throw new Exception("Could not find the logic graph assembly in loaded assemblies.");

		var type = lgAsm.GetType("UnityEngine.Graphs.LogicGraph.OnAnimationEventNode");
		if (type == null)
			throw new Exception("Failed to find type 'OnAnimationEventNode'.");

		var method = type.GetMethod("AnimationEventNameChanged");
		if (method == null)
			throw new Exception("Could not find 'AnimationEventNameChanged' method.");

		method.Invoke(null, new object[] { animation, oldName, newName });
	}

	private void LogicGraphEventParameterEditingDone (AnimationEvent evt)
	{
		GUIUtility.keyboardControl = 0;
		m_LogicEventName = string.Empty;
		Event.current.Use();
	}

	private static void DoEditRegularParameters (AnimationEvent evt, Type selectedParameter)
	{
		if (selectedParameter == typeof(AnimationEvent) || selectedParameter == typeof(float))
			evt.floatParameter = EditorGUILayout.FloatField("Float", evt.floatParameter);

		if (selectedParameter.IsEnum)
			evt.intParameter = EnumPopup("Enum", selectedParameter, evt.intParameter);
		else if (selectedParameter == typeof(AnimationEvent) || selectedParameter == typeof(int))
			evt.intParameter = EditorGUILayout.IntField("Int", evt.intParameter);

		if (selectedParameter == typeof(AnimationEvent) || selectedParameter == typeof(string))
			evt.stringParameter = EditorGUILayout.TextField("String", evt.stringParameter);
			
		if (selectedParameter == typeof(AnimationEvent) || selectedParameter.IsSubclassOf(typeof(UnityEngine.Object)) || selectedParameter == typeof(UnityEngine.Object))
		{
			Type type = typeof (UnityEngine.Object);
			if (selectedParameter != typeof (AnimationEvent))
				type = selectedParameter;

			bool allowSceneObjects = false;
			evt.objectReferenceParameter = EditorGUILayout.ObjectField(ObjectNames.NicifyVariableName(type.Name), evt.objectReferenceParameter, type, allowSceneObjects);
		}
	}

	private static string GetEventNameForLogicGraphEvent (IEnumerable<AnimationEvent> events, AnimationEvent animEvent)
	{
		const string baseName = "LogicGraphEvent";

		for (var i = 1; i < 1000; i++)
		{
			var name = baseName + i;
			if (!events.Any(evt => IsLogicGraphEvent(evt) && evt.stringParameter == name))
				return animEvent.stringParameter = baseName + i;
		}

		return string.Empty;
	}

	private static void AddLogicGraphEventFunction(List<string> methods, List<Type> parameters)
	{
		methods.Insert(0, kLogicGraphEventFunction);
		parameters.Insert(0, typeof(string));
	}

	private static bool IsLogicGraphEvent(AnimationEvent evt)
	{
		return evt.functionName == kLogicGraphEventFunction;
	}

	public static int EnumPopup (string label, Type enumType, int selected)
	{
		if (!enumType.IsEnum)
			throw new Exception("parameter _enum must be of type System.Enum");		

		string[] enumStrings = System.Enum.GetNames(enumType);		
		int i = System.Array.IndexOf(enumStrings, Enum.GetName(enumType, selected) );
		
		i = EditorGUILayout.Popup(label, i, enumStrings, EditorStyles.popup);	
		
		if (i == -1)
			return selected;
		else
		{
			System.Enum res = (System.Enum)Enum.Parse(enumType, enumStrings[i]);
			return Convert.ToInt32(res);
		}
	}

	private void OnDestroy()
	{
		if (m_Owner)
			m_Owner.Focus ();
	}
}

} // namespace
