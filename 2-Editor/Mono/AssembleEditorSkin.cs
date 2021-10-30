using UnityEngine;
using UnityEditorInternal;
using System.Collections.Generic;
using System.IO;

namespace UnityEditor 
{

// Class used by Tools/build_resources to assemble the skins from all images and source skins.
// Main entry point is DoIt, but there's also some UI that's part of developer builds. That UI is mainly for tweaking, etc. 
// If you find values using the UI, you need to enter them into the constants below (as build_resources just uses those).
class AssembleEditorSkin : EditorWindow
{
	// Color levels for texture generation
	static float s_InBlack = 0;
	static float s_InWhite = 1;
	static float s_Gamma = 1.328f;
	static float s_OutBlack = 0;
	static float s_OutWhite = .297f;
	static float s_TextBrightness = .705f;

	// Generated destination files
	const string kGeneratedDirectory = "Editor Default Resources/Builtin Skins/Generated/";
	const string kDestDarkFile = kGeneratedDirectory + "DarkSkin";
	const string kDestLightFile = kGeneratedDirectory + "LightSkin";

	// Source paths to work our magic from
	static readonly string[] kSrcImageDirs = {"Editor Default Resources/Builtin Skins/LightSkin/Images/", "Editor Default Resources/Builtin Skins/LightSkin/FlowImages/"};
	static readonly string[] kSrcOverrideImageDirs = { "Editor Default Resources/Builtin Skins/DarkSkin/Images/", "Editor Default Resources/Builtin Skins/DarkSkin/FlowImages/" };
	const string kSrcLightSkins = "Editor Default Resources/Builtin Skins/LightSkin/Skins/";
	const string kSrcDarkSkins = "Editor Default Resources/Builtin Skins/DarkSkin/Skins/";
	const string kSrcSharedSkins = "Editor Default Resources/Builtin Skins/Shared/Skins/";

	public static void DoIt () 
	{
		// Make sure Generated directory exists
		Directory.CreateDirectory ("Assets/" + kGeneratedDirectory);
		
		// Assemble the light skin from all skins in SrcLightSkins and SrcSharedSkins
		GUISkin light = BuildSkin (kDestLightFile, kSrcLightSkins, kSrcSharedSkins);
		
		// Use the light skin to build the dark one.
		// Textures can be overridden by matching the name in s_OverrideImageDir
		GUISkin dark;
		dark = CopyLightToDark (light, kSrcImageDirs, kSrcOverrideImageDirs, kDestDarkFile);
		
		// Override the skins from darkskins override and the shared skins
		AddSkinsFromDir (dark, kSrcDarkSkins);
		AddSkinsFromDir (dark, kSrcSharedSkins);
			
		AssetDatabase.SaveAssets ();
		
		InternalEditorUtility.RepaintAllViews ();
		InternalEditorUtility.RequestScriptReload ();
	}
	
	static GUISkin BuildSkin (string destFilePath, params string[] sourceDirPaths) 
	{
		// Recycle or create the dest GUISkin
		GUISkin destSkin = AssetDatabase.LoadAssetAtPath ("Assets/" + destFilePath + ".guiskin", typeof (GUISkin)) as GUISkin;
		if (destSkin == null)
		{
				string dstPath = "Assets/" + destFilePath + ".guiskin";
				AssetDatabase.DeleteAsset (dstPath);
				destSkin = ScriptableObject.CreateInstance<GUISkin> ();
				AssetDatabase.CreateAsset (destSkin, dstPath);
		} 
		destSkin.customStyles = new GUIStyle[] {};

		foreach (string dir in sourceDirPaths)
			AddSkinsFromDir (destSkin, dir);

		return destSkin;
	}
	
	static void AddSkinsFromDir (GUISkin dest, string dir)
	{
		// Build a sorteddict of all existing styles
		SortedDictionary<string, GUIStyle> styles = new SortedDictionary<string, GUIStyle> ();
		foreach (GUIStyle gs in dest.customStyles)
			styles[gs.name] = gs;

		foreach(FileInfo fi in new DirectoryInfo(Application.dataPath + "/" + dir).GetFiles("*.guiskin"))
		{
			GUISkin srcSkin = (GUISkin)AssetDatabase.LoadAssetAtPath("Assets/" + dir + fi.Name, typeof (GUISkin));		
			AddSkin (srcSkin, styles, dest, fi.Name == "base.guiskin");
		}
		foreach(FileInfo fi in new DirectoryInfo(Application.dataPath + "/" + dir).GetFiles("*.GUISkin"))
		{
			GUISkin srcSkin = (GUISkin)AssetDatabase.LoadAssetAtPath("Assets/" + dir + fi.Name, typeof (GUISkin));		
			AddSkin (srcSkin, styles, dest, fi.Name == "base.GUISkin");
		}
		GUIStyle[] foos = new GUIStyle[styles.Count];
		styles.Values.CopyTo(foos, 0);
		dest.customStyles = foos;		
		EditorUtility.SetDirty (dest);
	}
		
	static void AddSkin (GUISkin source, SortedDictionary<string, GUIStyle> styles, GUISkin dest, bool includeBuiltins)
	{
		if (includeBuiltins)
		{
			dest.font = source.font;
			dest.settings.doubleClickSelectsWord = source.settings.doubleClickSelectsWord;
			dest.settings.tripleClickSelectsLine = source.settings.tripleClickSelectsLine;
			dest.settings.cursorColor = source.settings.cursorColor;
			dest.settings.cursorFlashSpeed = source.settings.cursorFlashSpeed;
			dest.settings.selectionColor = source.settings.selectionColor;

			dest.box = new GUIStyle (source.box);
			dest.button = new GUIStyle (source.button);
			dest.toggle = new GUIStyle (source.toggle);
			dest.label = new GUIStyle (source.label);
			dest.textField = new GUIStyle (source.textField);
			dest.textArea = new GUIStyle (source.textArea);
			dest.window = new GUIStyle (source.window);
			dest.horizontalSlider = new GUIStyle (source.horizontalSlider);
			dest.horizontalSliderThumb = new GUIStyle (source.horizontalSliderThumb);
			dest.verticalSlider = new GUIStyle (source.verticalSlider);
			dest.verticalSliderThumb = new GUIStyle (source.verticalSliderThumb);
			dest.horizontalScrollbar = new GUIStyle (source.horizontalScrollbar);
			dest.horizontalScrollbarThumb = new GUIStyle (source.horizontalScrollbarThumb);
			dest.horizontalScrollbarLeftButton = new GUIStyle (source.horizontalScrollbarLeftButton);
			dest.horizontalScrollbarRightButton = new GUIStyle (source.horizontalScrollbarRightButton);
			dest.verticalScrollbar = new GUIStyle (source.verticalScrollbar);
			dest.verticalScrollbarThumb = new GUIStyle (source.verticalScrollbarThumb);
			dest.verticalScrollbarUpButton = new GUIStyle (source.verticalScrollbarUpButton);
			dest.verticalScrollbarDownButton = new GUIStyle (source.verticalScrollbarDownButton);
			dest.scrollView = new GUIStyle (source.scrollView);
		}
			
		foreach (GUIStyle gs in source.customStyles)
			styles[gs.name] = new GUIStyle (gs);
	}
	
	
	public static GUISkin CopyLightToDark (GUISkin srcSkin, string[] sourceTexturePaths, string[] overrideTexturePaths, string dest)
	{
		// Load all textures
		Dictionary<Texture2D, Texture2D> remaps = new Dictionary<Texture2D, Texture2D>();
		foreach (string sourceTexturePath in sourceTexturePaths)
		{
			// .Net has no Dictionary.AddRange? Guess I'll just do it myself
			Dictionary<Texture2D, Texture2D> tempRemaps = LoadAllTexturesAtPath(sourceTexturePath);
			foreach (KeyValuePair<Texture2D, Texture2D> keyValuePair in tempRemaps)
				remaps[keyValuePair.Key] = keyValuePair.Value;
		}
		
		// Apply levels on them.
		AdjustTextures (remaps.Values);
		
		// Load override textures
		List<Texture2D> overrides = new List<Texture2D>();
		foreach (string overrideTexturePath in overrideTexturePaths)
		{
			overrides.AddRange(LoadAllTexturesAtPath(overrideTexturePath).Values);
		}

		// change the remaps by name.
		// This is a really stoopid way to do it, but the arrays should only be <1k members over the next couple of years.
		foreach (Texture2D tex in overrides)
		{
			foreach (KeyValuePair<Texture2D, Texture2D> kvp in remaps)
			{
				if (kvp.Value.name == tex.name)
				{
					DestroyImmediate (remaps[kvp.Key]);
					remaps[kvp.Key] = tex;
					break;
				}
			}
		}
		
			// create a skin with remapped textures and modified colors
		GUISkin gs = null;
		if (srcSkin)
		{
			gs = AssetDatabase.LoadAssetAtPath ("Assets/" + dest + ".guiskin", typeof(GUISkin)) as GUISkin;
			if (gs == null)
			{
				string destPath = "Assets/" + dest + ".GUISkin";
				AssetDatabase.DeleteAsset (destPath);
				gs = ScriptableObject.CreateInstance<GUISkin> ();
				AssetDatabase.CreateAsset (gs, destPath);
			}
			
			RemapSkin (srcSkin, gs, remaps);
			RecolorSkin (gs);
		}
		// Figure out which textures are generated as part of the skin.
		List<Object> stuffToSave = new List<Object> ();
		foreach (Texture2D tex in remaps.Values)
		{
			if (!EditorUtility.IsPersistent (tex))
				stuffToSave.Add (tex);
			else 
				Debug.Log ("skipping tex " + tex.name);
		}
		SaveAllFilesToAsset (stuffToSave, dest + "_Textures");
		
		
		return gs;
	}
	
	static void SaveAllFilesToAsset (List<Object> objs, string fName)
	{
		string destPath = "Assets/" + fName + ".asset";
		AssetDatabase.CreateAsset (objs[0], destPath);
		
		for (int i = 1; i < objs.Count; i++)
			AssetDatabase.AddObjectToAsset (objs[i], destPath);
	}
	
	static Dictionary<Texture2D, Texture2D> LoadAllTexturesAtPath (string path)
	{
		Dictionary<Texture2D, Texture2D> retval = new Dictionary<Texture2D, Texture2D> ();
		foreach(FileInfo fi in new DirectoryInfo(Application.dataPath + "/" + path).GetFiles("*.png"))
		{
			Texture2D key = (Texture2D)AssetDatabase.LoadAssetAtPath ("Assets/" + path + fi.Name, typeof(Texture2D));
			if (key)
			{
				WWW www = new WWW ("file://" + fi.FullName);
				Texture2D tex =  www.texture;
				if (tex) 
				{
					tex.name = fi.Name;
					tex.wrapMode = TextureWrapMode.Clamp;
					retval[key] = tex;
				}
			}
		}
		return retval;
	}
	
	static void AdjustTextures (IEnumerable<Texture2D> incoming)
	{
		foreach (Texture2D tex in incoming)
		{
			DarkenTexture (tex, tex);
		}
	}
	
	
	static void DarkenTexture (Texture2D tex, Texture2D dest)
	{
		Color[] pixels = tex.GetPixels (0);
		for (int i = 0; i < pixels.Length; i++)
		{
			Color c = pixels[i];
			c.r = ApplyLevels (c.r, s_InBlack, s_InWhite, s_Gamma, s_OutBlack, s_OutWhite);
			c.g = ApplyLevels (c.g, s_InBlack, s_InWhite, s_Gamma, s_OutBlack, s_OutWhite);
			c.b = ApplyLevels (c.b, s_InBlack, s_InWhite, s_Gamma, s_OutBlack, s_OutWhite);
			pixels[i] = c;
	 	}
	 	dest.SetPixels (pixels, 0);
	 	dest.Apply ();
	}
	
	static float ApplyLevels (float color, float inBlack, float inWhite, float gamma, float outBlack, float outWhite)
	{
		color = Mathf.Clamp01 ((color - inBlack) / (inWhite - inBlack));
		color = Mathf.Pow (color, gamma);
		return outBlack + color * (outWhite - outBlack);
	}
	
	// Copy a skin from src to dest, remapping all textures according to remaps.
	static void RemapSkin (GUISkin source, GUISkin dest, Dictionary<Texture2D, Texture2D> remap)
	{
		dest.font = source.font;
		dest.settings.doubleClickSelectsWord = source.settings.doubleClickSelectsWord;
		dest.settings.tripleClickSelectsLine = source.settings.tripleClickSelectsLine;
		dest.settings.cursorColor = RecalcColor (source.settings.cursorColor);
		dest.settings.cursorFlashSpeed = source.settings.cursorFlashSpeed;
		dest.settings.selectionColor = source.settings.selectionColor;		
		
		dest.box = RemapStyle (source.box, remap);
		dest.button = RemapStyle (source.button, remap);
		dest.toggle = RemapStyle (source.toggle, remap);
		dest.label = RemapStyle (source.label, remap);
		dest.textField = RemapStyle (source.textField, remap);
		dest.textArea = RemapStyle (source.textArea, remap);
		dest.window = RemapStyle (source.window, remap);
		dest.horizontalSlider = RemapStyle (source.horizontalSlider, remap);
		dest.horizontalSliderThumb = RemapStyle (source.horizontalSliderThumb, remap);
		dest.verticalSlider = RemapStyle (source.verticalSlider, remap);
		dest.verticalSliderThumb = RemapStyle (source.verticalSliderThumb, remap);
		dest.horizontalScrollbar = RemapStyle (source.horizontalScrollbar, remap);
		dest.horizontalScrollbarThumb = RemapStyle (source.horizontalScrollbarThumb, remap);
		dest.horizontalScrollbarLeftButton = RemapStyle (source.horizontalScrollbarLeftButton, remap);
		dest.horizontalScrollbarRightButton = RemapStyle (source.horizontalScrollbarRightButton, remap);
		dest.verticalScrollbar = RemapStyle (source.verticalScrollbar, remap);
		dest.verticalScrollbarThumb = RemapStyle (source.verticalScrollbarThumb, remap);
		dest.verticalScrollbarUpButton = RemapStyle (source.verticalScrollbarUpButton, remap);
		dest.verticalScrollbarDownButton = RemapStyle (source.verticalScrollbarDownButton, remap);
		dest.scrollView = RemapStyle (source.scrollView, remap);
	
		List<GUIStyle> customs = new List<GUIStyle> ();
		foreach (GUIStyle gs in source.customStyles)
			customs.Add (RemapStyle (gs, remap));

		dest.customStyles = customs.ToArray ();
	}
	
	static GUIStyle RemapStyle (GUIStyle src, Dictionary<Texture2D, Texture2D> remaps)
	{
		GUIStyle gs = new GUIStyle (src);
		gs.normal.background = RemapTex (gs.normal.background, remaps);
		gs.active.background = RemapTex (gs.active.background, remaps);
		gs.hover.background = RemapTex (gs.hover.background, remaps);
		gs.focused.background = RemapTex (gs.focused.background, remaps);
		gs.onNormal.background = RemapTex (gs.onNormal.background, remaps);
		gs.onActive.background = RemapTex (gs.onActive.background, remaps);
		gs.onHover.background = RemapTex (gs.onHover.background, remaps);
		gs.onFocused.background = RemapTex (gs.onFocused.background, remaps);
		return gs;
	}

	static Texture2D RemapTex (Texture2D src, Dictionary<Texture2D, Texture2D> remaps)
	{
		if (src == null)
			return null;
		return remaps.ContainsKey (src) ? remaps[src] : src;
	}
	
	// Recolor all styles in the skin using HLS-based color inversion.
	static void RecolorSkin (GUISkin skin)
	{
		RecolorStyle (skin.box);
		RecolorStyle (skin.button);
		RecolorStyle (skin.toggle);
		RecolorStyle (skin.label);
		RecolorStyle (skin.textField);
		RecolorStyle (skin.textArea);
		RecolorStyle (skin.window);
		RecolorStyle (skin.horizontalSlider);
		RecolorStyle (skin.horizontalSliderThumb);
		RecolorStyle (skin.verticalSlider);
		RecolorStyle (skin.verticalSliderThumb);
		RecolorStyle (skin.horizontalScrollbar);
		RecolorStyle (skin.horizontalScrollbarThumb);
		RecolorStyle (skin.horizontalScrollbarLeftButton);
		RecolorStyle (skin.horizontalScrollbarRightButton);
		RecolorStyle (skin.verticalScrollbar);
		RecolorStyle (skin.verticalScrollbarThumb);
		RecolorStyle (skin.verticalScrollbarUpButton);
		RecolorStyle (skin.verticalScrollbarDownButton);
		RecolorStyle (skin.scrollView);

		foreach (GUIStyle s in skin.customStyles)
			RecolorStyle (s);
	}

	static void RecolorStyle (GUIStyle s)
	{
		s.normal.textColor = RecalcColor (s.normal.textColor);
		s.active.textColor = RecalcColor (s.active.textColor);
		s.hover.textColor = RecalcColor (s.hover.textColor);
		s.focused.textColor = RecalcColor (s.focused.textColor);
		s.onNormal.textColor = RecalcColor (s.onNormal.textColor);
		s.onActive.textColor = RecalcColor (s.onActive.textColor);
		s.onHover.textColor = RecalcColor (s.onHover.textColor);
		s.onFocused.textColor = RecalcColor (s.onFocused.textColor);
	}
	
	static Color RecalcColor (Color c)
	{
		float h, l, s;
		RGB2HLS (c.r, c.g, c.b, out h, out l, out s);
		l = s_TextBrightness - s_TextBrightness * l;
		Color rgb = new Color (0, 0, 0, c.a);
		HLS2RGB (h, l, s, out rgb.r, out rgb.g, out rgb.b);
		return rgb;
	}
	
	static void RGB2HLS (float r, float g, float b, out float h, out float l, out float s)
	{
		float v;
		float m;
		float vm;
		float r2, g2, b2;
	
		h = 0;
		// default to black
		s = 0;
		l = 0;
		v = Mathf.Max (r, g);
		v = Mathf.Max (v, b);
		m = Mathf.Min (r, g);
		m = Mathf.Min (m, b);
		l = (m + v) / 2.0f;
		if (l <= 0.0)
			return;

		vm = v - m;
		s = vm;
		if (s > 0.0)
		{
			s /= (l <= 0.5f) ? (v + m) : (2.0f - v - m);
		}
		else
		{
			return;
		}
		r2 = (v - r) / vm;
		g2 = (v - g) / vm;
		b2 = (v - b) / vm;
		if (r == v)
		{
			h = (g == m ? 5.0f + b2 : 1.0f - g2);
		}
		else if (g == v)
		{
			h = (b == m ? 1.0f + r2 : 3.0f - b2);
		}
		else
		{
			h = (r == m ? 3.0f + g2 : 5.0f - r2);
		}
		h /= 6.0f;
		if (h == 1f)
			h = 0f;
	}
	
	static void HLS2RGB (float h, float l, float s, out float r, out float g, out float b)
	{
		float v;
 
		r = l;   // default to gray
		g = l;
		b = l;
		v = (l <= 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
		if (v > 0)
		{
			float m;
			float sv;
			int sextant;
			float fract, vsf, mid1, mid2;
 
			m = l + l - v;
			sv = (v - m ) / v;
			h *= 6f;
			sextant = (int)h;
			fract = h - sextant;
			vsf = v * sv * fract;
			mid1 = m + vsf;
			mid2 = v - vsf;
			switch (sextant)
			{
			case 0:
				r = v;
				g = mid1;
				b = m;
				break;
			case 1:
				r = mid2;
				g = v;
				b = m;
				break;
			case 2:
				r = m;
				g = v;
				b = mid1;
				break;
			case 3:
				r = m;
				g = mid2;
				b = v;
				break;
			case 4:
				r = mid1;
				g = m;
				b = v;
				break;
			case 5:
				r = v;
				g = m;
				b = mid2;
				break;
			}
		}		
	}

	// UI CODE IS HERE
	// ====================================================
	float inBlack = s_InBlack, inWhite = s_InWhite, gamma = s_Gamma, outBlack = s_OutBlack, outWhite = s_OutWhite, textBrightness = s_TextBrightness;
	
	static void Init ()
	{
		AssembleEditorSkin win = (AssembleEditorSkin)EditorWindow.GetWindow (typeof(AssembleEditorSkin));
		win.inBlack = s_InBlack;
		win.inWhite = s_InWhite;
		win.gamma = s_Gamma;
		win.outBlack = s_OutBlack;
		win.outWhite = s_OutWhite;
		win.textBrightness = s_TextBrightness;
	}
	
	public void OnGUI ()
	{		
		GUILayout.Label ("Image Remapping", EditorStyles.boldLabel);
		GUI.changed = false;
		s_InBlack = inBlack = EditorGUILayout.Slider (new GUIContent ("In Black"), inBlack, 0, 1);
		s_Gamma = gamma = EditorGUILayout.Slider (new GUIContent ("Gamma"), gamma, 0, 2);
		s_InWhite = inWhite = EditorGUILayout.Slider (new GUIContent ("In White"), inWhite, 0, 1);
		s_OutBlack = outBlack = EditorGUILayout.Slider (new GUIContent ("Out Black"), outBlack, 0, 1);

		s_OutWhite = outWhite = EditorGUILayout.Slider (new GUIContent ("Out White"), outWhite, 0, 1);
		
		s_TextBrightness = textBrightness = EditorGUILayout.Slider (new GUIContent ("Text Brightness "), textBrightness, 0, 1);
		float c = ApplyLevels (0.76f, s_InBlack, s_InWhite, s_Gamma, s_OutBlack, s_OutWhite);

		GUILayout.Label ("Background: " + c);
		if (GUILayout.Button ("DO IT - update all controls", "button"))
			DoIt ();
		
		bool wasDark = EditorGUIUtility.isProSkin;
		bool darkSkin = EditorGUILayout.Toggle ("Use Dark Skin", wasDark);
		if (darkSkin != wasDark)
			InternalEditorUtility.SwitchSkinAndRepaintAllViews ();

		MakeButtonRow ("button", "toggle", "label", "radio");
		MakeButtonRow ("button", "buttonleft", "buttonmid", "buttonright", "popup", "DropDown");
		MakeButtonRow ("largebutton", "largebuttonleft", "largebuttonmid", "largebuttonright", "largepopup", "largeDropDown");
		MakeButtonRow ("button", "boldLabel", "label");		
		MakeButtonRow ("button", "ControlLabel", "minibutton", "minipopup", "minipulldown");
		MakeButtonRow ("button", "ControlLabel", "minibuttonleft", "minibuttonmid", "minibuttonright", "miniBoldLabel");
		MakeButtonRow ("button", "label", "IN Label", "TextField", "TextArea");
	}
	
	bool test = false;
	void MakeButtonRow (params string[] styles)
	{
		GUILayout.BeginHorizontal ();
		foreach (string s in styles)
			test = GUILayout.Toggle (test, s, s);
		
		GUILayout.EndHorizontal ();
	}

	// Called from c++ 
	static void RegenerateAllIconsWithMipLevels ()
	{
		GenerateIconsWithMipLevels.GenerateAllIconsWithMipLevels ();
	}

	// Called from c++ 
	static void RegenerateSelectedIconsWithMipLevels ()
	{
		GenerateIconsWithMipLevels.GenerateSelectedIconsWithMips();
	}

	
}

}
