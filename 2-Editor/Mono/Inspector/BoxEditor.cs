using UnityEditorInternal;
using UnityEngine;


namespace UnityEditor
{
	internal class BoxEditor
	{
		private int m_ControlIdHint;
		private const float kViewAngleThreshold = 3.0f * Mathf.Deg2Rad;
		private int m_HandleControlID;
		private bool m_UseLossyScale = false;
		private bool m_AlwaysDisplayHandles = false;
		private bool m_DisableZaxis;

		public BoxEditor (bool useLossyScale, int controlIdHint)
		{
			m_UseLossyScale = useLossyScale;
			m_ControlIdHint = controlIdHint;
		}

		public BoxEditor (bool useLossyScale, int controlIdHint, bool disableZaxis)
		{
			m_UseLossyScale = useLossyScale;
			m_ControlIdHint = controlIdHint;
			m_DisableZaxis = disableZaxis;
		}
		
		public void OnEnable()
		{
			m_HandleControlID = -1;
		}

		public void OnDisable ()
		{
		}

		public void SetAlwaysDisplayHandles (bool enable)
		{
			m_AlwaysDisplayHandles = enable;
		}

		public bool OnSceneGUI(Transform transform, Color color, ref Vector3 center, ref Vector3 size)
		{
			return OnSceneGUI (transform, color, true, ref center, ref size);
		}

		public bool OnSceneGUI(Transform transform, Color color, bool handlesOnly, ref Vector3 center, ref Vector3 size)
		{
			if (m_UseLossyScale)
			{
				Matrix4x4 noScaleMatrix = Matrix4x4.TRS (transform.position, transform.rotation, Vector3.one);

				// Transform size & center in global space using lossy scale (mirroring DrawPrimitiveColliderGizmo for BoxCollider)
				size.Scale (transform.lossyScale);
				center = transform.TransformPoint (center);
				center = noScaleMatrix.inverse.MultiplyPoint (center);

				bool changed = OnSceneGUI (noScaleMatrix, color, handlesOnly, ref center, ref size);

				// Transform size & center back into local space
				center = noScaleMatrix.MultiplyPoint (center);
				center = transform.InverseTransformPoint (center);
				size.Scale (new Vector3 (1f / transform.lossyScale.x, 1f / transform.lossyScale.y, 1f / transform.lossyScale.z));

				return changed;
			}

			return OnSceneGUI (transform.localToWorldMatrix, color, handlesOnly, ref center, ref size);
		}

		// Returns true if box editor was changed
		public bool OnSceneGUI(Matrix4x4 transform, Color color, bool handlesOnly, ref Vector3 center, ref Vector3 size)
		{
			bool dragging = GUIUtility.hotControl == m_HandleControlID;
			bool turnedOnByShift = Event.current.shift;
			bool needsOnSceneGUI = m_AlwaysDisplayHandles || turnedOnByShift || dragging;

			if (!needsOnSceneGUI)
			{
				// Early out but make sure controlIDs match
				for (int i = 0; i < 6; i++)
					GUIUtility.GetControlID(m_ControlIdHint, FocusType.Keyboard);	
				return false;
			}

			// Use our own color for handles
			Color orgColor = Handles.color;
			Handles.color = color;

			// Collider box data (local space)
			Vector3 minPos = center - size * 0.5f;
			Vector3 maxPos = center + size * 0.5f;
			//Vector3 orgMinPos = minPos; // outcommented for 3.4 where we use shift for showing the handles instead of mirroring
			//Vector3 orgMaxPos = maxPos;

			// Set matrix so we can work in localspace here, but intersect and render in worldspace in slider2D
			// We create our own localToWorld matrix to ensure that we use lossyScale (as on c++ side) 
			Matrix4x4 orgMatrix = Handles.matrix;
			Handles.matrix = transform;

			int prevHotControl = GUIUtility.hotControl;

			// Wireframe box (before handles so handles are rendered top most)
			if (!handlesOnly)
				DrawWireframeBox((maxPos - minPos)*0.5f + minPos, (maxPos - minPos));

			// Handles
			MidpointHandles(ref minPos, ref maxPos, Handles.matrix);
			//EdgeHandles(ref minPos, ref maxPos, Handles.matrix);

			// Detect if any of our handles got hotcontrol
			if (prevHotControl != GUIUtility.hotControl && GUIUtility.hotControl != 0)
				m_HandleControlID = GUIUtility.hotControl;

			bool changed = GUI.changed;

			// Update if changed
			if (changed)
			{
				// outcommented for 3.4 where we use shift for showing the handles instead of mirroring
				/*if (Event.current.shift)
				{
					// Mirror change
					Vector3 minDelta = minPos - orgMinPos;
					Vector3 maxDelta = maxPos - orgMaxPos;
					Vector3 newMinPos = orgMinPos + minDelta - maxDelta;
					Vector3 newMaxPos = orgMaxPos + maxDelta - minDelta;
					m_Center.vector3Value = (newMaxPos + newMinPos) * 0.5f;
					m_Size.vector3Value = (newMaxPos - newMinPos);					
				}
				else*/
				{
					center = (maxPos + minPos) * 0.5f;
					size = (maxPos - minPos);	
				}
			}


			// Reset states
			Handles.color = orgColor;
			Handles.matrix = orgMatrix;
			return changed;
		}

		public void DrawWireframeBox (Vector3 center, Vector3 siz)
		{
			Vector3 halfsize = siz * 0.5f;

			Vector3[] points = new Vector3[10];
			points[0] = center + new Vector3(-halfsize.x, -halfsize.y, -halfsize.z);
			points[1] = center + new Vector3(-halfsize.x,  halfsize.y, -halfsize.z);
			points[2] = center + new Vector3( halfsize.x,  halfsize.y, -halfsize.z);
			points[3] = center + new Vector3( halfsize.x, -halfsize.y, -halfsize.z);
			points[4] = center + new Vector3(-halfsize.x, -halfsize.y, -halfsize.z);

			points[5] = center + new Vector3(-halfsize.x, -halfsize.y,  halfsize.z);
			points[6] = center + new Vector3(-halfsize.x,  halfsize.y,  halfsize.z);
			points[7] = center + new Vector3( halfsize.x,  halfsize.y,  halfsize.z);
			points[8] = center + new Vector3( halfsize.x, -halfsize.y,  halfsize.z);
			points[9] = center + new Vector3(-halfsize.x, -halfsize.y,  halfsize.z);

			Handles.DrawPolyLine (points);
			Handles.DrawLine (points[1], points[6]);
			Handles.DrawLine (points[2], points[7]);
			Handles.DrawLine (points[3], points[8]);
			
		}

		private void MidpointHandles(ref Vector3 minPos, ref Vector3 maxPos, Matrix4x4 transform)
		{
			Vector3 xAxis = new Vector3(1, 0, 0);
			Vector3 yAxis = new Vector3(0, 1, 0);
			Vector3 zAxis = new Vector3(0, 0, 1);
			Vector3 middle = (maxPos + minPos) * 0.5f;

			// +X
			Vector3 localPos = new Vector3(maxPos.x, middle.y, middle.z);
			Vector3 newPos = MidpointHandle(localPos, yAxis, zAxis, transform);
			maxPos.x = newPos.x;

			// -X
			localPos = new Vector3(minPos.x, middle.y, middle.z);
			newPos = MidpointHandle(localPos, yAxis, -zAxis, transform);
			minPos.x = newPos.x;

			// +Y
			localPos = new Vector3(middle.x, maxPos.y, middle.z);
			newPos = MidpointHandle(localPos, xAxis, -zAxis, transform);
			maxPos.y = newPos.y;
			// -Y
			localPos = new Vector3(middle.x, minPos.y, middle.z);
			newPos = MidpointHandle(localPos, xAxis, zAxis, transform);
			minPos.y = newPos.y;

			if (!m_DisableZaxis)
			{
				// +Z
				localPos = new Vector3 (middle.x, middle.y, maxPos.z);
				newPos = MidpointHandle (localPos, yAxis, -xAxis, transform);
				maxPos.z = newPos.z;
				// -Z
				localPos = new Vector3 (middle.x, middle.y, minPos.z);
				newPos = MidpointHandle (localPos, yAxis, xAxis, transform);
				minPos.z = newPos.z;
			}
		}


		private Vector3 MidpointHandle(Vector3 localPos, Vector3 localTangent, Vector3 localBinormal, Matrix4x4 transform)
		{
			Color orgColor = Handles.color;

			float alphaFactor = 1.0f;
			AdjustMidpointHandleColor(localPos, localTangent, localBinormal, transform, alphaFactor);

			int id = GUIUtility.GetControlID(m_ControlIdHint, FocusType.Keyboard);
			if (alphaFactor > 0.0f)
			{
				Vector3 localDir = Vector3.Cross(localTangent, localBinormal).normalized;
				localPos = Slider1D.Do(id, localPos, localDir, HandleUtility.GetHandleSize(localPos) * 0.03f, Handles.DotCap, SnapSettings.scale);
			}

			Handles.color = orgColor;
			return localPos;
		}

		private void AdjustMidpointHandleColor(Vector3 localPos, Vector3 localTangent, Vector3 localBinormal, Matrix4x4 transform, float alphaFactor)
		{
			Vector3 worldPos = transform.MultiplyPoint(localPos);

			// We can't just pass localNormal and transform it using transform, because if transform
			// contains scale, then normal is no longer perpendicular to the plane, so we pass 
			// tangent & binormal to overcome this problem
			Vector3 worldTangent = transform.MultiplyVector(localTangent);
			Vector3 worldBinormal = transform.MultiplyVector(localBinormal);
			Vector3 worldDir = Vector3.Cross(worldTangent, worldBinormal).normalized;
			
			// Adjust color of handle is backfacing
			float cosV;
			if (Camera.current.isOrthoGraphic)
				cosV = Vector3.Dot(-Camera.current.transform.forward, worldDir);
			else
				cosV = Vector3.Dot((Camera.current.transform.position - worldPos).normalized, worldDir);

			if (cosV < -0.0001f)
				alphaFactor *= Handles.backfaceAlphaMultiplier;

			if (alphaFactor < 1.0f)
				Handles.color = new Color(Handles.color.r, Handles.color.g, Handles.color.b, Handles.color.a * alphaFactor);
		}

		private void EdgeHandles(ref Vector3 minPos, ref Vector3 maxPos, Matrix4x4 transform)
		{
			Vector3 xAxis = new Vector3(1, 0, 0);
			Vector3 yAxis = new Vector3(0, 1, 0);
			Vector3 zAxis = new Vector3(0, 0, 1);

			// XY Plane
			{
				float middleZ = (minPos.z + maxPos.z) * .5f;

				// minX minY 
				Vector3 localPos = new Vector3(minPos.x, minPos.y, middleZ);
				Vector3 newLocalPos = EdgeHandle(localPos, xAxis, -xAxis, -yAxis, transform); 
				minPos.x = newLocalPos.x;
				minPos.y = newLocalPos.y;

				// minX maxY 
				localPos = new Vector3(minPos.x, maxPos.y, middleZ);
				newLocalPos = EdgeHandle(localPos, xAxis, -xAxis, yAxis, transform); 
				minPos.x = newLocalPos.x;
				maxPos.y = newLocalPos.y;

				// maxX maxY 
				localPos = new Vector3(maxPos.x, maxPos.y, middleZ);
				newLocalPos = EdgeHandle(localPos, xAxis, xAxis, yAxis, transform); 
				maxPos.x = newLocalPos.x;
				maxPos.y = newLocalPos.y;

				// maxX minY 
				localPos = new Vector3(maxPos.x, minPos.y, middleZ);
				newLocalPos = EdgeHandle(localPos, xAxis, xAxis, -yAxis, transform); 
				maxPos.x = newLocalPos.x;
				minPos.y = newLocalPos.y;
			}

			// XZ Plane
			{
				float middleY = (minPos.y + maxPos.y) * .5f;

				// minX minZ 
				Vector3 localPos = new Vector3(minPos.x, middleY, minPos.z);
				Vector3 newLocalPos = EdgeHandle(localPos, yAxis, -xAxis, -zAxis, transform);
				minPos.x = newLocalPos.x;
				minPos.z = newLocalPos.z;

				// minX maxZ
				localPos = new Vector3(minPos.x, middleY, maxPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, -xAxis, zAxis, transform);
				minPos.x = newLocalPos.x;
				maxPos.z = newLocalPos.z;

				// maxX maxZ
				localPos = new Vector3(maxPos.x, middleY, maxPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, xAxis, zAxis, transform);
				maxPos.x = newLocalPos.x;
				maxPos.z = newLocalPos.z;

				// maxX minZ 
				localPos = new Vector3(maxPos.x, middleY, minPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, xAxis, -zAxis, transform);
				maxPos.x = newLocalPos.x;
				minPos.z = newLocalPos.z;
			}

			// YZ Plane
			{
				float middleX = (minPos.x + maxPos.x) * .5f;

				// minY minZ 
				Vector3 localPos = new Vector3(middleX, minPos.y, minPos.z);
				Vector3 newLocalPos = EdgeHandle(localPos, yAxis, -yAxis, -zAxis, transform);
				minPos.y = newLocalPos.y;
				minPos.z = newLocalPos.z;

				// minY maxZ 
				localPos = new Vector3(middleX, minPos.y, maxPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, -yAxis, zAxis, transform);
				minPos.y = newLocalPos.y;
				maxPos.z = newLocalPos.z;

				// maxY maxZ 
				localPos = new Vector3(middleX, maxPos.y, maxPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, yAxis, zAxis, transform);
				maxPos.y = newLocalPos.y;
				maxPos.z = newLocalPos.z;

				// maxY minZ 
				localPos = new Vector3(middleX, maxPos.y, minPos.z);
				newLocalPos = EdgeHandle(localPos, yAxis, yAxis, -zAxis, transform);
				maxPos.y = newLocalPos.y;
				minPos.z = newLocalPos.z;
			}

		}

		private Vector3 EdgeHandle(Vector3 handlePos, Vector3 handleDir, Vector3 slideDir1, Vector3 slideDir2, Matrix4x4 transform)
		{
			Color orgColor = Handles.color;

			// Skip 2D handles that are 1D due to camera viewdir
			bool visible = true;
			if (Camera.current)
			{
				Vector3 camPosInLocalSpace = Handles.matrix.inverse.MultiplyPoint (Camera.current.transform.position);
				Vector3 posToCamera = (handlePos - camPosInLocalSpace).normalized;
				Vector3 planeNormal = Vector3.Cross(slideDir1, slideDir2);

				float cosV1 = Vector3.Dot(planeNormal, posToCamera);
				if (Mathf.Acos(Mathf.Abs(cosV1)) > (Mathf.PI * 0.5f - kViewAngleThreshold)) 
					visible = false;
			}

			float alphaFactor = visible ? 1.0f : 0.0f;
			AdjustEdgeHandleColor(handlePos, slideDir1, slideDir2, transform, alphaFactor);

			int id = GUIUtility.GetControlID(m_ControlIdHint, FocusType.Keyboard);
			if (alphaFactor > 0.0f)
				handlePos = Slider2D.Do(id, handlePos, handleDir, slideDir1, slideDir2, HandleUtility.GetHandleSize(handlePos) * 0.03f, Handles.DotCap, SnapSettings.scale, true);

			Handles.color = orgColor;
			return handlePos;
		}


		private void AdjustEdgeHandleColor(Vector3 handlePos, Vector3 slideDir1, Vector3 slideDir2, Matrix4x4 transform, float alphaFactor)
		{
			Vector3 worldHandlePos = transform.MultiplyPoint(handlePos);
			Vector3 worldSlideDir1 = transform.MultiplyVector(slideDir1).normalized;
			Vector3 worldSlideDir2 = transform.MultiplyVector(slideDir2).normalized;

			bool isBackfaceHandle;
			if (Camera.current.isOrthoGraphic)
			{
				isBackfaceHandle =  Vector3.Dot(-Camera.current.transform.forward, worldSlideDir1) < 0.0f &&
									Vector3.Dot(-Camera.current.transform.forward, worldSlideDir2) < 0.0f;
			}
			else
			{
				// Test if camera is in positive half-spaces of the two planes
				Plane plane1 = new Plane(worldSlideDir1, worldHandlePos);
				Plane plane2 = new Plane(worldSlideDir2, worldHandlePos);
				isBackfaceHandle =	!plane1.GetSide(Camera.current.transform.position) &&
									!plane2.GetSide(Camera.current.transform.position);
			}

			if (isBackfaceHandle)
				alphaFactor *= Handles.backfaceAlphaMultiplier;

			if (alphaFactor < 1.0f)
				Handles.color = new Color(Handles.color.r, Handles.color.g, Handles.color.b, Handles.color.a * alphaFactor);
		}
	}
}
