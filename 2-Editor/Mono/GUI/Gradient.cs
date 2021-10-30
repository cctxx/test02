using UnityEngine;

// TODO:
// Create new GUI.DrawTexture function that takes tex coords as input (for tiled checker grid background)
// GradientEditor: Color swatch and alpha slider below gradient
// Move Gradient logic to C++
// -------------------------------------------------------------------
// Move Gradient to own txt file
// GradientEditor: Remove on lost focus
// SerializedProperty for Gradient
// Inject Gradient instead of ColorCurve
// Try ColorKey inside Gradient c#


/*
[System.Serializable]
public class Gradient 
{
	public float[] alphaTimes = { 0, 1 };
	public float[] alphaValues = { 1, 1 };
	
	public float[] colorTimes = { 0, 1 };
	public Color[] colorValues = { Color.black, Color.white }; // Only RGB is actually used, but this simplifies the code somewhat

	public Color CalcColor (float time)
	{
		Color retVal = CalcSwatch (colorTimes, colorValues, time, (a,b,t) => Color.Lerp (a,b,t));
		retVal.a = CalcSwatch(alphaTimes, alphaValues, time, (a, b, t) => Mathf.Lerp(a, b, t));
		return retVal;
	}
	
	delegate T Lerp<T> (T a, T b, float time);
	
	T CalcSwatch<T> (float[] times, T[] values, float time, Lerp<T> lerp)
	{
		if (time == 0f)
			return values[0];
		if (time == 1f)
			return values[values.Length-1];

		float lastTime = 0;
		T lastValue = values[0];
		for (int i = 0; i < times.Length; i++)
		{
			if (times[i] >= time)	
			{
				float frac = (time - lastTime) / (times[i] - lastTime);
				return lerp (lastValue, values[i], frac);
			}
			lastTime = times[i];
			lastValue = values[i];
		}
		return lastValue;
	}

	public bool IsSameAs (Gradient other)
	{
		if (other == null)
			return false;

		if (ReferenceEquals(this, other))
			return true;

		if (alphaTimes.Length == other.alphaTimes.Length && colorTimes.Length == other.colorTimes.Length)
		{
			for (int i = 0; i < alphaTimes.Length; ++i)
			{
				if (alphaTimes[i] != other.alphaTimes[i])
					return false;

				if (alphaValues[i] != other.alphaValues[i])
					return false;
			}

			for (int i = 0; i < colorTimes.Length; ++i)
			{
				if (colorTimes[i] != other.colorTimes[i])
					return false;

				if (colorValues[i] != other.colorValues[i])
					return false;
			}

			return true;
		}
		return false;
	}
}
*/
