using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public class AudioSourceNodes
	{
		[Logic (typeof(AudioSource))]
		public static IEnumerator PlayOneShot (AudioSource self, AudioClip clip, float volume)
		{
			self.PlayOneShot (clip, volume);

			yield return new WaitForSeconds (clip.length);
		}
	}
}


