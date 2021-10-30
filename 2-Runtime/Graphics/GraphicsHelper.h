#pragma once

// Remove this when SetShaders has been inlined
#include <Runtime/GfxDevice/GfxDevice.h>
#include "External/shaderlab/Library/program.h"

namespace ShaderLab {
	class FloatVal;
	struct VectorVal;
	struct TextureBinding;
	class PropertySheet;
}

namespace GraphicsHelper
{
	void	Clear( UInt32 clearFlags, const float color[4], float depth, int stencil );
		
	void	SetBlendState( const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props );
	void	SetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props );
	void	SetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props );
	void	EnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props );

	//TextureCombinersHandle CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular );
	//void	SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props );

	// This is inlined until GfxDeviceClient has been moved out of GfxDevice module
	inline void SetShaders (GfxDevice& device, ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props)
	{
		// We do this outside of GfxDevice module to avoid dependency on ShaderLab

		if (device.IsThreadable())
		{
			// This is a threadable device running in single-threaded mode
			GfxThreadableDevice& threadableDevice = static_cast<GfxThreadableDevice&>(device);

			// GLSL-like platforms might result in different set of shader parameters
			// for fog modes.
			const bool useGLStyleFogParams = (device.GetRenderer() == kGfxRendererOpenGL ||
											  device.GetRenderer() == kGfxRendererOpenGLES20Desktop ||
											  device.GetRenderer() == kGfxRendererOpenGLES20Mobile ||
											  device.GetRenderer() == kGfxRendererOpenGLES30);
			const FogMode fogMode = useGLStyleFogParams ? device.GetFogParams().mode : kFogDisabled;

			UInt8* paramsBuffer[kShaderTypeCount];
			for (int pt = 0; pt < kShaderTypeCount; ++pt)
			{
				paramsBuffer[pt] = NULL;
				ShaderLab::SubProgram* prog = programs[pt];
				if (!prog)
					continue;

				GpuProgramParameters* params = &prog->GetParams(fogMode);
				if (params && !params->IsReady())
					threadableDevice.CreateShaderParameters (prog, fogMode);

				int bufferSize = params->GetValuesSize();
				if (bufferSize > 0)
				{
					paramsBuffer[pt] = ALLOC_TEMP_MANUAL (UInt8, bufferSize);
					params->PrepareValues (props, paramsBuffer[pt]);
				}
			}

			GpuProgram* gpuPrograms[kShaderTypeCount];
			for (int pt = 0; pt < kShaderTypeCount; ++pt)
			{
				if (programs[pt])
					gpuPrograms[pt] = &programs[pt]->GetGpuProgram();
				else
					gpuPrograms[pt] = NULL;
 			}

			const GpuProgramParameters* params[kShaderTypeCount];
			for (int pt = 0; pt < kShaderTypeCount; ++pt)
			{
				if (programs[pt])
					params[pt] = &programs[pt]->GetParams(fogMode);
				else
					params[pt] = NULL;
			}

			threadableDevice.SetShadersThreadable (gpuPrograms, params, paramsBuffer);

			for (int pt = 0; pt < kShaderTypeCount; ++pt)
			{
				if (paramsBuffer[pt])
					FREE_TEMP_MANUAL(paramsBuffer[pt]);
			}
		}
		else
		{
			// Old school device that doesn't support threadable interface,
			// or GfxDeviceClient that wants to deal with parameters itself
			device.SetShadersMainThread(programs, props);
		}
	}
	
}