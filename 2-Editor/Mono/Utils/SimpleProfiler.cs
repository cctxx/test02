//#define SIMPLE_PROFILER
using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditor
{
	// Simple profiler that can measure time spend inside code blocks.
	// If the same code block is hit multiple times, the times are summed up.
	// Measured blocks may be nested, although the profiler does not do anything intelligent with it.
	// Ie. you can measure a big code block as well as smaller code blocks within it.
	// All measured times are printed when calling PrintTimes, at which point the timer is also reset.
	
	// Note, out built-in profiler in Unity did not catch the code I needed to profile,
	// hence I had to make this one.
	internal class SimpleProfiler
	{
		// Lazy coding with parallel stacks
		private static Stack<string> m_Names = new Stack<string> ();
		private static Stack<float> m_StartTime = new Stack<float> ();
		private static Dictionary<string, float> m_Timers = new Dictionary<string, float> ();
		
		[System.Diagnostics.Conditional ("SIMPLE_PROFILER")]
		public static void Begin (string label)
		{
			m_Names.Push (label);
			m_StartTime.Push (Time.realtimeSinceStartup);
		}
		
		[System.Diagnostics.Conditional ("SIMPLE_PROFILER")]
		public static void End ()
		{
			string str = m_Names.Pop ();
			float duration = (Time.realtimeSinceStartup - m_StartTime.Pop ());
			if (m_Timers.ContainsKey (str))
				m_Timers[str] += duration;
			else
				m_Timers[str] = duration;
		}
		
		[System.Diagnostics.Conditional ("SIMPLE_PROFILER")]
		public static void PrintTimes ()
		{
			string str = "Measured execution times:\n----------------------------\n";
			foreach (KeyValuePair<string, float> kvp in m_Timers)
				str += string.Format ("{0,6:0.0} ms: {1}\n", kvp.Value*1000, kvp.Key);
			Debug.Log (str);
			m_Names.Clear ();
			m_StartTime.Clear ();
			m_Timers.Clear ();
		}
	}
}

