using UnityEngine;
using UnityEditor;
using System.Collections;

namespace UnityEditor {

internal class RenderThumbnailUtility
{
	public static Bounds CalculateVisibleBounds (GameObject prefab)
	{
		return prefab.renderer.bounds;
	}

	public static Texture2D Render (GameObject prefab)
	{
		if (prefab == null)
			return null;
		if (prefab.renderer == null)
			return null;

		EditorUtility.SetTemporarilyAllowIndieRenderTexture(true);

		Texture2D texture = new Texture2D (64, 64);
		texture.hideFlags = HideFlags.HideAndDontSave;
		texture.name = "Preview Texture";
		RenderTexture renderTexture = RenderTexture.GetTemporary(texture.width, texture.height);
		
		GameObject cameraGO = new GameObject ("Preview");
		cameraGO.hideFlags = HideFlags.HideAndDontSave;
		// Setup camera
		Camera camera = cameraGO.AddComponent (typeof(Camera)) as Camera;
		camera.clearFlags = CameraClearFlags.Color;
		camera.backgroundColor = new Color (0.5F, 0.5F, 0.5F, 0);
		camera.cullingMask = 0;
		camera.enabled = false;
		camera.targetTexture = renderTexture;

		// Setup light
		Light light = cameraGO.AddComponent (typeof(Light)) as Light;
		light.type = LightType.Directional;
		
		Bounds bounds = CalculateVisibleBounds(prefab);
	
		Vector3 direction = new Vector3 (0.7F, 0.3F, 0.7F);
		float cameraDistance = bounds.extents.magnitude * 1.6F;
		cameraGO.transform.position = bounds.center + direction.normalized * cameraDistance;
		cameraGO.transform.LookAt (bounds.center);
		camera.nearClipPlane = cameraDistance * 0.1f;
		camera.farClipPlane = cameraDistance * 2.2f;

		// Prepare renderer
		Camera oldCamera = Camera.current;
		camera.RenderDontRestore();
		
		// Setup single light
		Light[] lights = { light };
		Graphics.SetupVertexLights(lights);

		// And render the whole shebang
		Component[] renderers = prefab.GetComponentsInChildren (typeof(Renderer));
		foreach (Renderer r in renderers)
		{
			if (!r.enabled)
				continue;

			Material[] materials = r.sharedMaterials;
			for (int m=0;m<materials.Length;m++)
			{
			
				if (materials[m] == null)
					 continue;
				Material currentMaterial = materials[m];

				string replace = ShaderUtil.GetDependency (currentMaterial.shader, "BillboardShader");
				if (replace != null && replace != "")
				{
					currentMaterial = Object.Instantiate(currentMaterial) as Material;
					currentMaterial.shader = Shader.Find(replace);
					currentMaterial.hideFlags = HideFlags.HideAndDontSave;
				}
				
				for (int p=0;p<currentMaterial.passCount;p++)
				{
					if (!currentMaterial.SetPass(p))
						continue;
					
					r.Render(m);
				}
				
				if (currentMaterial != materials[m])
					Object.DestroyImmediate(currentMaterial);
			}
		}
		
		// Read from the render texture into the texture
		texture.ReadPixels(new Rect (0, 0, texture.width, texture.height), 0, 0);

		//  Cleanup
		RenderTexture.ReleaseTemporary(renderTexture);
		Object.DestroyImmediate(cameraGO);
		
		// Revert to last camera
		Camera.SetupCurrent(oldCamera);

		EditorUtility.SetTemporarilyAllowIndieRenderTexture(false);

		return texture;
	}
}

} //namespace
