using UnityEngine;
using System.Collections;

namespace UnityEngine.Graphs.LogicGraph
{
	public class MaterialNodes
	{
		#if REIMPLEMENT_USING_CLASS_NODES
		[Logic(typeof(Renderer), typeof(NodeLibrary.StartStopEvents))]
		public static IEnumerator UVScroll (Renderer self, ByRef<NodeLibrary.StartStopEvents> evt, string property, Vector2 magnitude, float frequency, int mode)
		{
			Material mat = self.material;
			if (property == null || property == "")
				property = "_MainTex";
			
			if (mode == 0)
			{
				while (true)
				{
					// scroll uv
					Vector2 uv = mat.GetTextureOffset(property);
					uv += magnitude * Time.deltaTime;
					mat.SetTextureOffset(property, uv);
					
					// Handle Stop event
					if (evt.Value == NodeLibrary.StartStopEvents.Stop)
						break;
					yield return 0;
				}
			}
			else
			{
				// Elapsed time, measured in cycles.
				float elapsed = 0;
				
				Vector2 lastUV = mat.GetTextureOffset(property);
				
				float stopElapsed = 0f;
				bool exit = false;
				
				while (true)
				{
					// Handle Stop event
					if (evt.Value == NodeLibrary.StartStopEvents.Stop)
					{
						// When stopping, complete the current cycle before really stopping
						// Update: Actually we only need to complete the current half-cycle
						// because the cycle always has the same value in the middle as in the beginning and end.
						if (stopElapsed == 0)
							stopElapsed = Mathf.Ceil(elapsed * 2) * 0.5f;
						
						// When we reach the end of the cycle, stop at the exact time
						else if (elapsed >= stopElapsed)
						{
							elapsed = stopElapsed;
							exit = true;
						}
					}
					
					Vector2 uv = Vector2.zero;
					// Triangle wave (centered around 0)
					if (mode == 1)
						uv += magnitude * (Mathf.PingPong(elapsed * 2f + 0.5f, 1) - 0.5f);
					// Sine wave (centered around 0)
					if (mode == 2)
						uv += magnitude * 0.5f * Mathf.Sin(elapsed * 2f * Mathf.PI);
					
					mat.SetTextureOffset(property, mat.GetTextureOffset(property) + (uv - lastUV));
					lastUV = uv;
					
					if (exit)
						break;
					
					elapsed += Time.deltaTime * frequency;
					
					yield return 0;
				}
			}
			
		}
		
		[Logic(typeof(Renderer), typeof(NodeLibrary.StartStopEvents))]
		public static IEnumerator UVCycler (Renderer self, ByRef<NodeLibrary.StartStopEvents> evt, string property, int xTiles, int yTiles, float speed)
		{
			Material mat = self.material;
			if (property == null || property == "")
				property = "_MainTex";
			
			// TODO: find out what initial frame is based on uv offset in the beginning?
			
			float elapsed = 0;
			while (true)
			{
				int frame = Mathf.FloorToInt(elapsed);
				
				float xOffset = frame % xTiles;
				float yOffset = yTiles - 1 - (frame / xTiles) % yTiles;
				
				Vector2 uv = new Vector2(xOffset / xTiles, yOffset / yTiles);
				mat.SetTextureOffset(property, uv);
				
				// Handle Stop event
				if (evt.Value == NodeLibrary.StartStopEvents.Stop)
					break;
				
				elapsed += Time.deltaTime * speed;
				
				yield return 0;
			}
		}
		#endif
	}
}
