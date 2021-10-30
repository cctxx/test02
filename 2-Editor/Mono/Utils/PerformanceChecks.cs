using UnityEngine;
using UnityEditor;
using System;

namespace UnityEditor.Utils
{
	internal class PerformanceChecks
	{
		static string[] kShadersWithMobileVariants = {
			"VertexLit",
			"Diffuse",
			"Bumped Diffuse",
			"Bumped Specular",
			"Particles/Additive",
			"Particles/VertexLit Blended",
			"Particles/Alpha Blended",
			"Particles/Multiply",
			"RenderFX/Skybox"
		};

		private static bool IsMobileBuildTarget (BuildTarget target)
		{
			return target==BuildTarget.iPhone || target==BuildTarget.Android || target==BuildTarget.Tizen;
		}

		private static string FormattedTextContent (string localeString, params string[] args)
		{
			var content = EditorGUIUtility.TextContent (localeString);
			return string.Format(content.text, args);
		}

		public static string CheckMaterial (Material mat, BuildTarget buildTarget)
		{
			if (mat == null || mat.shader == null)
				return null;
			string shaderName = mat.shader.name;
			int shaderLOD = ShaderUtil.GetLOD(mat.shader);
			bool hasMobileVariant = Array.Exists (kShadersWithMobileVariants, s => s==shaderName);
			bool isMobileTarget = IsMobileBuildTarget (buildTarget);

			// check for clip usage on android (adreno driver bugs)
			// should be very first check as it is very important
			if( buildTarget==BuildTarget.Android && ShaderUtil.HasClip(mat.shader) )
			{
				return FormattedTextContent ("PerformanceChecks.ShaderWithClipAndroid");
			}

			// shaders that have faster / simpler equivalents already
			if (hasMobileVariant)
			{
				// has default white color?
				if (isMobileTarget && mat.HasProperty("_Color") && mat.GetColor("_Color") == new Color(1.0f,1.0f,1.0f,1.0f))
				{
					return FormattedTextContent ("PerformanceChecks.ShaderUsesWhiteColor", "Mobile/" + shaderName);
				}

				// recommend Mobile particle shaders on mobile platforms
				if (isMobileTarget && shaderName.StartsWith("Particles/"))
				{
					return FormattedTextContent ("PerformanceChecks.ShaderHasMobileVariant", "Mobile/" + shaderName);
				}

				// has default skybox tint color?
				if (shaderName == "RenderFX/Skybox" && mat.HasProperty("_Tint") && mat.GetColor("_Tint") == new Color(0.5f,0.5f,0.5f,0.5f))
				{
					return FormattedTextContent ("PerformanceChecks.ShaderMobileSkybox", "Mobile/Skybox");
				}
			}

			// recommend "something simpler" for complex shaders on mobile platforms
			if (shaderLOD >= 300 && isMobileTarget && !shaderName.StartsWith("Mobile/"))
			{
				return FormattedTextContent ("PerformanceChecks.ShaderExpensive");
			}

			// vertex lit shader with max. emission: recommend Unlit shaders
			if (shaderName.Contains("VertexLit") && mat.HasProperty("_Emission"))
			{
				Color col = mat.GetColor("_Emission");
				if (col.r >= 0.5f && col.g >= 0.5f && col.b >= 0.5f)
				{
					return FormattedTextContent ("PerformanceChecks.ShaderUseUnlit");
				}
			}

			// normalmapped shader without a normalmap: recommend non-normal mapped one
			if (mat.HasProperty("_BumpMap") && mat.GetTexture("_BumpMap")==null)
			{
				return FormattedTextContent ("PerformanceChecks.ShaderNoNormalMap");
			}

			return null;
		}
	}
}
