#include "UnityPrefix.h"
#include "GfxReturnStructs.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"

static int AppendString(dynamic_array<char>& strBuf, const char* str)
{
	int strLen = strlen(str);
	int strOfs = strBuf.size();
	strBuf.resize_uninitialized(strOfs + strLen + 1);
	std::copy(str, str + strLen + 1, &strBuf[strOfs]);
	return strOfs;
}

static const char* GetStringAtOffset(int offset, const char* strBuf, int strBufLen)
{
	if (offset >= 0 && offset < strBufLen)
		return &strBuf[offset];
	ErrorString("Invalid offset in GetStringAtOffset()");
	return "";
}

GfxRet_ValueParameter::GfxRet_ValueParameter(const GpuProgramParameters::ValueParameter& src, dynamic_array<char>& strBuf)
: index(src.m_Index)
, arraySize(src.m_ArraySize)
, rowCount(src.m_RowCount)
, colCount(src.m_ColCount)
{
	nameStrOfs = AppendString(strBuf, src.m_Name.GetName());
}

void GfxRet_ValueParameter::ToValueParameter(GpuProgramParameters::ValueParameter& dest, const char* strBuf, int strBufLen) const
{
	dest.m_Name = ShaderLab::Property(GetStringAtOffset(nameStrOfs, strBuf, strBufLen));
	dest.m_Index = index;
	dest.m_ArraySize = arraySize;
	dest.m_RowCount = rowCount;
	dest.m_ColCount = colCount;
}


GfxRet_TextureParameter::GfxRet_TextureParameter(const GpuProgramParameters::TextureParameter& src, dynamic_array<char>& strBuf)
: index(src.m_Index)
, samplerIndex(src.m_SamplerIndex)
, dim(src.m_Dim)
{
	nameStrOfs = AppendString(strBuf, src.m_Name.GetName());
}

void GfxRet_TextureParameter::ToTextureParameter(GpuProgramParameters::TextureParameter& dest, const char* strBuf, int strBufLen) const
{
	dest.m_Name = ShaderLab::Property(GetStringAtOffset(nameStrOfs, strBuf, strBufLen));
	dest.m_Index = index;
	dest.m_SamplerIndex = samplerIndex;
	dest.m_Dim = dim;
}


GfxRet_BufferParameter::GfxRet_BufferParameter(const GpuProgramParameters::BufferParameter& src, dynamic_array<char>& strBuf)
: index(src.m_Index)
{
	nameStrOfs = AppendString(strBuf, src.m_Name.GetName());
}

void GfxRet_BufferParameter::ToBufferParameter(GpuProgramParameters::BufferParameter& dest, const char* strBuf, int strBufLen) const
{
	dest.m_Name = ShaderLab::Property(GetStringAtOffset(nameStrOfs, strBuf, strBufLen));
	dest.m_Index = index;
}

GfxRet_ShaderError::GfxRet_ShaderError(const ShaderErrors::ShaderError& src, dynamic_array<char>& strBuf)
: line(src.line)
, warning(src.warning)
, programError(src.programError)
{
	messageStrOfs = AppendString(strBuf, src.message.c_str());
	messageDetailsStrOfs = AppendString(strBuf, src.messageDetails.c_str());
}

void GfxRet_ShaderError::ToShaderError(ShaderErrors::ShaderError& dest, const char* strBuf, int strBufLen) const
{
	dest.message = GetStringAtOffset(messageStrOfs, strBuf, strBufLen);
	dest.messageDetails = GetStringAtOffset(messageDetailsStrOfs, strBuf, strBufLen);
	dest.line = line;
	dest.warning = warning;
	dest.programError = programError;
}


GfxRet_ChannelAssigns::GfxRet_ChannelAssigns(const ChannelAssigns& src)
: targetMap(src.m_TargetMap)
, sourceMap(src.m_SourceMap)
, directlyWired(src.m_DirectlyWired)
{
	std::copy(src.m_Channels, src.m_Channels + kVertexCompCount, channels);
};

void GfxRet_ChannelAssigns::ToChannelAssigns(ChannelAssigns& dest) const
{
	dest.m_TargetMap = targetMap;
	dest.m_SourceMap = sourceMap;
	dest.m_DirectlyWired = directlyWired;
	std::copy(channels, channels + kVertexCompCount, dest.m_Channels);
}

Gfx_GpuProgramParameters::Gfx_GpuProgramParameters()
: valueParameterCount(0)
, valueParameterOffset(0)
, textureParameterCount(0)
, textureParameterOffset(0)
, bufferParameterCount(0)
, bufferParameterOffset(0)
{
}

Gfx_GpuProgramParameters::Gfx_GpuProgramParameters(const GpuProgramParameters& params, dynamic_array<UInt8>& outBuf, dynamic_array<char> &strBuf)
{
	const GpuProgramParameters::ValueParameterArray& srcValueParams = params.GetValueParams();
	valueParameterCount = srcValueParams.size();
	valueParameterOffset = outBuf.size();
	outBuf.resize_uninitialized(valueParameterOffset + valueParameterCount * sizeof(GfxRet_ValueParameter));
	GfxRet_ValueParameter* valueParams = reinterpret_cast<GfxRet_ValueParameter*>(outBuf.data() + valueParameterOffset);
	for (int i = 0; i < valueParameterCount; ++i)
		valueParams[i] = GfxRet_ValueParameter(srcValueParams[i], strBuf);

	const GpuProgramParameters::TextureParameterList& srcTextureParams = params.GetTextureParams();
	textureParameterCount = srcTextureParams.size();
	textureParameterOffset = outBuf.size();
	outBuf.resize_uninitialized(textureParameterOffset + textureParameterCount * sizeof(GfxRet_TextureParameter));
	GfxRet_TextureParameter* textureParams = reinterpret_cast<GfxRet_TextureParameter*>(outBuf.data() + textureParameterOffset);
	for (int i = 0; i < textureParameterCount; ++i)
		textureParams[i] = GfxRet_TextureParameter(srcTextureParams[i], strBuf);

	const GpuProgramParameters::BufferParameterArray& srcBufferParams = params.GetBufferParams();
	bufferParameterCount = srcBufferParams.size();
	bufferParameterOffset = outBuf.size();
	outBuf.resize_uninitialized(bufferParameterOffset + bufferParameterCount * sizeof(GfxRet_BufferParameter));
	GfxRet_BufferParameter* bufferParams = reinterpret_cast<GfxRet_BufferParameter*>(outBuf.data() + bufferParameterOffset);
	for (int i = 0; i < bufferParameterCount; ++i)
		bufferParams[i] = GfxRet_BufferParameter(srcBufferParams[i], strBuf);
}

void Gfx_GpuProgramParameters::GetOutput(GpuProgramParameters& outParams, const char* dataBegin, const char* stringBuffer, int stringBufferLength) const
{
	const GfxRet_ValueParameter* valueParams = reinterpret_cast<const GfxRet_ValueParameter*>(dataBegin + valueParameterOffset);
	for (int i = 0; i < valueParameterCount; ++i)
	{
		GpuProgramParameters::ValueParameter param;
		valueParams[i].ToValueParameter(param, stringBuffer, stringBufferLength);
		outParams.AddValueParam(param);
	}

	const GfxRet_TextureParameter* textureParams = reinterpret_cast<const GfxRet_TextureParameter*>(dataBegin + textureParameterOffset);
	for (int i = 0; i < textureParameterCount; ++i)
	{
		GpuProgramParameters::TextureParameter param;
		textureParams[i].ToTextureParameter(param, stringBuffer, stringBufferLength);
		outParams.AddTextureParam(param);
	}
}

GfxRet_CreateGpuProgram::GfxRet_CreateGpuProgram(const CreateGpuProgramOutput& output, dynamic_array<UInt8>& outBuf)
: channelAssignsCount(0)
, channelAssignsOffset(0)
, shaderErrorCount(0)
, shaderErrorOffset(0)
, stringBufferLength(0)
, stringBufferOffset(0)
{
	perFogModeParamsEnabled = output.GetPerFogModeParamsEnabled();

	// Shared buffer for strings
	dynamic_array<char> strBuf;

	if (output.GetParams())
	{
		params = Gfx_GpuProgramParameters(*output.GetParams(), outBuf, strBuf);
	}

	if (output.GetChannelAssigns())
	{
		channelAssignsCount = 1;
		channelAssignsOffset = outBuf.size();
		outBuf.resize_uninitialized(channelAssignsOffset + channelAssignsCount * sizeof(GfxRet_ChannelAssigns));
		GfxRet_ChannelAssigns* channelAssigns = reinterpret_cast<GfxRet_ChannelAssigns*>(outBuf.data() + channelAssignsOffset);
		channelAssigns[0] = GfxRet_ChannelAssigns(*output.GetChannelAssigns());
	}
	
	if (output.GetShaderErrors())
	{
		const ShaderErrors::ErrorContainer& srcShaderErrors = output.GetShaderErrors()->GetErrors();
		shaderErrorCount = srcShaderErrors.size();
		shaderErrorOffset = outBuf.size();
		outBuf.resize_uninitialized(shaderErrorOffset + shaderErrorCount * sizeof(GfxRet_ShaderError));
		GfxRet_ShaderError* shaderErrors = reinterpret_cast<GfxRet_ShaderError*>(outBuf.data() + shaderErrorOffset);
		ShaderErrors::ErrorContainer::const_iterator srcIt = srcShaderErrors.begin();
		for (int i = 0; i < shaderErrorCount; ++srcIt, ++i)
			new (shaderErrors + i) GfxRet_ShaderError(*srcIt, strBuf);
		Assert(srcIt == srcShaderErrors.end());
	}

	stringBufferLength = strBuf.size();
	stringBufferOffset = outBuf.size();
	outBuf.resize_uninitialized(stringBufferOffset + stringBufferLength);
	std::copy(strBuf.begin(), strBuf.end(), outBuf.begin() + stringBufferOffset);
}

void GfxRet_CreateGpuProgram::GetOutput(CreateGpuProgramOutput& output) const
{
	output.SetPerFogModeParamsEnabled(perFogModeParamsEnabled);

	const char* dataBegin = reinterpret_cast<const char*>(this);
	const char* stringBuffer = dataBegin + stringBufferOffset;

	if (params.valueParameterCount > 0 || params.textureParameterCount > 0 || params.bufferParameterCount > 0)
	{
		GpuProgramParameters& outParams = output.CreateParams();
		params.GetOutput(outParams, dataBegin, stringBuffer, stringBufferLength);
	}

	if (channelAssignsCount > 0)
	{
		Assert(channelAssignsCount == 1);
		const GfxRet_ChannelAssigns* channelAssigns = reinterpret_cast<const GfxRet_ChannelAssigns*>(dataBegin + channelAssignsOffset);
		channelAssigns[0].ToChannelAssigns(output.CreateChannelAssigns());
	}

	if (shaderErrorCount > 0)
	{
		ShaderErrors& outErrors = output.CreateShaderErrors();

		const GfxRet_ShaderError* shaderErrors = reinterpret_cast<const GfxRet_ShaderError*>(dataBegin + shaderErrorOffset);
		for (int i = 0; i < shaderErrorCount; ++i)
		{
			ShaderErrors::ShaderError err;
			shaderErrors[i].ToShaderError(err, stringBuffer, stringBufferLength);
			outErrors.AddShaderError(err.message, err.messageDetails, err.line, err.warning, err.programError);
		}
	}
}
