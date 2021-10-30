using UnityEngine;
using UnityEditor;
using System.Collections;
using System;

namespace UnityEditor {

internal class AnimValueManager {
	long lastTime;
	ArrayList values = new ArrayList();

	public void Add (AnimValue v) 
	{
		values.Add (v);
		v.owner = this;
	}

	public void Remove (AnimValue v)
	{
		values.Remove (v);
	}

	bool updating = false;
	public bool isUpdating
	{
		get { return updating; }
	}
	
	internal void WantsUpdate () 
	{
		if (!updating) 
		{
			lastTime = System.DateTime.Now.Ticks;
			updating = true;
			EditorApplication.update += Update;
		}
	}
	public float speed = 2;
	internal delegate void Callback ();	
	internal Callback callback;
	
	void Update () 
	{
		long now = System.DateTime.Now.Ticks;
		float deltaTime = (now - lastTime) / 10000000.0f;
		lastTime = now;
		bool wantsNextUpdate = false;
		foreach (AnimValue v in values) 
		{
			if (v.wantsUpdate) 
			{
				v.m_LerpPos = Mathf.Clamp01 (v.m_LerpPos + deltaTime * speed * v.speedMultiplier);
				v.Update ();
				wantsNextUpdate |= v.wantsUpdate;
			}
		}
		if (!wantsNextUpdate) 
		{
			EditorApplication.update -= Update;
			updating = false;
		}

		if (callback != null) 
			callback ();			
	}

	public void PrintDebugInfo ()
	{
		Debug.Log("AnimValueManager (" + GetHashCode () + ") values size: " + values.Count);
	}
}

internal abstract class AnimValue {
	internal AnimValueManager owner;
	[System.NonSerialized]
	internal float m_LerpPos = 1;
	protected object m_Source;
	private object m_Target;
	internal bool wantsUpdate { get { return m_LerpPos < 1; } }
	private bool m_EaseOut = true;
	public bool easeOut { get { return m_EaseOut; } set { m_EaseOut = value; } }
	private float m_SpeedMultiplier = 1;
	public float speedMultiplier { get { return m_SpeedMultiplier; } set { m_SpeedMultiplier = value; } }
	internal abstract void Update ();
	
	protected void BeginAnimating (object target, object value) {
		if (target.Equals (value) || (m_Target != null && m_Target.Equals (target)))
		{
			// Still set the target, as it might not be the same just because the value is.
			m_Target = target;
			return;
		}
		m_LerpPos = 0f;
		m_Source = value;
		m_Target = target;

		if (owner != null)
			owner.WantsUpdate ();
		else 
			Debug.Log ("Something is wrong. BeginAnimating with null owner");
	}
	protected float smoothed { get {
		if (!easeOut)
			return m_LerpPos;
		float v = 1 - m_LerpPos;
		return (1 - v * v * v * v);	
	} }
	
	protected object GetTarget (object def) {
		if (m_Target != null)
			return m_Target;
		return def;
	}
	protected void StopAnim (object value) { m_LerpPos = 1f; m_Target = value; }
}

[System.Serializable]
internal class AnimFloat : AnimValue {
	[SerializeField]
	float m_Value;
	public AnimFloat () { m_Value = 0.0F;  }
	public AnimFloat (float val) { m_Value = val; }
	public static implicit operator float(AnimFloat v) { return v.m_Value; }
	public float target { 
		get { return (float)GetTarget (m_Value); } 
		set { BeginAnimating (value, m_Value); }
	}
	public float value {
		get { return (float)m_Value; }
		set { m_Value = value; StopAnim (value); }
	}
	internal override void Update () {
		m_Value = Mathf.Lerp ((float)m_Source, target, smoothed);
	}
}


[System.Serializable]
internal class AnimVector3 : AnimValue {
	[SerializeField]
   	Vector3 m_Value;
	public AnimVector3 () { m_Value = new Vector3(); }
	public AnimVector3 (Vector3 val) { m_Value = val; }
	public static implicit operator Vector3(AnimVector3 v) { return v.m_Value; }
	public Vector3 target { 
		get { return (Vector3)GetTarget (m_Value); } 
		set { BeginAnimating (value, m_Value); }
	}
	public Vector3 value {
		get { return (Vector3)m_Value; }
		set { m_Value = value; StopAnim (value); }
	}
	internal override void Update () {
		m_Value = Vector3.Lerp ((Vector3)m_Source, target, smoothed);
	}
}

[System.Serializable]
internal class AnimBool : AnimValue {
	[SerializeField]
	float m_Value;
	public AnimBool () { m_Value = 0.0f; }
	public AnimBool (bool val) { m_Value = val ? 1.0f : 0.0f; }
	public static implicit operator bool(AnimBool v) { return v.m_Value > .5f; }
	public bool target { 
		get { return (Convert.ToSingle(GetTarget (m_Value))) > .5f; } 
		set { BeginAnimating (value ? 1f: 0f, m_Value); }
	}
	public bool value {
		get { return m_Value > .5f; }
		set { m_Value = value ? 1f : 0f; StopAnim (value); }
	}
	public float faded { get {  return m_Value; } }
	public float Fade (float from, float to) {
		return Mathf.Lerp (from, to, faded);
	}
	internal override void Update () {
		m_Value = Mathf.Lerp ((float)m_Source, target ? 1f : 0f, smoothed);
	}

}

[System.Serializable]
internal class AnimQuaternion : AnimValue {
	[SerializeField]
	Quaternion m_Value;
	public AnimQuaternion () { m_Value = Quaternion.identity; }
	public AnimQuaternion (Quaternion val) { m_Value = val; }
	public static implicit operator Quaternion(AnimQuaternion v) { return v.m_Value; }
	public Quaternion target { 
		get { return (Quaternion)GetTarget (m_Value); } 
		set { if (value != (Quaternion)m_Value) BeginAnimating (value, m_Value); }
	}
	public Quaternion value {
		get { return m_Value; }
		set { m_Value = value; StopAnim (value); }
	}
	internal override void Update () {
		m_Value = Quaternion.Slerp ((Quaternion)m_Source, target, smoothed);
	}
}



} //namespace
