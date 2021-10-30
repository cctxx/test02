#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "GraphicsHelper.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/intshader.h"

namespace GraphicsHelper {
	
void Clear( UInt32 clearFlags, const float color[4], float depth, int stencil )
{
	GfxDevice &device = GetGfxDevice();
	
	
#if UNITY_WIN
	int viewport[4];
	device.GetViewport(viewport);
	
	bool canUseNativeClear =
	gGraphicsCaps.hasNonFullscreenClear ||
	(viewport[0]==0 && viewport[1]==0 && viewport[2]==device.GetCurrentTargetWidth() && viewport[3]==device.GetCurrentTargetHeight());
	if (!canUseNativeClear)
	{
#if ENABLE_MULTITHREADED_CODE
		DebugAssert(Thread::CurrentThreadIsMainThread());
#endif
		
		// Some devices (e.g. D3D11) can't clear sub-areas of a render target, so must draw a quad instead.
		Shader* clearShader = Shader::GetScreenClearShader();
		if (!clearShader || clearShader->GetShaderLabShader()->GetActiveSubShader().GetValidPassCount() != 4)
		{
			AssertString ("Valid screen clear shader not found");
			return;
		}
		ShaderLab::SubShader& ss = clearShader->GetShaderLabShader()->GetActiveSubShader();
		const int clearIndex = clearFlags & 3;
		ss.GetPass (clearIndex)->ApplyPass(0, NULL);
		
		bool oldWireframe = device.GetWireframe();
		device.SetWireframe (false);
		DeviceMVPMatricesState saveMVPMatrices;
		LoadFullScreenOrthoMatrix ();
		device.ImmediateBegin (kPrimitiveQuads);
		device.ImmediateColor(color[0], color[1], color[2], color[3]);
		float z = -100.0f - 1e-9f;
		device.ImmediateVertex (0.0f, 0.0f, z);
		device.ImmediateVertex (0.0f, 1.0f, z);
		device.ImmediateVertex (1.0f, 1.0f, z);
		device.ImmediateVertex (1.0f, 0.0f, z);
		device.ImmediateEnd ();
		device.SetWireframe (oldWireframe);
		return;
	}
#endif
	
	device.Clear(clearFlags, color, depth, stencil);
}

void SetBlendState( const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props )
{
	GfxDevice& device = GetGfxDevice();
	if (device.IsRecording())
		device.RecordSetBlendState(state, alphaRef, props);
	else
		device.SetBlendState(state, alphaRef.ToFloat(props));
}

void SetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props )
{
	GfxDevice& device = GetGfxDevice();
	if (device.IsRecording())
		device.RecordSetMaterial(ambient, diffuse, specular, emissive, shininess, props);
	else
		device.SetMaterial(ambient.Get(props).GetPtr(), diffuse.Get(props).GetPtr(), specular.Get(props).GetPtr(), emissive.Get(props).GetPtr(), shininess.ToFloat(props));
}

void SetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props )
{
	GfxDevice& device = GetGfxDevice();
	if (device.IsRecording())
		device.RecordSetColor(color, props);
	else
		device.SetColor(color.Get(props).GetPtr());
}

void EnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props )
{
	GfxDevice& device = GetGfxDevice();
	if (device.IsRecording())
		device.RecordEnableFog(fogMode, fogStart, fogEnd, fogDensity, fogColor, props);
	else
	{
		GfxFogParams fog;
		fog.mode = fogMode;
		fog.color = fogColor.Get(props);
		fog.start = fogStart.ToFloat(props);
		fog.end = fogEnd.ToFloat(props);
		fog.density = fogDensity.ToFloat(props);
		device.EnableFog(fog);
	}
}

} // namespace GfxDeviceHelper