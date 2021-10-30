using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditorInternal
{
	internal class DopeLine
	{
		public static GUIStyle dopekeyStyle = "Dopesheetkeyframe";

		public Rect position;
		public AnimationWindowCurve[] m_Curves;
		public List<AnimationWindowKeyframe> keys;
		public int m_HierarchyNodeID;
		public System.Type objectType;
		public bool tallMode;
		public bool hasChildren;
		public bool isMasterDopeline;

		public System.Type valueType
		{
			get
			{
				if (m_Curves.Length > 0)
				{
					System.Type type = m_Curves[0].m_ValueType;
					for (int i = 1; i < m_Curves.Length; i++)
					{
						if (m_Curves[i].m_ValueType != type)
							return null;
					}
					return type;
				} 

				return null;
			}
		}
		
		public bool isPptrDopeline
		{
			get
			{
				if (m_Curves.Length > 0)
				{
					for (int i = 0; i < m_Curves.Length; i++)
					{
						if (!m_Curves[i].isPPtrCurve)
							return false;
					}
					return true;
				}
				return false;
			}
		}

		public void LoadKeyframes ()
		{
			keys = new List<AnimationWindowKeyframe> ();
			foreach (AnimationWindowCurve curve in m_Curves)
				foreach (AnimationWindowKeyframe key in curve.m_Keyframes)
					keys.Add (key);
			
			keys.Sort ((a, b) => a.time.CompareTo (b.time));
		}
			

		public DopeLine (int hierarchyNodeId, AnimationWindowCurve[] curves)
		{
			m_HierarchyNodeID = hierarchyNodeId;

			m_Curves = curves;

			LoadKeyframes ();
		}
	}
}
