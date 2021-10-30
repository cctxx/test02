using UnityEngine;
using NUnit.Framework; 

namespace UnityEditor
{

// Must be in sync with ParticleSystemCurves.h
internal enum MinMaxCurveState
{
	k_Scalar = 0,
	k_Curve = 1,
	k_TwoCurves = 2,
	k_TwoScalars = 3
};

internal class SerializedMinMaxCurve
{
	public SerializedProperty scalar;
	public SerializedProperty maxCurve;
	public SerializedProperty minCurve;
	public SerializedProperty minCurveFirstKeyValue;
	public SerializedProperty maxCurveFirstKeyValue;
	private SerializedProperty minMaxState;

	public ModuleUI m_Module;			// Module that owns this SerializedMinMaxCurve
	private string m_Name;				// Curve name. Used for creating unique name based on ParticleSystem name + Module name + m_Name
	public GUIContent m_DisplayName;
	private bool m_SignedRange;			// True if curve can have negative values
	public float m_DefaultCurveScalar;	// Used when switching to curve from scalar and scalar is 0. Ensures a valid y axis range (must be positive)
	public float m_RemapValue;			// Used for remap UI values e.g for rotation: state is in radians but we want UI to show it in degrees
	public bool m_AllowConstant;
	public bool m_AllowRandom;
	public float m_MaxAllowedScalar = Mathf.Infinity;


	public SerializedMinMaxCurve(ModuleUI m, GUIContent displayName)
	{
		Init(m, displayName, "curve", false);
	}
	public SerializedMinMaxCurve(ModuleUI m, GUIContent displayName, string name)
	{
		Init(m, displayName, name, false);
	}
	public SerializedMinMaxCurve(ModuleUI m, GUIContent displayName, bool signedRange)
	{
		Init(m, displayName, "curve", signedRange);
	}
	public SerializedMinMaxCurve(ModuleUI m, GUIContent displayName, string name, bool signedRange)
	{
		Init(m, displayName, name, signedRange);
	}

	void Init(ModuleUI m, GUIContent displayName, string uniqueName, bool signedRange)
	{
		m_Module = m;
		m_DisplayName = displayName;
		m_Name = uniqueName;
		m_SignedRange = signedRange;
		m_RemapValue = 1f;
		m_DefaultCurveScalar = 1f;
		m_AllowConstant = true;
		m_AllowRandom = true;
		
		scalar = m.GetProperty(m_Name, "scalar");
		maxCurve = m.GetProperty(m_Name, "maxCurve");
		maxCurveFirstKeyValue = maxCurve.FindPropertyRelative("m_Curve.Array.data[0].value");
		minCurve = m.GetProperty(m_Name, "minCurve");
		minCurveFirstKeyValue = minCurve.FindPropertyRelative("m_Curve.Array.data[0].value");
		minMaxState = m.GetProperty(m_Name, "minMaxState");

		// Reconstruct added curves when we initialize
		if (state == MinMaxCurveState.k_Curve || state == MinMaxCurveState.k_TwoCurves)
		{
			if (m_Module.m_ParticleSystemUI.m_ParticleEffectUI.IsParticleSystemUIVisible(m_Module.m_ParticleSystemUI))
				m.GetParticleSystemCurveEditor().AddCurveDataIfNeeded(GetUniqueCurveName(), CreateCurveData(Color.black));
		}
		m.AddToModuleCurves (maxCurve); // It is enough just to add max
	}

	public MinMaxCurveState state
	{
		get { return (MinMaxCurveState)minMaxState.intValue; }
		set { SetMinMaxState(value); }
	}


	public bool signedRange
	{
		get { return m_SignedRange; }
	}

	public float maxConstant
	{
		// The maxConstant is stored as a normalized value in the first key of maxCurve and then scaled with our scalar
		get
		{
			return maxCurveFirstKeyValue.floatValue * scalar.floatValue;
		}

		set
		{
			value = ClampValueToMaxAllowed (value);

			if (!signedRange)
				value = Mathf.Max(value, 0f);

			float totalMin = minConstant;
			float absMin = Mathf.Abs(totalMin);
			float absMax = Mathf.Abs(value);

			float newScalar = absMax > absMin ? absMax : absMin;
			if (newScalar != scalar.floatValue)
			{
				SetScalarAndNormalizedConstants(newScalar, totalMin, value);
			}
			else
			{
				SetNormalizedConstant (maxCurve, value);
			}
		}
	}

	float ClampValueToMaxAllowed (float val)
	{ 
		if (Mathf.Abs (val) > m_MaxAllowedScalar)
			return m_MaxAllowedScalar * Mathf.Sign (val);
		return val;
	}

	public float minConstant
	{
		// The minConstant is stored as a normalized value in the first key of minCurve and then scaled with our scalar
		get
		{
			return minCurveFirstKeyValue.floatValue * scalar.floatValue;
		}

		set
		{
			value = ClampValueToMaxAllowed(value);
			
			if (!signedRange)
				value = Mathf.Max(value, 0f);

			float totalMax = maxConstant;
			float absMin = Mathf.Abs(value);
			float absMax = Mathf.Abs(totalMax);

			float newScalar = absMax > absMin ? absMax : absMin;
			if (newScalar != scalar.floatValue)
			{
				SetScalarAndNormalizedConstants(newScalar, value, totalMax);
			}
			else
			{
				SetNormalizedConstant(minCurve, value);
			}
		}
	}

	public void SetScalarAndNormalizedConstants(float newScalar, float totalMin, float totalMax)
	{
		scalar.floatValue = newScalar;
		SetNormalizedConstant(minCurve, totalMin);
		SetNormalizedConstant(maxCurve, totalMax);
	}

	void SetNormalizedConstant(SerializedProperty curve, float totalValue)
	{
		float scalarValue = scalar.floatValue;

		scalarValue = Mathf.Max(scalarValue, 0.0001f);
		float relativeValue = totalValue / scalarValue;

		SetCurveConstant (curve, relativeValue);
	}

	// Callback for Curve Editor to get axis labels
	public Vector2 GetAxisScalars ()
	{
		return new Vector2 (m_Module.GetXAxisScalar(), scalar.floatValue * m_RemapValue);
	}

	// Callback for Curve Editor to set axis labels back
	public void SetAxisScalars (Vector2 axisScalars)
	{
		// X axis: TODO: We do not support changing the X values in the curve editor yet
		//m_Module.SetXAxisScalar (axisScalars.x);

		// Y axis:
		float remap = (m_RemapValue == 0.0f) ? 1f : m_RemapValue;
		scalar.floatValue = axisScalars.y / remap;
	}

	public void RemoveCurveFromEditor ()
	{
		ParticleSystemCurveEditor sce = m_Module.GetParticleSystemCurveEditor ();
		if (sce.IsAdded (GetMinCurve(), maxCurve))
			sce.RemoveCurve (GetMinCurve(), maxCurve);
	}


	public bool OnCurveAreaMouseDown (int button, Rect drawRect, Rect curveRanges)
	{
		if (button == 0)
		{
			ToggleCurveInEditor();
			return true;
		}
		
		if (button == 1)
		{
			AnimationCurveContextMenu.Show (drawRect, maxCurve, GetMinCurve(), scalar, curveRanges, m_Module.GetParticleSystemCurveEditor());
			return true;
		}

		return false;
	}

	public ParticleSystemCurveEditor.CurveData CreateCurveData (Color color)
	{
		NUnit.Framework.Assert.That(state != MinMaxCurveState.k_Scalar); // We should not create curve data for scalars

		return new ParticleSystemCurveEditor.CurveData(GetUniqueCurveName(), m_DisplayName, GetMinCurve(), maxCurve, color, m_SignedRange, GetAxisScalars, SetAxisScalars, m_Module.foldout);
	}

	SerializedProperty GetMinCurve ()
	{
		return state == MinMaxCurveState.k_TwoCurves ? minCurve : null;
	}

	public void ToggleCurveInEditor ()
	{
		ParticleSystemCurveEditor sce = m_Module.GetParticleSystemCurveEditor ();
		if (sce.IsAdded (GetMinCurve(), maxCurve))
			sce.RemoveCurve (GetMinCurve(), maxCurve);
		else
			sce.AddCurve (CreateCurveData (sce.GetAvailableColor ()));
	}

	private void SetMinMaxState (MinMaxCurveState newState)
	{
		if (newState == state)
			return;

		MinMaxCurveState oldState = state;
		ParticleSystemCurveEditor sce = m_Module.GetParticleSystemCurveEditor();

		if (sce.IsAdded (GetMinCurve(), maxCurve))
		{
			sce.RemoveCurve (GetMinCurve(), maxCurve);
		}

		switch (newState)
		{
			case MinMaxCurveState.k_Scalar:		InitSingleScalar (oldState); break;
			case MinMaxCurveState.k_TwoScalars: InitDoubleScalars (oldState); break;
			case MinMaxCurveState.k_Curve:		InitSingleCurve (oldState); break;
			case MinMaxCurveState.k_TwoCurves:	InitDoubleCurves (oldState); break;
		}

		// Assign state AFTER matching data to new state AND removing curve from curveEditor since it uses current 'state'
		minMaxState.intValue = (int)newState;

		// Add curve to CurveEditor if needed
		// Keep added to the editor if it was added before
		switch (newState)
		{
			case MinMaxCurveState.k_TwoCurves:
			case MinMaxCurveState.k_Curve:
				sce.AddCurve (CreateCurveData (sce.GetAvailableColor()));
				break;
			case MinMaxCurveState.k_Scalar:
			case MinMaxCurveState.k_TwoScalars:
				// Scalar do not add anything to the curve editor
				break;
			default:
				Debug.LogError("Unhandled enum value");
				break;
		}
		
		// Ensure we draw new icons for properties
		UnityEditorInternal.AnimationCurvePreviewCache.ClearCache();
	}

	void InitSingleScalar (MinMaxCurveState oldState)
	{
		switch (oldState)
		{
			case MinMaxCurveState.k_Curve:
			case MinMaxCurveState.k_TwoCurves:
			case MinMaxCurveState.k_TwoScalars:
				// Transfer curve sign back to scalar
				float maxCurveValue = GetMaxKeyValue(maxCurve.animationCurveValue.keys);
				scalar.floatValue *= maxCurveValue; 
			break;
		}
	
		// Ensure one key max curve (assumed by MinMaxCurve when evaluting curve.key[0].value * scalar)
		SetCurveConstant (maxCurve, 1f);
	}

	void InitDoubleScalars (MinMaxCurveState oldState)
	{
		// It is important that both minConstant and maxConstent is set to ensure one-key optimized curves!
		minConstant = GetAverageKeyValue(minCurve.animationCurveValue.keys) * scalar.floatValue;

		switch (oldState)
		{
			case MinMaxCurveState.k_Scalar:
				// Transfer scalar sign back to max curve
				maxConstant = scalar.floatValue;
				break;

			case MinMaxCurveState.k_Curve:
			case MinMaxCurveState.k_TwoCurves:
				maxConstant = GetAverageKeyValue(maxCurve.animationCurveValue.keys) * scalar.floatValue;
				break;
			default:
				Debug.LogError ("Enum not handled!");
				break;
		}
		
		// Double scalars is treated as double curves in the backend so ensure to call SetCurveRequirements
		SetCurveRequirements();
	}

	void InitSingleCurve(MinMaxCurveState oldState)
	{
		switch (oldState)
		{
			case MinMaxCurveState.k_Scalar:
				SetCurveConstant (maxCurve, GetNormalizedValueFromScalar());
				break;

			case MinMaxCurveState.k_TwoScalars:
			case MinMaxCurveState.k_TwoCurves:
				// Do nothing as maxCurve is the same as for two curves (notice that twoScalars is similar to twoCurves)
				break;
		}

		SetCurveRequirements();
	}

	void InitDoubleCurves(MinMaxCurveState oldState)
	{
		switch (oldState)
		{
			case MinMaxCurveState.k_Scalar:
				{
					// Transfer scalar sign to max curve
					SetCurveConstant(maxCurve, GetNormalizedValueFromScalar());
				}
				break;
			case MinMaxCurveState.k_TwoScalars:
				{
					// The TwoScalars mimic the TwoCurves so no need to change here
				}
				break;
			case MinMaxCurveState.k_Curve:
				{
					// Do nothing as maxCurve is the same as for two curves
				}
				break;
		}

		SetCurveRequirements ();
	}

	float GetNormalizedValueFromScalar()
	{
		if (scalar.floatValue < 0)
			return -1.0f;
		
		if (scalar.floatValue > 0)
			return 1.0f;

		return 0.0f;
	}

	void SetCurveRequirements()
	{
		// Abs negative values if we change to curve mode (in curve mode the sign is transfered to the curve)
		scalar.floatValue = Mathf.Abs(scalar.floatValue);

		// Ensure proper y-axis value (0 does not create a valid range)
		if (scalar.floatValue == 0)
			scalar.floatValue = m_DefaultCurveScalar;
	}

	void SetCurveConstant (SerializedProperty curve, float value)
	{
		Keyframe[] keys = new Keyframe[1];
		keys[0] = new Keyframe(0f, value);
		curve.animationCurveValue = new AnimationCurve (keys);
	}

	private float GetAverageKeyValue (Keyframe[] keyFrames)
	{
		float sum = 0;
		foreach (Keyframe key in keyFrames)
			sum += key.value;

		return sum / keyFrames.Length;
	}
	

	private float GetMaxKeyValue(Keyframe[] keyFrames)
	{
		float maxValue = -Mathf.Infinity;
		float minValue = Mathf.Infinity;
		foreach (Keyframe key in keyFrames)
		{
			if (key.value > maxValue)
				maxValue = key.value;
			if (key.value < minValue)
				minValue = key.value;
		}

		if (Mathf.Abs(minValue) > maxValue)
			return minValue;
		
		return maxValue;
	}

	private bool IsCurveConstant (Keyframe[] keyFrames, out float constantValue)
	{
		if (keyFrames.Length == 0)
		{
			constantValue = 0.0f;
			return false;
		}
		
		constantValue = keyFrames[0].value;
		for (int i=1; i<keyFrames.Length; ++i)
		{
			if (Mathf.Abs(constantValue - keyFrames[i].value) > 0.00001f)
				return false;
		}
		return true;
	}

	public string GetUniqueCurveName()
	{
		return SerializedModule.Concat(m_Module.GetUniqueModuleName(), m_Name);
	}

	public bool SupportsProcedural()
	{
		bool isValid = AnimationUtility.CurveSupportsProcedural(maxCurve.animationCurveValue);
		if ((state != MinMaxCurveState.k_TwoCurves) && (state != MinMaxCurveState.k_TwoScalars))
			return isValid;
		else
			return isValid && AnimationUtility.CurveSupportsProcedural(minCurve.animationCurveValue);
	}
}

} // namespace UnityEditor
