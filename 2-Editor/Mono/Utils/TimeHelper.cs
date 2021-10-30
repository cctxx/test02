

namespace UnityEditor
{

// Silly helper to get proper deltatime inside events
internal struct TimeHelper
{
	public float deltaTime;
	long lastTime;
	
	public void Begin ()
	{
		lastTime = System.DateTime.Now.Ticks;
	}
	
	public float Update ()
	{
		deltaTime = (System.DateTime.Now.Ticks - lastTime) / 10000000.0f;
		lastTime = System.DateTime.Now.Ticks;
		return deltaTime;
	}
}

}
