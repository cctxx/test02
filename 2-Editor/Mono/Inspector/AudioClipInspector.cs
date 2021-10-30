using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditor
{
	[CustomEditor(typeof(AudioClip))]
	[CanEditMultipleObjects]
	internal class AudioClipInspector : Editor
	{
		private AudioClip m_PlayingClip = null;
		private bool playing { get { return m_PlayingClip != null; } }
		Vector2 m_Position = Vector2.zero;
		GUIView m_GUI;
		
		static GUIStyle s_PreButton;
		
		static Rect m_wantedRect;
		static bool m_bAutoPlay;
		static bool m_bLoop = false;
		static bool m_bPlayFirst;
		
		static GUIContent[] s_PlayIcons = {null, null};
		static GUIContent[] s_AutoPlayIcons = {null, null};
		static GUIContent[] s_LoopIcons = {null, null};
		
		static Texture2D s_DefaultIcon;

		override public void OnInspectorGUI()
		{
			// Override with inspector that doesn't show anything
		}
		
		static void Init () {
			if (s_PreButton != null)
				return;
			s_PreButton = "preButton";
			
			m_bAutoPlay = EditorPrefs.GetBool("AutoPlayAudio", false);
			m_bLoop = false;
			
			s_AutoPlayIcons[0] = EditorGUIUtility.IconContent ("preAudioAutoPlayOff", "Turn Auto Play on");
			s_AutoPlayIcons[1] = EditorGUIUtility.IconContent ("preAudioAutoPlayOn", "Turn Auto Play off");
			s_PlayIcons[0] = EditorGUIUtility.IconContent ("preAudioPlayOff", "Play");
			s_PlayIcons[1] = EditorGUIUtility.IconContent ("preAudioPlayOn", "Stop");
			s_LoopIcons[0] = EditorGUIUtility.IconContent ("preAudioLoopOff", "Loop on");
			s_LoopIcons[1] = EditorGUIUtility.IconContent ("preAudioLoopOn", "Loop off");
			
			s_DefaultIcon = EditorGUIUtility.LoadIcon ("Profiler.Audio");
		}

	
		public void OnDisable () {
			AudioUtil.StopAllClips();
			AudioUtil.ClearWaveForm( target as AudioClip );
			
			EditorPrefs.SetBool("AutoPlayAudio", m_bAutoPlay);
		}
		
		public void OnEnable() {
			m_bAutoPlay = EditorPrefs.GetBool("AutoPlayAudio", false);
			if (m_bAutoPlay)
				m_bPlayFirst = true;
		}
		
		public override Texture2D RenderStaticPreview(string assetPath, Object[] subAssets, int width, int height)
		{
		
			AudioImporter importer = AssetImporter.GetAtPath(assetPath) as AudioImporter;
            if (!importer)
            {
                // The asset was most likely a movie and a preview cannot be generated at this stage for
                // for the Audio component.
                return null;
            }

            AudioClip clip = target as AudioClip;
            Texture2D[] waveForms = new Texture2D[clip.channels];

            for (int i = 0; i < clip.channels; ++i)
                waveForms[i] = AudioUtil.GetWaveForm(clip, importer, i, width, height / clip.channels);

		    return CombineWaveForms (waveForms);
		}
		
		public override bool HasPreviewGUI()
		{
			return (targets != null);
		}
		
		public override void OnPreviewSettings ()
		{	
			if (s_DefaultIcon == null) Init ();
			
			AudioClip clip = target as AudioClip;
			
			EditorGUI.BeginDisabledGroup (AudioUtil.IsMovieAudio(clip));
			{

				bool isEditingMultipleObjects = targets.Length > 1;
			
				EditorGUI.BeginDisabledGroup(isEditingMultipleObjects);
				{
					m_bAutoPlay = isEditingMultipleObjects ? false : m_bAutoPlay;
					m_bAutoPlay = PreviewGUI.CycleButton(m_bAutoPlay?1:0, s_AutoPlayIcons) != 0;
				} EditorGUI.EndDisabledGroup ();
				
				bool l = m_bLoop;
				m_bLoop = PreviewGUI.CycleButton(m_bLoop?1:0, s_LoopIcons) != 0;
				if ((l != m_bLoop) && playing)
					AudioUtil.LoopClip(clip, m_bLoop);
				
				EditorGUI.BeginDisabledGroup(isEditingMultipleObjects && !playing);
				{
					bool newPlaying = PreviewGUI.CycleButton(playing?1:0, s_PlayIcons) != 0;
					
					if (newPlaying != playing)
					{
						if (newPlaying)
						{
							AudioUtil.PlayClip(clip, 0, m_bLoop);
							m_PlayingClip = clip;
						}
						else
						{
							AudioUtil.StopAllClips ();
							m_PlayingClip = null;
						}
					}
				} EditorGUI.EndDisabledGroup ();
			
			} EditorGUI.EndDisabledGroup ();
		}
		
		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (s_DefaultIcon == null) Init ();
			
			AudioClip clip = target as AudioClip;
			
			Event evt = Event.current;
			if (evt.type != EventType.Repaint && evt.type != EventType.Layout && evt.type != EventType.Used)
			{
				int px2sample = (AudioUtil.GetSampleCount(clip) / (int)r.width);
			
				switch (evt.type) {
					case EventType.MouseDrag:
					case EventType.MouseDown:
					{
						if (r.Contains (evt.mousePosition) && !AudioUtil.IsMovieAudio(clip))
						{
							if (m_PlayingClip != clip)
							{
								AudioUtil.StopAllClips ();
								AudioUtil.PlayClip (clip, 0, m_bLoop);
								m_PlayingClip = clip;
							}
							AudioUtil.SetClipSamplePosition( clip, px2sample * (int)evt.mousePosition.x );
							evt.Use();
						}
					}
					break;
				}					
				return;
			}
			
			if (Event.current.type == EventType.Repaint)
				background.Draw(r, false, false, false, false);
			
			int c = AudioUtil.GetChannelCount(clip);
			m_wantedRect = new Rect (r.x, r.y , r.width, r.height);				
			float sec2px = ((float)m_wantedRect.width / clip.length);
			
			bool previewAble = AudioUtil.HasPreview(clip) || !(AudioUtil.IsMOD(clip) || AudioUtil.IsMovieAudio(clip));
			if (!previewAble)
			{
				float labelY = (r.height > 150)? r.y + (r.height/2) - 10 :  r.y +  (r.height/2) - 25;
				if (r.width > 64)
				{
					if (AudioUtil.IsMOD(clip))
					{
						EditorGUI.DropShadowLabel (new Rect (r.x, labelY, r.width, 20), string.Format("Module file with "+AudioUtil.GetMODChannelCount(clip) + " channels."));	
					}
					else
					if (AudioUtil.IsMovieAudio(clip))
					{
						if (r.width > 390)
							EditorGUI.DropShadowLabel (new Rect (r.x, labelY, r.width, 20), "Audio is attached to a movie. To audition the sound, play the movie.");
						else
						{
							EditorGUI.DropShadowLabel (new Rect (r.x, labelY, r.width, 20), "Audio is attached to a movie.");
							EditorGUI.DropShadowLabel (new Rect (r.x, labelY + 10, r.width, 20), "To audition the sound, play the movie.");
						}			
					}
					else
						EditorGUI.DropShadowLabel (new Rect (r.x, labelY, r.width, 20), "Can not show PCM data for this file");				
				}	

				if (m_PlayingClip == clip)
				{
					float t = AudioUtil.GetClipPosition(clip);	
					
					System.TimeSpan ts = new System.TimeSpan(0,0,0, 0, (int)(t*1000.0f));
										
					EditorGUI.DropShadowLabel (new Rect (m_wantedRect.x, m_wantedRect.y, m_wantedRect.width, 20), string.Format("Playing - {0:00}:{1:00}.{2:000}", ts.Minutes, ts.Seconds, ts.Milliseconds));
				}	
			}
			else
			{
				PreviewGUI.BeginScrollView (m_wantedRect, m_Position, m_wantedRect, "PreHorizontalScrollbar", "PreHorizontalScrollbarThumb");
				
				
				Texture2D previewTexture = null;
				
				 // asynchronous loading
				if (r.width < 100) 
				   previewTexture = AssetPreview.GetAssetPreview(clip);
				else
				   previewTexture = AudioUtil.GetWaveFormFast(clip, 1, 0, clip.samples, r.width, r.height);				
				
				if (previewTexture == null)
				{
						Rect defaultIconRect = new Rect ();
						defaultIconRect.x = (m_wantedRect.width - s_DefaultIcon.width) / 2f + m_wantedRect.x;
						defaultIconRect.y = (m_wantedRect.height - s_DefaultIcon.height) / 2f + m_wantedRect.y;
						defaultIconRect.width = s_DefaultIcon.width;
						defaultIconRect.height = s_DefaultIcon.height;
					
						GUI.DrawTexture(defaultIconRect, s_DefaultIcon);
					
					Repaint ();
				}
				else
					GUI.DrawTexture(new Rect(m_wantedRect.x , m_wantedRect.y, m_wantedRect.width, m_wantedRect.height), previewTexture);
				
				for (int i=0;i<c;++i)
				{
					if (c>1 && r.width > 64)
					{
						var labelRect = new Rect (m_wantedRect.x + 5, m_wantedRect.y + (m_wantedRect.height / c) * i, 30, 20);
						EditorGUI.DropShadowLabel (labelRect, "ch " + (i+1).ToString());
					}
				}
				
				if (m_PlayingClip == clip)
				{
					float t = AudioUtil.GetClipPosition(clip);	
					
					System.TimeSpan ts = new System.TimeSpan(0,0,0, 0, (int)(t*1000.0f));
									
					GUI.DrawTexture(new Rect(m_wantedRect.x + (int)(sec2px * t), m_wantedRect.y, 2, m_wantedRect.height), EditorGUIUtility.whiteTexture );
					if (r.width > 64)
						EditorGUI.DropShadowLabel (new Rect (m_wantedRect.x, m_wantedRect.y, m_wantedRect.width, 20), string.Format("{0:00}:{1:00}.{2:000}", ts.Minutes, ts.Seconds, ts.Milliseconds));				
					else
						EditorGUI.DropShadowLabel (new Rect (m_wantedRect.x, m_wantedRect.y, m_wantedRect.width, 20), string.Format("{0:00}:{1:00}", ts.Minutes, ts.Seconds));				
					
					if(!AudioUtil.IsClipPlaying(clip))
						m_PlayingClip = null;
				}
				
							
				PreviewGUI.EndScrollView();		
			}	
			
			
			// autoplay start?
			if (m_bPlayFirst)
			{
				AudioUtil.PlayClip(clip, 0, m_bLoop);
				m_PlayingClip = clip;
				m_bPlayFirst = false;
			}
			
			// force update GUI
			if (playing)
				GUIView.current.Repaint();
		}
		
		private static Texture2D CombineWaveForms (Texture2D[] waveForms)
		{
			if (waveForms.Length == 1)
				return waveForms[0];
			
			int previewWidth = waveForms[0].width;
			
			int previewHeight = 0;
			foreach (Texture2D texture in waveForms)
				previewHeight += texture.height;
			
			var output = new Texture2D (previewWidth, previewHeight, TextureFormat.ARGB32, false);
			
			int y = 0;
			foreach (Texture2D waveForm in waveForms)
			{
				y += waveForm.height;
				output.SetPixels(0, previewHeight - y, previewWidth, waveForm.height, waveForm.GetPixels ());
				DestroyImmediate (waveForm);
			}
			
			output.Apply ();
			
			return output;
		}
		
		public override string GetInfoString()
		{
			AudioClip clip = target as AudioClip;
			int c = AudioUtil.GetChannelCount(clip);
			string ch = c==1?"Mono":c==2?"Stereo":(c-1).ToString() + ".1";		
			string s = "";
			if (AudioUtil.GetClipType(clip) != AudioType.MPEG)
				s = AudioUtil.GetBitsPerSample(clip) + " bit, " + AudioUtil.GetFrequency(clip) + " Hz, " + ch + ", ";
			else
				s = AudioUtil.GetFrequency(clip) + " Hz, " + ch + ", ";
				
            System.TimeSpan ts = new System.TimeSpan(0,0,0,0, (int) AudioUtil.GetDuration(clip));
            
            if ((uint) AudioUtil.GetDuration(clip) == 0xffffffff)
            	s += "Unlimited";
            else
                s += string.Format("{0:00}:{1:00}.{2:000}", ts.Minutes, ts.Seconds, ts.Milliseconds);

			s += ", ";
			
			s += EditorUtility.FormatBytes(AudioUtil.GetSize(clip));
			
			// show final format (wav, mp3, ogg)
			s += " (" + AudioUtil.GetClipType(clip) + ")";
			
			return s;	
		}
		
	}	
}


