using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor 
{
/*
internal sealed class GradientPreviewCache
{
		private class CacheEntry
	{
		public Gradient m_Gradient;
		public Texture2D m_Preview;
	}
	static private List<CacheEntry> m_Previews;

	private static void Init ()
	{
		if (m_Previews == null)
			m_Previews = new List<CacheEntry> ();
	}


	private static CacheEntry FindPreview (Gradient gradient)
	{
		Init ();

		for (int i=0; i<m_Previews.Count; ++i)
		{
			if (gradient.IsSameAs (m_Previews[i].m_Gradient))
				return m_Previews[i];
		}
		return null;
	}


	public static Texture2D GetPreview (Gradient gradient)
	{
		CacheEntry entry = FindPreview (gradient);
		if (entry != null)
		{
			GradientEditor.RefreshPreview (gradient, entry.m_Preview);
			return entry.m_Preview;
		}

		// Create prevew
		Texture2D preview = GradientEditor.CreateGradientTexture (gradient);

		// Add to cache
		CacheEntry newEntry = new CacheEntry {m_Preview = preview, m_Gradient = gradient};
		m_Previews.Add (newEntry);

		return preview;

	}
	
	public static Texture2D GetPreview (SerializedProperty property)
	{
		return GetPreview (property.gradientValue);
	}
	
	public static void ClearCache ()
	{
		if (m_Previews != null)
		{
			for (int i = 0; i < m_Previews.Count; ++i)
				Object.DestroyImmediate(m_Previews[i].m_Preview);
			m_Previews.Clear();
		}
	}


}
    */
} // namespace
