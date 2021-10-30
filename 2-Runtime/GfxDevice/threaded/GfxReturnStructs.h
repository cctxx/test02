#pragma once

#include "Runtime/GfxDevice/GpuProgram.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
class ChannelAssigns;
namespace ShaderLab { class SubProgram; }

struct GfxRet_ValueParameter
{
	GfxRet_ValueParameter(const GpuProgramParameters::ValueParameter& src, dynamic_array<char>& strBuf);
	void ToValueParameter(GpuProgramParameters::ValueParameter& dest, const char* strBuf, int strBufLen) const;

	int nameStrOfs;
	int index;
	int arraySize;
	UInt8 rowCount;
	UInt8 colCount;
};

struct GfxRet_TextureParameter
{
	GfxRet_TextureParameter(const GpuProgramParameters::TextureParameter& src, dynamic_array<char>& strBuf);
	void ToTextureParameter(GpuProgramParameters::TextureParameter& dest, const char* strBuf, int strBufLen) const;

	int nameStrOfs;
	int index;
	int samplerIndex;
	TextureDimension dim;
};

struct GfxRet_BufferParameter
{
	GfxRet_BufferParameter(const GpuProgramParameters::BufferParameter& src, dynamic_array<char>& strBuf);
	void ToBufferParameter(GpuProgramParameters::BufferParameter& dest, const char* strBuf, int strBufLen) const;
	
	int nameStrOfs;
	int index;
};

struct GfxRet_ChannelAssigns
{
	GfxRet_ChannelAssigns(const ChannelAssigns& src);
	void ToChannelAssigns(ChannelAssigns& dest) const;

	UInt32 targetMap;
	UInt32 sourceMap;
	SInt8 channels[kVertexCompCount];
	bool directlyWired;
};

struct GfxRet_ShaderError
{
	GfxRet_ShaderError(const ShaderErrors::ShaderError& src, dynamic_array<char>& strBuf);
	void ToShaderError(ShaderErrors::ShaderError& dest, const char* strBuf, int strBufLen) const;
	
	int	messageStrOfs;
	int messageDetailsStrOfs;
	int line;
	bool warning;
	bool programError;
};

struct Gfx_GpuProgramParameters
{
	Gfx_GpuProgramParameters();
	Gfx_GpuProgramParameters(const GpuProgramParameters& params, dynamic_array<UInt8>& outBuf, dynamic_array<char> &strBuf);

	//void GetOutput(GpuProgramParameters &output, const char* strBuf, int strBufLen) const;
	void GetOutput(GpuProgramParameters& outParams, const char* dataBegin, const char* strBuf, int strBufLen) const;

	int valueParameterCount;
	int valueParameterOffset;

	// Array of GfxRet_TextureParameter
	int textureParameterCount;
	int textureParameterOffset;
	
	// Array of GfxRet_BufferParameter
	int bufferParameterCount;
	int bufferParameterOffset;	
};

struct GfxRet_CreateGpuProgram
{
	GfxRet_CreateGpuProgram(const CreateGpuProgramOutput& output, dynamic_array<UInt8>& outBuf);
	
	void GetOutput(CreateGpuProgramOutput& output) const;

	bool perFogModeParamsEnabled;

	Gfx_GpuProgramParameters params;
	/*
	// Array of GfxRet_ValueParameter
	int valueParameterCount;
	int valueParameterOffset;

	// Array of GfxRet_TextureParameter
	int textureParameterCount;
	int textureParameterOffset;
	
	// Array of GfxRet_BufferParameter
	int bufferParameterCount;
	int bufferParameterOffset;
	*/
	// Array of GfxRet_ChannelAssigns
	int channelAssignsCount;
	int channelAssignsOffset;
	
	// Array of GfxRet_ShaderError
	int shaderErrorCount;
	int shaderErrorOffset;

	// Shared buffer of string data
	int stringBufferLength;
	int stringBufferOffset;

	ClientIDMapper::ClientID gpuProgram;
	
	PropertyNamesSet names;
};

