using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using System.Linq;
using NUnit.Framework;

using Object = UnityEngine.Object;

namespace UnityEditor
{
	public enum BodyPart
	{
		None = -1,
		Avatar = 0,
		Body,
		Head,
		LeftArm,
		LeftFingers,
		RightArm,
		RightFingers,
		LeftLeg,
		RightLeg,
		Last
	}

	internal class AvatarControl
	{
		class Styles
		{
			// IF you change the order of this array, please update:
			// BodyPartMapping
			// m_BodyPartHumanBone
			// BodyPart

			public GUIContent[] Silhouettes =
			{
				EditorGUIUtility.IconContent ("AvatarInspector/BodySilhouette"),
				EditorGUIUtility.IconContent ("AvatarInspector/HeadZoomSilhouette"),
				EditorGUIUtility.IconContent ("AvatarInspector/LeftHandZoomSilhouette"),
				EditorGUIUtility.IconContent ("AvatarInspector/RightHandZoomSilhouette")
			};

			public GUIContent[,] BodyPart =
			{
				{
					null,
					EditorGUIUtility.IconContent ("AvatarInspector/Torso"),
					EditorGUIUtility.IconContent ("AvatarInspector/Head"),
					EditorGUIUtility.IconContent ("AvatarInspector/LeftArm"),
					EditorGUIUtility.IconContent ("AvatarInspector/LeftFingers"),
					EditorGUIUtility.IconContent ("AvatarInspector/RightArm"),
					EditorGUIUtility.IconContent ("AvatarInspector/RightFingers"),
					EditorGUIUtility.IconContent ("AvatarInspector/LeftLeg"),
					EditorGUIUtility.IconContent ("AvatarInspector/RightLeg")
				},
				{
					null,
					null,
					EditorGUIUtility.IconContent ("AvatarInspector/HeadZoom"),
					null,
					null,
					null,
					null,
					null,
					null
				},
				{
					null,
					null,
					null,
					null,
					EditorGUIUtility.IconContent ("AvatarInspector/LeftHandZoom"),
					null,
					null,
					null,
					null
				},
				{
					null,
					null,
					null,
					null,
					null,
					null,
					EditorGUIUtility.IconContent ("AvatarInspector/RightHandZoom"),
					null,
					null
				},
			};

			/*public GUIContent[] BodyPartIK =
			{
				null,
				null,
				null,
				null,
				EditorGUIUtility.IconContent ("AvatarInspector/LeftFingersIk"),
				null,
				EditorGUIUtility.IconContent ("AvatarInspector/RightFingersIk"),
				null,
				null
			};*/

			public GUILayoutOption ButtonSize = GUILayout.MaxWidth (120);
		}

		static Styles styles { get { if (s_Styles == null) s_Styles = new Styles (); return s_Styles; } }
		static Styles s_Styles;

		public enum BodyPartColor
		{
			Off = 0x00,
			Green = 0x01 << 0,
			Red = 0x01 << 1,
			IKGreen = 0x01 << 2,
			IKRed = 0x01 << 3,
		}

		public delegate BodyPartColor BodyPartFeedback (BodyPart bodyPart);

		static public int ShowBoneMapping(int shownBodyView, BodyPartFeedback bodyPartCallback, AvatarSetupTool.BoneWrapper[] bones, SerializedObject serializedObject)
		{
			GUILayout.BeginHorizontal ();
			{
				GUILayout.FlexibleSpace ();
				
				if (styles.Silhouettes[shownBodyView].image)
				{
					Rect rect = GUILayoutUtility.GetRect (styles.Silhouettes[shownBodyView], GUIStyle.none, GUILayout.MaxWidth (styles.Silhouettes[shownBodyView].image.width));
					DrawBodyParts (rect, shownBodyView, bodyPartCallback);
					
					for (int i=0; i<bones.Length; i++)
						DrawBone(shownBodyView, i, rect, bones[i], serializedObject);
				}
				else
					GUILayout.Label ("texture missing,\nfix me!");
				
				GUILayout.FlexibleSpace ();
			}
			GUILayout.EndHorizontal ();
			
			// Body view buttons
			Rect buttonsRect = GUILayoutUtility.GetLastRect ();
			const float buttonHeight = 16;
			string[] labels = new string[] { "Body", "Head", "Left Hand", "Right Hand"};
			buttonsRect.x += 5;
			buttonsRect.width = 70;
			buttonsRect.yMin = buttonsRect.yMax - (buttonHeight * 4 + 5);
			buttonsRect.height = buttonHeight;
			for (int i=0; i<labels.Length; i++)
			{
				if (GUI.Toggle (buttonsRect, shownBodyView == i, labels[i], EditorStyles.miniButton))
					shownBodyView = i;
				buttonsRect.y += buttonHeight;
			}
			
			return shownBodyView;
		}
		
		static public void DrawBodyParts (Rect rect, int shownBodyView, BodyPartFeedback bodyPartCallback)
		{
			GUI.color = new Color (0.2f, 0.2f, 0.2f, 1.0f);
			if (styles.Silhouettes[shownBodyView] != null)
				GUI.DrawTexture (rect, styles.Silhouettes[shownBodyView].image);
			for (int i=1; i<(int)BodyPart.Last; i++)
				DrawBodyPart (shownBodyView, i, rect, bodyPartCallback ((BodyPart)i));
		}
		
		static protected void DrawBodyPart (int shownBodyView, int i, Rect rect, BodyPartColor bodyPartColor)
		{
			if (styles.BodyPart[shownBodyView, i] != null && styles.BodyPart[shownBodyView, i].image != null)
			{
				if ((bodyPartColor & BodyPartColor.Green) == BodyPartColor.Green)
					GUI.color = Color.green;
				else if ((bodyPartColor & BodyPartColor.Red) == BodyPartColor.Red)
					GUI.color = Color.red;
				else
					GUI.color = Color.gray;
				GUI.DrawTexture (rect, styles.BodyPart[shownBodyView, i].image);
				GUI.color = Color.white;
			}
			
			/*if (shownBodyView == 0 && styles.BodyPartIK[i] != null && styles.BodyPartIK[i].image != null)
			{
				if ((bodyPartColor & BodyPartColor.IKRed) == BodyPartColor.IKRed)
					GUI.color = Color.red;
				else if ((bodyPartColor & BodyPartColor.IKGreen) == BodyPartColor.IKGreen)
					GUI.color = Color.green;
				else
					GUI.color = Color.gray;
				GUI.DrawTexture (rect, styles.BodyPartIK[i].image);
				GUI.color = Color.white;
			}*/
		}
		
		static Vector2[,] s_BonePositions = new Vector2[4, HumanTrait.BoneCount];
		
		public static List<int> GetViewsThatContainBone (int bone)
		{
			List<int> views = new List<int> ();
			
			if (bone < 0 || bone >= HumanTrait.BoneCount)
				return views;
			
			for (int i=0; i<4; i++)
			{
				if (s_BonePositions[i, bone] != Vector2.zero)
					views.Add (i);
			}
			return views;
		}
		
		static AvatarControl ()
		{
			// Body view
			int view = 0;
			// hips
			s_BonePositions[view,  0] = new Vector2 ( 0.00f, 0.08f);
			
			// upper leg
			s_BonePositions[view,  1] = new Vector2 ( 0.16f, 0.01f);
			s_BonePositions[view,  2] = new Vector2 (-0.16f, 0.01f);
			
			// lower leg
			s_BonePositions[view,  3] = new Vector2 ( 0.21f,-0.40f);
			s_BonePositions[view,  4] = new Vector2 (-0.21f,-0.40f);
			
			// foot
			s_BonePositions[view,  5] = new Vector2 ( 0.23f,-0.80f);
			s_BonePositions[view,  6] = new Vector2 (-0.23f,-0.80f);
			
			// spine - head
			s_BonePositions[view,  7] = new Vector2 ( 0.00f, 0.25f);
			s_BonePositions[view,  8] = new Vector2 ( 0.00f, 0.43f);
			s_BonePositions[view,  9] = new Vector2 ( 0.00f, 0.66f);
			s_BonePositions[view, 10] = new Vector2 ( 0.00f, 0.76f);
			
			// shoulder
			s_BonePositions[view, 11] = new Vector2 ( 0.14f, 0.60f);
			s_BonePositions[view, 12] = new Vector2 (-0.14f, 0.60f);
			
			// upper arm
			s_BonePositions[view, 13] = new Vector2 ( 0.30f, 0.57f);
			s_BonePositions[view, 14] = new Vector2 (-0.30f, 0.57f);
			
			// lower arm
			s_BonePositions[view, 15] = new Vector2 ( 0.48f, 0.30f);
			s_BonePositions[view, 16] = new Vector2 (-0.48f, 0.30f);
			
			// hand
			s_BonePositions[view, 17] = new Vector2 ( 0.66f, 0.03f);
			s_BonePositions[view, 18] = new Vector2 (-0.66f, 0.03f);
			
			// toe
			s_BonePositions[view, 19] = new Vector2 ( 0.25f,-0.89f);
			s_BonePositions[view, 20] = new Vector2 (-0.25f,-0.89f);
			
			// Head view
			view = 1;
			// neck - head
			s_BonePositions[view,  9] = new Vector2 (-0.20f,-0.62f);
			s_BonePositions[view, 10] = new Vector2 (-0.15f,-0.30f);
			// left, right eye
			s_BonePositions[view, 21] = new Vector2 ( 0.63f, 0.16f);
			s_BonePositions[view, 22] = new Vector2 ( 0.15f, 0.16f);
			// jaw
			s_BonePositions[view, 23] = new Vector2 ( 0.45f,-0.40f);
			
			// Left hand view
			view = 2;
			// finger bases, thumb - little
			s_BonePositions[view, 24] = new Vector2 (-0.35f, 0.11f);
			s_BonePositions[view, 27] = new Vector2 ( 0.19f, 0.11f);
			s_BonePositions[view, 30] = new Vector2 ( 0.22f, 0.00f);
			s_BonePositions[view, 33] = new Vector2 ( 0.16f,-0.12f);
			s_BonePositions[view, 36] = new Vector2 ( 0.09f,-0.23f);
			
			// finger tips, thumb - little
			s_BonePositions[view, 26] = new Vector2 (-0.03f, 0.33f);
			s_BonePositions[view, 29] = new Vector2 ( 0.65f, 0.16f);
			s_BonePositions[view, 32] = new Vector2 ( 0.74f, 0.00f);
			s_BonePositions[view, 35] = new Vector2 ( 0.66f,-0.14f);
			s_BonePositions[view, 38] = new Vector2 ( 0.45f,-0.25f);
			
			// finger middles, thumb - little
			for (int i=0; i<5; i++)
				s_BonePositions[view, 25+i*3] = Vector2.Lerp (s_BonePositions[view, 24+i*3], s_BonePositions[view, 26+i*3], 0.58f);
			
			// Right hand view
			view = 3;
			for (int i=0; i<15; i++)
				s_BonePositions[view, 24+i+15] = Vector2.Scale (s_BonePositions[view-1, 24+i], new Vector2 (-1, 1));
		}

		static protected void DrawBone(int shownBodyView, int i, Rect rect, AvatarSetupTool.BoneWrapper bone, SerializedObject serializedObject)
		{
			if (s_BonePositions[shownBodyView, i] == Vector2.zero)
				return;
			
			Vector2 pos = s_BonePositions[shownBodyView, i];
			pos.y *= -1; // because higher values should be up
			pos.Scale (new Vector2 (rect.width * 0.5f, rect.height * 0.5f));
			pos = rect.center + pos;
			int kIconSize = AvatarSetupTool.BoneWrapper.kIconSize;
			Rect r = new Rect (pos.x - kIconSize * 0.5f, pos.y - kIconSize * 0.5f, kIconSize, kIconSize);
			bone.BoneDotGUI(r, i, true, true, serializedObject);
		}
	}
}

