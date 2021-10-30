using UnityEngine;

namespace UnityEditor.VersionControl
{
	// Shared message class to give plugin messages consistency
	public partial class Message
	{
		public void Show ()
		{
			Message.Info (message);
		}

		private static void Info (string message)
		{
			Debug.Log ("Version control:\n" + message);
		}
	}
}
