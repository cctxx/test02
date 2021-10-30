using UnityEngine;
using System.Collections;
using UnityEditorInternal;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditor
{
	class TransitionPreview
	{
		private AvatarPreview m_AvatarPreview;
		private Timeline m_Timeline;

        private AnimatorController m_Controller;
		private StateMachine m_StateMachine;
		private List<Vector2> m_ParameterMinMax = new List<Vector2>();
		private List<ParameterInfo> m_ParameterInfoList;

		private Transition m_RefTransition;
		private TransitionInfo m_RefTransitionInfo = new TransitionInfo();
		private Transition m_Transition;
		private State m_SrcState;
		private State m_DstState;
		private State m_RefSrcState;
			
		private bool m_ShowBlendValue = false;

		private bool m_MustResample = true;
		private bool m_MustSampleMotions = false;
		public bool mustResample { set { m_MustResample = value; } get { return m_MustResample; } } 
		private float m_LastEvalTime = -1.0f;

		private AvatarMask m_LayerMask;
		private int m_LayerIndex;
		private int m_MotionSetIndex;

        class ParameterInfo
		{
			public string m_Name;
			public float m_Value;			
		}

		int FindParameterInfo(List<ParameterInfo> parameterInfoList, string name)
		{
			int ret = -1;

			for (int i = 0; i < parameterInfoList.Count && ret == -1; i++)
			{
				if (parameterInfoList[i].m_Name == name)
				{
					ret = i;
				}
			}

			return ret;
		}

		class TransitionInfo 
		{
			State m_SrcState;
			State m_DstState;			
			float m_TransitionDuration;
			float m_TransitionOffset;			
			float m_ExitTime;

			public bool IsEqual(TransitionInfo info)
			{
				return m_SrcState == info.m_SrcState &&		 
				m_DstState == info.m_DstState &&				
				Mathf.Approximately(m_TransitionDuration,info.m_TransitionDuration) &&
				Mathf.Approximately(m_TransitionOffset,info.m_TransitionOffset) &&				
				Mathf.Approximately(m_ExitTime, info.m_ExitTime);
			}

			public TransitionInfo()
			{
				Init();
			}

			void Init()
			{
				m_SrcState = null;
				m_DstState = null;				
				m_TransitionDuration = 0.0f;
				m_TransitionOffset = 0.0f;				
				m_ExitTime = 0.5f;
			}

			public void Set(Transition transition, State srcState)
			{
				if(transition != null)
				{
					if (srcState != null)
						m_SrcState = srcState;
					else
						m_SrcState = transition.srcState;
					m_DstState = transition.dstState;
					m_TransitionDuration = transition.duration;
					m_TransitionOffset = transition.offset;
                    m_ExitTime = 0.5f;

                    for (int i = 0; i < transition.conditionCount; i++)
                    {
                    	AnimatorCondition condition = transition.GetCondition(i);
						if (condition.mode == TransitionConditionMode.ExitTime)
						{
							m_ExitTime = condition.exitTime;
                            break;
                        }
                    }
				}
				else
				{
					Init();
				}
			}
		};

        private bool HasExitTime(Transition t)
        {
            for (int i = 0; i < t.conditionCount; i++)
            {
                if (t.GetCondition(i).mode  == TransitionConditionMode.ExitTime)
                {
                    return true;
                }
            }

            return false;
        }

        private void SetExitTime(Transition t, float time)
        {
			for (int i = 0; i < t.conditionCount; i++)
			{
				AnimatorCondition condition = t.GetCondition(i);
				if (condition.mode == TransitionConditionMode.ExitTime)
				{
					condition.exitTime = time;                    
                    break;
                }
            }

            m_ExitTime = time;
        }

        private float GetExitTime(Transition t)
        {
			for (int i = 0; i < t.conditionCount; i++)
            {
				AnimatorCondition condition = t.GetCondition(i);
				if (condition.mode == TransitionConditionMode.ExitTime)
                {
					return condition.exitTime;
                }
            }

            return m_ExitTime;
        }

		private void CopyStateForPreview(State src, ref State dst)
		{
			dst.name = src.name;
			dst.iKOnFeet = src.iKOnFeet;
			dst.speed = src.speed;
            dst.mirror = src.mirror;

			dst.SetMotionInternal(src.GetMotionInternal(m_MotionSetIndex));

			
		}
	
		private void CopyTransitionForPreview(Transition src, ref Transition dst)
		{
			if (src != null)
			{
				dst.duration = src.duration;
				dst.offset = src.offset;

                SetExitTime(dst, GetExitTime(src));
			}
		}

        float m_ExitTime = 0.5f;
		float m_LeftStateWeightA = 0;
		float m_LeftStateWeightB = 1;
		float m_LeftStateTimeA = 0;
		float m_LeftStateTimeB = 1;

		float m_RightStateWeightA = 0;
		float m_RightStateWeightB = 1;
		float m_RightStateTimeA = 0;
		float m_RightStateTimeB = 1;

		List<Timeline.PivotSample> m_SrcPivotList = new List<Timeline.PivotSample>();
		List<Timeline.PivotSample> m_DstPivotList = new List<Timeline.PivotSample>();
		
		private bool MustResample(TransitionInfo info)
		{
			return mustResample || !info.IsEqual(m_RefTransitionInfo);
		}


		private void WriteParametersInController()
		{
			if (m_Controller)
			{
				int parameterCount = m_Controller.parameterCount;

				for (int i = 0; i < parameterCount; i++)
				{
					string parameterName = m_Controller.GetParameterName(i);

					int parameterInfoIndex = FindParameterInfo(m_ParameterInfoList, parameterName);

					if (parameterInfoIndex != -1)
					{
						m_AvatarPreview.Animator.SetFloat(parameterName, m_ParameterInfoList[parameterInfoIndex].m_Value);
					}
				}
			}
		}
		
		private void ResampleTransition(Transition transition, AvatarMask layerMask, TransitionInfo info, Animator previewObject)
		{			
							
			m_MustResample = false;
				
			bool resetTimeSettings = m_RefTransition != transition;
								
			m_RefTransition = transition;
			m_RefTransitionInfo = info;

			m_LayerMask = layerMask;
					
			if (m_AvatarPreview != null)
			{
				m_AvatarPreview.OnDestroy();
				m_AvatarPreview = null;
			}
				
            ClearController();


			Motion sourceStateMotion = (transition.srcState) ? transition.srcState.GetMotionInternal(m_MotionSetIndex) : m_RefSrcState.GetMotionInternal(m_MotionSetIndex);
			Init(previewObject, sourceStateMotion != null ? sourceStateMotion : transition.dstState.GetMotionInternal(m_MotionSetIndex));
			
			if (m_Controller == null) //  did not create controller
				return;
			
			/// sample all frames

			m_StateMachine.defaultState = m_DstState;
            m_Transition.mute = true;
            AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);			
			m_AvatarPreview.Animator.Update(0.00001f);
			WriteParametersInController();
			m_AvatarPreview.Animator.SetLayerWeight(m_LayerIndex, 1);

			float nextStateDuration = m_AvatarPreview.Animator.GetCurrentAnimatorStateInfo(m_LayerIndex).length;

            m_StateMachine.defaultState = m_SrcState;
            m_Transition.mute = false;
            AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);
			m_AvatarPreview.Animator.Update(0.00001f);			
			WriteParametersInController();
			m_AvatarPreview.Animator.SetLayerWeight(m_LayerIndex, 1);			

			float currentStateDuration = m_AvatarPreview.Animator.GetCurrentAnimatorStateInfo(m_LayerIndex).length;
			
			if (m_LayerIndex > 0) m_AvatarPreview.Animator.stabilizeFeet = false;
			float maxDuration = (currentStateDuration * GetExitTime(m_RefTransition)) + (currentStateDuration * m_Transition.duration ) + nextStateDuration;
			
			// case 546812 disable previewer if the duration is too big, otherwise it hang Unity. 2000.0f is an arbitrary choice, it can be increase if needed.
			// in some case we got a m_Transition.duration == Infinity, bail out before unity hang.
			if (maxDuration > 2000.0f)
			{
				Debug.LogWarning("Transition duration is longer than 2000 second, Disabling previewer.");
				return;
			}
			// We want 30 samples/sec, maxed at 300 sample for very long state, and very short animation like 1 frame should at least get 5 sample
			float currentStateStepTime = currentStateDuration > 0 ? Mathf.Min(Mathf.Max(currentStateDuration / 300.0f, 1.0f/30.0f), currentStateDuration / 5.0f) :  1.0f/30.0f;
			float nextStateStepTime = nextStateDuration > 0 ? Mathf.Min(Mathf.Max(nextStateDuration / 300.0f, 1.0f/30.0f), nextStateDuration / 5.0f) : 1.0f/30.0f;

			float stepTime = currentStateStepTime;

			float currentTime = 0.0f;

			bool hasStarted = false;
			bool hasTransitioned = false;
			bool hasFinished = false;

			m_AvatarPreview.Animator.StartRecording(-1);
			
			m_LeftStateWeightA = 0;
			m_LeftStateTimeA = 0;

			m_AvatarPreview.Animator.Update(0.0f);

			while(!hasFinished)
			{                    
				m_AvatarPreview.Animator.Update(stepTime);

				AnimatorStateInfo currentState = m_AvatarPreview.Animator.GetCurrentAnimatorStateInfo(m_LayerIndex);
				currentTime += stepTime;

				if (!hasStarted)
				{
                    m_LeftStateWeightA = currentState.normalizedTime;
					m_LeftStateTimeA = currentTime;

					hasStarted = true;
				}
				
				if (hasTransitioned && currentTime >= maxDuration)
				{
					hasFinished = true;
				}

				if (!hasTransitioned && currentState.IsName(m_DstState.uniqueName))
				{
                    m_RightStateWeightA = currentState.normalizedTime;
					m_RightStateTimeA = currentTime;

					hasTransitioned = true;
				}

				if (!hasTransitioned)
				{
                    m_LeftStateWeightB = currentState.normalizedTime;
					m_LeftStateTimeB = currentTime;
				}

				if (hasTransitioned)
				{
                    m_RightStateWeightB = currentState.normalizedTime;
					m_RightStateTimeB = currentTime;				
				}
			

				if (m_AvatarPreview.Animator.IsInTransition(m_LayerIndex))
				{
					stepTime = nextStateStepTime;
				}
			}
				
			float endTime = currentTime;
			m_AvatarPreview.Animator.StopRecording();
			
				
			float leftDuration =  (m_LeftStateTimeB - m_LeftStateTimeA) / (m_LeftStateWeightB - m_LeftStateWeightA);
			float rightDuration = (m_RightStateTimeB - m_RightStateTimeA) / (m_RightStateWeightB - m_RightStateWeightA);
								
			if (m_MustSampleMotions)
			{
				// Do this as infrequently as possible
				m_MustSampleMotions = false;
				m_SrcPivotList.Clear ();
				m_DstPivotList.Clear ();


				stepTime = nextStateStepTime;
				m_StateMachine.defaultState  = m_DstState;
				m_Transition.mute = true;
                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);				
				m_AvatarPreview.Animator.SetLayerWeight(m_LayerIndex, 1);
				m_AvatarPreview.Animator.Update (0.0000001f);
				WriteParametersInController();
				currentTime = 0.0f;
				while (currentTime <= rightDuration)
				{
					m_AvatarPreview.Animator.Update(stepTime * 2);
					currentTime += stepTime * 2;
					Timeline.PivotSample sample = new Timeline.PivotSample();
					sample.m_Time = currentTime;
					sample.m_Weight = m_AvatarPreview.Animator.pivotWeight;
					m_DstPivotList.Add (sample);
				}

				stepTime = currentStateStepTime;
				m_StateMachine.defaultState = m_SrcState;
	        	m_Transition.mute = true;
                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);				
				m_AvatarPreview.Animator.Update(0.0000001f);
				WriteParametersInController();
				m_AvatarPreview.Animator.SetLayerWeight(m_LayerIndex, 1);
				currentTime = 0.0f;
				while (currentTime <= leftDuration)
				{
					m_AvatarPreview.Animator.Update(stepTime * 2);
					currentTime += stepTime * 2;
					Timeline.PivotSample sample = new Timeline.PivotSample();
					sample.m_Time = currentTime;
					sample.m_Weight = m_AvatarPreview.Animator.pivotWeight;
					m_SrcPivotList.Add (sample);
				}
											

				m_Transition.mute = false;
                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);				
				m_AvatarPreview.Animator.Update(0.0000001f);
				WriteParametersInController();
			}





            m_Timeline.StopTime = m_AvatarPreview.timeControl.stopTime = endTime;
			m_AvatarPreview.timeControl.currentTime = m_Timeline.Time;
			if (resetTimeSettings)
			{
				m_Timeline.Time = m_Timeline.StartTime = m_AvatarPreview.timeControl.currentTime = m_AvatarPreview.timeControl.startTime = 0;				
				m_Timeline.ResetRange();
			}

			m_AvatarPreview.Animator.StartPlayback();

		}

		public void SetAnyStateTransition( Transition transition, State sourceState, AvatarMask layerMask, int motionSetIndex,Animator previewObject)
		{
			TransitionInfo info = new TransitionInfo();
			info.Set(transition, sourceState);
			m_MotionSetIndex = motionSetIndex;

			if (MustResample(info))
			{
				m_RefSrcState = sourceState;
				ResampleTransition(transition, layerMask, info, previewObject);
			}
			
		}

		public void SetTransition(Transition transition, AvatarMask layerMask, int motionSetIndex, Animator previewObject)
		{						
			TransitionInfo info = new TransitionInfo();
			info.Set(transition, null);
			m_MotionSetIndex = motionSetIndex;

			if (MustResample(info))
			{
				ResampleTransition(transition, layerMask, info, previewObject);
			}
		}
		
		
		private void OnPreviewAvatarChanged()
		{
			m_RefTransitionInfo = new TransitionInfo();
			ClearController();			
			CreateController();
			CreateParameterInfoList();
		}
		
		void ClearController()
		{
			if (m_AvatarPreview != null && m_AvatarPreview.Animator != null)
                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, null);
			
			Object.DestroyImmediate(m_Controller);		
			Object.DestroyImmediate(m_SrcState);
			Object.DestroyImmediate(m_DstState);
			Object.DestroyImmediate(m_Transition);

			m_StateMachine = null;			
			m_Controller = null;
			m_SrcState = null;
			m_DstState = null;
			m_Transition = null;
		}
		
		void CreateParameterInfoList()
		{
			m_ParameterInfoList = new List<ParameterInfo>();
		    if (m_Controller)
		    {
			int parameterCount = m_Controller.parameterCount;
			for (int i = 0; i < parameterCount; i++)
			{
				ParameterInfo parameterInfo = new ParameterInfo();
		            parameterInfo.m_Name = m_Controller.GetParameterName(i);
				m_ParameterInfoList.Add(parameterInfo);
			}						
		}
		}

		void CreateController()
		{
			if (m_Controller == null && m_AvatarPreview != null && m_AvatarPreview.Animator != null && m_RefTransition != null)
			{
			
				// controller
				m_LayerIndex = 0;
                m_Controller = new AnimatorController();
				m_Controller.hideFlags = HideFlags.DontSave;
				m_Controller.AddLayer("preview");


				bool isDefaultMask = true;

				if (m_LayerMask != null)
				{
					int maskPartCount = m_LayerMask.humanoidBodyPartCount;
					for (int i = 0; i < maskPartCount && isDefaultMask; i++)
						if (!m_LayerMask.GetHumanoidBodyPartActive(i)) isDefaultMask = false;

					if (!isDefaultMask)
					{
						m_Controller.AddLayer("Additionnal");
						m_LayerIndex++;
						m_Controller.SetLayerMask(m_LayerIndex, m_LayerMask) ;
					}
				}
				m_StateMachine = m_Controller.GetLayerStateMachine(m_LayerIndex);


				State sourceState = (m_RefTransition.srcState) ? m_RefTransition.srcState : m_RefSrcState;

				/// Add parameters 
				m_ParameterMinMax.Clear();
				Motion srcMotion = sourceState.GetMotionInternal(m_MotionSetIndex);
				if (srcMotion && srcMotion is BlendTree)
				{
					BlendTree leftBlendTree = srcMotion as BlendTree;

					for (int i = 0; i < leftBlendTree.recursiveBlendParameterCount; i++)
					{
						string blendValueName = leftBlendTree.GetRecursiveBlendParameter(i);
						if (m_Controller.FindParameter(blendValueName) == -1)
						{
							m_Controller.AddParameter(blendValueName, AnimatorControllerParameterType.Float);
							m_ParameterMinMax.Add(new Vector2(leftBlendTree.GetRecursiveBlendParameterMin(i), leftBlendTree.GetRecursiveBlendParameterMax(i)));
						}
					}
				}

				Motion dstMotion = m_RefTransition.dstState.GetMotionInternal(m_MotionSetIndex);
				if (dstMotion && dstMotion is BlendTree)
				{
					BlendTree rightBlendTree = dstMotion as BlendTree;

					for (int i = 0; i < rightBlendTree.recursiveBlendParameterCount; i++)
					{
						string blendValueName = rightBlendTree.GetRecursiveBlendParameter(i);
						int parameterIndex = m_Controller.FindParameter(blendValueName);
						if (parameterIndex == -1)
						{
							m_Controller.AddParameter(blendValueName, AnimatorControllerParameterType.Float);
							m_ParameterMinMax.Add(new Vector2(rightBlendTree.GetRecursiveBlendParameterMin(i), rightBlendTree.GetRecursiveBlendParameterMax(i)));
						}
						else
						{
							m_ParameterMinMax[parameterIndex] = 
							new Vector2(Mathf.Min(rightBlendTree.GetRecursiveBlendParameterMin(i), m_ParameterMinMax[parameterIndex][0]),
							            Mathf.Max(rightBlendTree.GetRecursiveBlendParameterMax(i), m_ParameterMinMax[parameterIndex][1]));
						}
					}
				}					
			

				// states			
				m_SrcState = m_StateMachine.AddState(sourceState.name);
				m_SrcState.hideFlags = HideFlags.DontSave;
				m_DstState = m_StateMachine.AddState(m_RefTransition.dstState.name);
				m_DstState.hideFlags = HideFlags.DontSave;

				CopyStateForPreview(sourceState, ref m_SrcState);
				CopyStateForPreview(m_RefTransition.dstState, ref m_DstState);

				// transition
				m_Transition = m_StateMachine.AddTransition(m_SrcState, m_DstState);
				m_Transition.hideFlags = HideFlags.DontSave;
				CopyTransitionForPreview(m_RefTransition, ref m_Transition);								


				DisableIKOnFeetIfNeeded();


                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);
			}
		}

		private void DisableIKOnFeetIfNeeded()
		{
			bool disable = false;
			if (m_SrcState.GetMotion() == null || m_DstState.GetMotion() == null)
			{
				disable = true;
			}

			if (m_LayerIndex > 0)
			{
				disable = !m_LayerMask.hasFeetIK;
			}

			if(disable)
			{
				m_SrcState.iKOnFeet = false;
				m_DstState.iKOnFeet = false;
			}
		}

		private void Init(Animator scenePreviewObject, Motion motion)
		{
			if (m_AvatarPreview == null)
			{
				m_AvatarPreview = new AvatarPreview(scenePreviewObject, motion); 
				m_AvatarPreview.OnAvatarChangeFunc = OnPreviewAvatarChanged;
				m_AvatarPreview.ShowIKOnFeetButton = false;
			}
			
			if (m_Timeline == null)
			{
				m_Timeline = new Timeline();
				m_MustSampleMotions = true;
			}

			CreateController();

			if(m_ParameterInfoList == null)
			{
				CreateParameterInfoList();
			}
		}

		public void DoTransitionPreview()
		{
			if (m_Controller == null)
				return;
			
			if (Event.current.type == EventType.Repaint)
				m_AvatarPreview.timeControl.Update (); 
			
			DoTimeline ();
			
			// Draw the blend values
			if (m_Controller.parameterCount > 0)
			{
				m_ShowBlendValue = EditorGUILayout.Foldout(m_ShowBlendValue, "BlendTree Parameters");

				if (m_ShowBlendValue)
				{
					for (int i = 0; i < m_Controller.parameterCount; i++)
					{
                        string name = m_Controller.GetParameterName(i);
						float value = m_ParameterInfoList[i].m_Value;
						float newValue = EditorGUILayout.Slider(name, value, m_ParameterMinMax[i][0], m_ParameterMinMax[i][1]);
 						if (newValue != value)
						{
							m_ParameterInfoList[i].m_Value = newValue;
							mustResample = true;
							m_MustSampleMotions = true;
 						}
					}
				}
			}			
		}
		
		
		private void DoTimeline ()
		{
			// get local durations
			float srcStateDuration =  (m_LeftStateTimeB - m_LeftStateTimeA) / (m_LeftStateWeightB - m_LeftStateWeightA);
			float dstStateDuration = (m_RightStateTimeB - m_RightStateTimeA) / (m_RightStateWeightB - m_RightStateWeightA);
			float transitionDuration =  m_Transition.duration * srcStateDuration;		
						
			// Set the timeline values
			m_Timeline.SrcStartTime = 0f;
			m_Timeline.SrcStopTime = srcStateDuration;
			m_Timeline.SrcName = m_SrcState.name;
			m_Timeline.HasExitTime = HasExitTime(m_RefTransition);
			
			m_Timeline.srcLoop = m_SrcState.GetMotion() ? m_SrcState.GetMotion().isLooping : false;
			m_Timeline.dstLoop = m_DstState.GetMotion() ? m_DstState.GetMotion().isLooping : false;

			m_Timeline.TransitionStartTime = GetExitTime(m_RefTransition) * srcStateDuration;
			m_Timeline.TransitionStopTime = m_Timeline.TransitionStartTime + transitionDuration;
						
			m_Timeline.Time = m_AvatarPreview.timeControl.currentTime;
			
			m_Timeline.DstStartTime = m_Timeline.TransitionStartTime - m_RefTransition.offset * dstStateDuration;
			m_Timeline.DstStopTime =  m_Timeline.DstStartTime + dstStateDuration;
			
			if (m_Timeline.TransitionStopTime == Mathf.Infinity)
				m_Timeline.TransitionStopTime = Mathf.Min (m_Timeline.DstStopTime, m_Timeline.SrcStopTime);
			
			
			m_Timeline.DstName = m_DstState.name;
						
			m_Timeline.SrcPivotList = m_SrcPivotList;
			m_Timeline.DstPivotList = m_DstPivotList;
						
			// Do the timeline
			Rect previewRect = EditorGUILayout.GetControlRect(false, 150, EditorStyles.label);
			
			EditorGUI.BeginChangeCheck();
			
			bool changedData = m_Timeline.DoTimeline(previewRect);
			
			if (EditorGUI.EndChangeCheck())
			{
				if(changedData)				
				{
					Undo.RegisterCompleteObjectUndo(m_RefTransition, "Edit Transition");
					SetExitTime(m_RefTransition, m_Timeline.TransitionStartTime / m_Timeline.SrcDuration);
					m_RefTransition.duration = m_Timeline.TransitionDuration / m_Timeline.SrcDuration;
					m_RefTransition.offset = (m_Timeline.TransitionStartTime - m_Timeline.DstStartTime) / m_Timeline.DstDuration; 					
				}				
				
				m_AvatarPreview.timeControl.nextCurrentTime = Mathf.Clamp (m_Timeline.Time, 0, m_AvatarPreview.timeControl.stopTime);
			}			
		}		
				
		public void OnDisable()
		{
			ClearController();
		}

		public void OnDestroy()
		{
			ClearController();

			if (m_Timeline != null)
			{
				m_Timeline = null;
			}
			
			if(m_AvatarPreview != null)
			{
				m_AvatarPreview.OnDestroy();
				m_AvatarPreview = null;
			}
		}

		public bool HasPreviewGUI()
		{
			return true;
		}

		public void OnPreviewSettings()
		{
			if(m_AvatarPreview!= null)
				m_AvatarPreview.DoPreviewSettings();
		}

		public void OnInteractivePreviewGUI(Rect r, GUIStyle background)
		{
			if (m_AvatarPreview != null && m_Controller != null)
			{
				if (m_LastEvalTime != m_AvatarPreview.timeControl.currentTime && Event.current.type == EventType.Repaint)
				{
					m_AvatarPreview.Animator.playbackTime = m_AvatarPreview.timeControl.currentTime;
					m_AvatarPreview.Animator.Update(0);
					m_LastEvalTime = m_AvatarPreview.timeControl.currentTime;
				}

			    m_AvatarPreview.DoAvatarPreview(r, background);
			}
		}
	  
	}
}//namespace UnityEditor
