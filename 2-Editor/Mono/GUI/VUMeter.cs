using UnityEngine;

namespace UnityEditor
{
	public sealed partial class EditorGUI
	{
		internal struct VUMeterData 
		{
			public float lastValue;
			public float peakValue;
			public Texture2D texture;
			public float peakValueTime;
			public bool IsDone() { return (lastValue == 0 && peakValue == 0); }
		}
	
		private static Texture2D s_VerticalVUTexture, s_HorizontalVUTexture;
		private const float VU_SPLIT = 0.9f;
		 
		internal static void VUMeterHorizontal(Rect position, float value, ref VUMeterData data)
		{
			if (Event.current.type != EventType.Repaint)
				return;
			
			if (!s_HorizontalVUTexture)
				s_HorizontalVUTexture = EditorGUIUtility.LoadIcon ("VUMeterTextureHorizontal");

			Color temp = GUI.color, temp2 = GUI.contentColor, temp3 = GUI.backgroundColor;
			
			if (value < data.lastValue)
			{
				value = Mathf.Lerp(data.lastValue, value, Time.smoothDeltaTime * 7.0f);
			}
			else
			{
				value = Mathf.Lerp(value, data.lastValue, Time.smoothDeltaTime * 2.0f);
				data.peakValue = value;	
				data.peakValueTime = Time.realtimeSinceStartup  ;
			}
				
			if (value > 1.0f / VU_SPLIT) value = 1.0f / VU_SPLIT;
			if (data.peakValue > 1.0f / VU_SPLIT) data.peakValue = 1.0f / VU_SPLIT;
																		
			// Draw background
			GUI.contentColor = new Color (0,0,0,0);
			EditorStyles.progressBarBack.Draw (position,false,false,false,false);
			
			// Draw foreground
			float width = position.width * (value * VU_SPLIT) - 2;
			if (width < 2) width = 2;
			Rect newRect = new Rect(position.x + 1, position.y + 1, width, position.height - 2);
			Rect uvRect = new Rect(0,0,value * VU_SPLIT,1);	
			GUI.DrawTextureWithTexCoords(newRect, s_HorizontalVUTexture, uvRect);
		
			// Draw peak indicator
			GUI.color =  Color.white;
			width = position.width * (data.peakValue * VU_SPLIT) - 2;
			if (width < 2) width = 2;
			newRect = new Rect(position.x + width, position.y + 1, 1, position.height - 2);
			if (Time.realtimeSinceStartup  - data.peakValueTime < 1.0f)
				GUI.DrawTexture(newRect, EditorGUIUtility.whiteTexture, ScaleMode.StretchToFill);
			
			
			// reset color
			GUI.backgroundColor = temp3;
			GUI.color = temp;
			GUI.contentColor = temp2;
			
			data.lastValue = value;
		}
		
		internal static void VUMeterVertical (Rect position, float value, ref VUMeterData data) 
		{
			if (Event.current.type != EventType.Repaint)
				return;
			if (!s_VerticalVUTexture)
				s_VerticalVUTexture = EditorGUIUtility.LoadIcon ("VUMeterTextureVertical");

			Color temp = GUI.color, temp2 = GUI.contentColor, temp3 = GUI.backgroundColor;
			
			if (value < data.lastValue)
			{
				value = Mathf.Lerp(data.lastValue, value, Time.smoothDeltaTime * 7.0f);
			}
			else
			{
				value = Mathf.Lerp(value, data.lastValue, Time.smoothDeltaTime * 2.0f);
				data.peakValue = value;	
				data.peakValueTime = Time.realtimeSinceStartup  ;
			}
			
			if (value > 1.0f / VU_SPLIT) value = 1.0f / VU_SPLIT;
			if (data.peakValue > 1.0f / VU_SPLIT) data.peakValue = 1.0f / VU_SPLIT;
							
			// Draw background
			GUI.contentColor = new Color (0,0,0,0);
			EditorStyles.progressBarBack.Draw (position,false,false,false,false);
			
			// Draw foreground
			float height = position.height * (value * VU_SPLIT) - 2;
			if (height < 2) height = 2;
			Rect newRect = new Rect(position.x + 1, (position.y + position.height) - height, position.width - 2, height);
			Rect uvRect = new Rect(0,0,1,value * VU_SPLIT);	
			GUI.DrawTextureWithTexCoords(newRect, s_VerticalVUTexture, uvRect);
			
			// Draw peak indicator
			GUI.color =  Color.white;
			height = position.height * (data.peakValue * VU_SPLIT) - 2;
			if (height < 2) height = 2;
			newRect = new Rect(position.x + 1, position.y + position.height - height, position.width - 2, 1);
			if (Time.realtimeSinceStartup - data.peakValueTime < 1.0f)
				GUI.DrawTexture(newRect,EditorGUIUtility.whiteTexture, ScaleMode.StretchToFill);
			
			// reset color
			GUI.backgroundColor = temp3;
			GUI.color = temp;
			GUI.contentColor = temp2;
			
			data.lastValue = value;
		}
	}
}

