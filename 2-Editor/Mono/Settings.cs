using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System;
using System.Globalization;

namespace UnityEditor {

internal interface IPrefType {
	string ToUniqueString();
	void FromUniqueString(string sstr);	
}

internal class PrefColor : IPrefType {
	string m_name;
	Color m_color;
	Color m_DefaultColor;
	
	public PrefColor() { }
	
	public PrefColor (string name, float defaultRed, float defaultGreen, float defaultBlue, float defaultAlpha) {
		this.m_name = name;
		this.m_color = this.m_DefaultColor = new Color (defaultRed, defaultGreen, defaultBlue, defaultAlpha);
		PrefColor pk = Settings.Get(name, this);		
		this.m_name = pk.Name;
		this.m_color = pk.Color;		
	}
	public Color Color { get { return m_color; } set { m_color = value; } }
	public string Name { get { return m_name; } }
	
	public static implicit operator Color(PrefColor pcolor) { return pcolor.Color; } 	
	
	public string ToUniqueString() {
		return String.Format(System.Globalization.CultureInfo.InvariantCulture, "{0};{1};{2};{3};{4}", m_name, Color.r, Color.g, Color.b, Color.a);
		//return m_name + ";" + Color.r.ToString(System.Globalization.CultureInfo.InvariantCulture.NumberFormat) + ";" + Color.g + ";" + Color.b + ";" + Color.a;
	}
	
	public void FromUniqueString(string s) {
		string[] split = s.Split(';');
		if (split.Length != 5)
		{
			Debug.LogError("Parsing PrefColor failed");
			return;
		}
		
		m_name = split[0];
		split[1] = split[1].Replace(',', '.');
		split[2] = split[2].Replace(',', '.');
		split[3] = split[3].Replace(',', '.');
		split[4] = split[4].Replace(',', '.');
		float r, g, b, a;
 		bool success = float.TryParse(split[1], NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out r);
		success &= float.TryParse(split[2], NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out g);
		success &= float.TryParse(split[3], NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out b);
		success &= float.TryParse(split[4], NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out a);

		if (success)
		{
			m_color = new Color(r, g, b, a);
		}
		else
		{
			Debug.LogError("Parsing PrefColor failed");
		}
	}

	internal void ResetToDefault ()
	{
		m_color = m_DefaultColor;
	}
	
}

internal class PrefKey : IPrefType {
	
	public PrefKey() { }
	
	public PrefKey ( string name, string shortcut )
	{
		this.m_name = name;
		this.m_event = Event.KeyboardEvent(shortcut);
		this.m_DefaultShortcut = shortcut;
		PrefKey pk = Settings.Get(name, this);		
		this.m_name = pk.Name;
		this.m_event = pk.KeyboardEvent;	
	}
		
	public static implicit operator Event(PrefKey pkey) { return pkey.m_event; }
	
	public string Name { get { return m_name; } }
	
	public Event  KeyboardEvent { 
		get { return m_event; } 
		set { m_event = value; } 
	}
	
	private string m_name;	
	private Event  m_event;
	private string m_DefaultShortcut;
	
	public string ToUniqueString() { 
		string s = m_name + ";" + (m_event.alt?"&":"") + (m_event.command?"%":"") + (m_event.shift?"#":"") + (m_event.control?"^":"") + m_event.keyCode;
		return s;
	}
	
	public bool activated { get 
		{ 
			return (Event.current.Equals ((Event)this));
		}
	}
		
	
	public void FromUniqueString(string s) {
		int i = s.IndexOf(";");
		if (i < 0)
		{
			Debug.LogError("Malformed string in Keyboard preferences");
			return;
		}
		m_name = s.Substring(0,i);
		m_event = Event.KeyboardEvent (s.Substring(i+1)); 
	}

	internal void ResetToDefault ()
	{
		m_event = Event.KeyboardEvent (m_DefaultShortcut);
	}
}
 

internal class Settings {
	static SortedList<string, object> m_Prefs = new SortedList<string, object>();
	
	static internal T Get<T>(string name, T defaultValue)
		where T : IPrefType, new()
	{
		if (defaultValue == null)
			throw new System.ArgumentException("default can not be null", "defaultValue");
		if (m_Prefs.ContainsKey(name))
			return (T)m_Prefs[name];
		else
		{
			string sstr = EditorPrefs.GetString(name, "");
			if (sstr == "")
			{
				Set(name, defaultValue);
				return defaultValue;
			}
			else
			{
				defaultValue.FromUniqueString(sstr);
				Set(name, defaultValue);
				return defaultValue;
			}
		}		
	} 
	
	
	static internal void Set<T>(string name, T value)
		where T : IPrefType
	{
		EditorPrefs.SetString(name, value.ToUniqueString());
		m_Prefs[name] = value;
	}	

	static internal IEnumerable <KeyValuePair<string, T>> Prefs<T>() 
		where T : IPrefType
	{ 		
		foreach (KeyValuePair<string, object> kvp in m_Prefs)
		{
			if (kvp.Value is T)
				yield return new KeyValuePair<string, T>(kvp.Key, (T)kvp.Value);
		}
		
	}
	
}

internal class SavedInt
{
	int m_Value;
	string m_Name;
	public SavedInt (string name, int value)
	{
		m_Name = name;
		m_Value = EditorPrefs.GetInt (name, value);
	}
	
	public int value { 
		get { return m_Value; }
		set {
			if (m_Value == value)
				return;
			m_Value = value;
			EditorPrefs.SetInt (m_Name, value);
		}
	}
	
	public static implicit operator int (SavedInt s) 
	{
		return s.value;
	}
}

internal class SavedFloat
{
	float m_Value;
	string m_Name;
	public SavedFloat (string name, float value)
	{
		m_Name = name;
		m_Value = EditorPrefs.GetFloat (name, value);
	}
	
	public float value {
		get { return m_Value; }
		set { 
			if (m_Value == value)
				return;
			m_Value = value;
			EditorPrefs.SetFloat (m_Name, value);
		}
	}
	
	public static implicit operator float (SavedFloat s) 
	{
		return s.value;
	}
}

internal class SavedBool
{
	bool m_Value;
	string m_Name;
	public SavedBool (string name, bool value)
	{
		m_Name = name;
		m_Value = EditorPrefs.GetBool (name, value);
	}
	
	public bool value { 
		get { return m_Value; }
		set {
			if (m_Value == value)
				return;
			m_Value = value;
			EditorPrefs.SetBool (m_Name, value);
		}
	}
	
	public static implicit operator bool (SavedBool s) 
	{
		return s.value;
	}
}

}