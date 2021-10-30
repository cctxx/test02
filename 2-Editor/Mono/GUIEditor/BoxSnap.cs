using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor 
{

	internal class SnapEdge 
	{
		public enum EdgeDir {Left, Right, CenterX, Up, Down, CenterY, None }
		const float kSnapDist = 0;
		public Object m_Object;
		public float pos, start, end;
		public float startDragPos, startDragStart;
		public EdgeDir dir;
		public override string ToString () 
		{
			if (m_Object != null) {
				return "Edge: " + dir + " of " + m_Object.name + "    pos: " + pos + " (" + start +" - "+end +")";
			} else 
				return "Edge: " + dir + " of NULL - something is wrong!";
		}
		
		internal SnapEdge (Object win, EdgeDir _d, float _p, float _s, float _e) 
		{
			dir = _d;
			m_Object = win;
			pos = _p;
			start = _s;
			end = _e;
		}
		
		internal static EdgeDir OppositeEdge (EdgeDir dir) 
		{
			switch (dir) {
			case EdgeDir.Left:    return EdgeDir.Right;
			case EdgeDir.Right:   return EdgeDir.Left;
			case EdgeDir.Down:    return EdgeDir.Up;
			case EdgeDir.Up:      return EdgeDir.Down;
			case EdgeDir.CenterX: return EdgeDir.CenterX;
			case EdgeDir.CenterY: 
			default:
				return EdgeDir.CenterY;
			}
		}

		private int EdgeCoordinateIndex ()
		{
			if (dir == EdgeDir.Left || dir == EdgeDir.Right || dir == EdgeDir.CenterX)
				return 0;
			return 1;
		}
		// activeEdges, 2 Lists [0]is horizontal, [1] is vertical
		// each of them will contain a list of all active edge snap pairs.
		internal static Vector2 Snap (List<SnapEdge> sourceEdges, List<SnapEdge> edgesToSnapAgainst, List<KeyValuePair<SnapEdge, SnapEdge>>[] activeEdges)
		{
			Vector2 delta = Vector2.zero;
			float kSnapDist = 10;

			activeEdges[0].Clear();
			activeEdges[1].Clear();

			// Cull all edges we have no overlaps with
			//edgesToSnapAgainst.RemoveAll (i => !EdgeInside (i, sourceEdges));

			// compare all edges against existing stuff.
			// Best match in all directions
			float []dirSnapDist = { kSnapDist, kSnapDist };
			// How far would it like us to snap?
			float []dirSnapValue = {0,0};
			
			foreach (SnapEdge e in sourceEdges) {
				int idx = e.EdgeCoordinateIndex();
				Snap (e, edgesToSnapAgainst, ref dirSnapDist[idx], ref dirSnapValue[idx], activeEdges[idx]);
			}
			
			delta.x = dirSnapValue[0];
			delta.y = dirSnapValue[1];

			return delta;
		}

		static bool EdgeInside (SnapEdge edge, List<SnapEdge> frustum)
		{
			foreach (var e in frustum)
			{
				if (!ShouldEdgesSnap (edge, e))
					return false;
			}
			return true;
		}

		// Could edge A snap to edge B? This is not a full test, only used for culling
		static bool ShouldEdgesSnap (SnapEdge a, SnapEdge b)
		{
			if ((a.dir == EdgeDir.CenterX || a.dir == EdgeDir.CenterY) && a.dir == b.dir)
				return true;
			return a.dir == OppositeEdge (b.dir) && !(a.start > b.end || a.end < b.start);
		}

		internal static void Snap (SnapEdge edge, List<SnapEdge> otherEdges, ref float maxDist, ref float snapVal, List<KeyValuePair<SnapEdge, SnapEdge>> activeEdges) 
		{
			foreach (SnapEdge e in otherEdges)
			{
				if (ShouldEdgesSnap (edge, e))
				{
					float dist = Mathf.Abs (e.pos - edge.pos);
					if (dist < maxDist) 
					{
						maxDist = dist;
						snapVal = e.pos - edge.pos;
						activeEdges.Clear ();
						activeEdges.Add (new KeyValuePair<SnapEdge, SnapEdge> (edge, e));
					} 
					else if (dist == maxDist)
						activeEdges.Add (new KeyValuePair<SnapEdge, SnapEdge> (edge, e));
				}
			}
		}
		
		internal void ApplyOffset (Vector2 offset, ref int changedEdges, bool windowMove) 
		{
			offset = (dir == EdgeDir.Left || dir == EdgeDir.Right) ? offset : new Vector2 (offset.y, offset.x);
			if (windowMove) 
			{
				pos += offset.x;
				if (offset.x != 0.0f)
					changedEdges |= (1<<(int)dir);
			} 
			else 
			{
				float oldpos = pos;
				pos = offset.x + startDragPos;
				if (pos != oldpos)
					changedEdges |= (1<<(int)dir);
			}
			start += offset.y;
			end += offset.y;
		}
	}
}
