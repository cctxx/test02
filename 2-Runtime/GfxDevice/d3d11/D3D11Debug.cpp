#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "D3D11Includes.h"


#if ENABLE_DX11_DEBUGGING
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "D3D11VBO.h"


static bool   s_DX11LoggingEnabled = true;
static UInt32 s_DX11CommandCount = 0;
static UInt32 s_Indentation = 0;
static DbgFrameDebugType s_FrameDebugType = kFrameDebugOutputDX11Commands; 
//static DbgFrameDebugType s_FrameDebugType = kFrameDebugOutputNone;
static bool s_BreakpointReached = FALSE;


void DbgDX11DisableLogging()
{
	s_DX11LoggingEnabled = false;
}
void DbgDX11EnableLogging()
{
	s_DX11LoggingEnabled = true;
}
void DbgDX11LogOutput(const char* format,...)
{
	if (!s_DX11LoggingEnabled || s_FrameDebugType == kFrameDebugOutputNone) return;
	
	va_list va;
	va_start (va, format);
	char buffer [1024 * 10];
	vsnprintf (buffer, sizeof(buffer), format, va);
	va_end (va);
	printf_console("%*s[%d]%s\n", s_Indentation, "", ++s_DX11CommandCount, buffer);

}

void DbgDX11IncreaseIndentation()
{
	s_Indentation++;
}
void DbgDX11DecreaseIndentation()
{
	s_Indentation--;
}
void DbgDX11PostOperation()
{
	if (!s_DX11LoggingEnabled) return;
}
void DbgDX11MarkDrawing(UInt32 triCount, UInt32 vertexCount)
{
	if (s_FrameDebugType != kFrameDebugOutputDX11CommandsWithStepping) return;

	printf_console("Submitted draw call, triangle count: %d, vertex count: %d\n", triCount, vertexCount);
	printf_console("------------------------------------------------------------------------------------------------\n");
	DbgDX11ShowCurrentGfxState();
	printf_console("------------------------------------------------------------------------------------------------\n");

	/*
	RenderColorSurfaceWiiU* colorSurface = InternalGetActiveRenderColorSurfaceWiiU(0);
	if (colorSurface != NULL)
	{
		DX11SwapBuffers(&colorSurface->colorBuffer);
	}
	else
	{
		DX11SwapBuffers(cafe::GetColorBuffer());
	}
//	    DX11_CHK (DX11SwapBuffers(&s_ColorBuffer));

	cafe::SetContextState();

	DX11_CHK (DX11Flush());
	printf_console("A - Contine, B - Cancel, + - Capture screenshot\n");

	bool done = false;

	while (!done)
	{

		switch (cafe::DbgWaitForKey())
		{
		case KPAD_BUTTON_A:
			done = true;
			break;
		case KPAD_BUTTON_B:
			DbgDX11SetFrameDebug(kFrameDebugOutputNone);
			break;
		case KPAD_BUTTON_PLUS:
			cafe::DbgCaptureScreenshot();
			break;
		default:
	#if ENABLE_DX11_METRICS
			DbgDX11SetBreakpoint();
			DX11ClearGPMetric();
	#endif
			break;
		}
	}
	*/
}
void DbgDX11SetFrameDebug(DbgFrameDebugType type)
{
	s_DX11CommandCount = 0;

	switch (s_FrameDebugType)
	{
	case kFrameDebugOutputDX11CommandsWithStepping:
		break;
	}

	s_FrameDebugType = type;

	switch (s_FrameDebugType)
	{
	case kFrameDebugOutputDX11CommandsWithStepping:
		break;
	}
}

void DbgDX11MarkFrameBegin()
{
	switch (s_FrameDebugType)
	{
	case kFrameDebugOutputDX11CommandsWithStepping:
		break;
	}
}
void DbgDX11MarkFrameEnd()
{
	DbgDX11SetFrameDebug(kFrameDebugOutputNone);
}


const char* DbgDX11GetShaderChannelName(ShaderChannel channel)
{
	const char* kShaderChannelStrings[kShaderChannelCount] = {
		"kShaderChannelVertex",
		"kShaderChannelNormal",
		"kShaderChannelColor",
		"kShaderChannelTexCoord0",
		"kShaderChannelTexCoord1",
		"kShaderChannelTangent"
	};
	return kShaderChannelStrings[channel];
}
const char* DbgDX11GetVertexComponentName(VertexComponent comp)
{
	const char* kVertexComponentStrings[kVertexCompCount] = {
		"kVertexCompVertex",
		"kVertexCompColor",
		"kVertexCompNormal",
		"kVertexCompTexCoord",
		"kVertexCompTexCoord0", "kVertexCompTexCoord1", "kVertexCompTexCoord2", "kVertexCompTexCoord3",
		"kVertexCompTexCoord4", "kVertexCompTexCoord5", "kVertexCompTexCoord6", "kVertexCompTexCoord7",
		"kVertexCompAttrib0", "kVertexCompAttrib1", "kVertexCompAttrib2", "kVertexCompAttrib3",
		"kVertexCompAttrib4", "kVertexCompAttrib5", "kVertexCompAttrib6", "kVertexCompAttrib7",
		"kVertexCompAttrib8", "kVertexCompAttrib9", "kVertexCompAttrib10", "kVertexCompAttrib11",
		"kVertexCompAttrib12", "kVertexCompAttrib13", "kVertexCompAttrib14", "kVertexCompAttrib15"
	};
	return kVertexComponentStrings[comp];
}
void DbgDX11ShowChannelBindings(const ChannelAssigns& channels, VertexBufferData& data)
{
	/*
	DX11_LOG_OUTPUT("VBO layout, VertexStride: %d", data.vertexStride);
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		DX11_LOG_OUTPUT("  [%d] - 0x%08x, Stride: %d (%s)", i, data.channel[i], data.channelStrides[i], DbgDX11GetShaderChannelName((ShaderChannel)i));
	}
	for (int i = kVertexCompVertex; i < kVertexCompCount; i++)
	{
		ShaderChannel sh = channels.GetSourceForTarget((VertexComponent)i);
		if (sh != kShaderChannelNone)
		{
			DX11_LOG_OUTPUT("  Vertex component %d targets %s", i, DbgDX11GetShaderChannelName((ShaderChannel)sh));
		}
	}
	*/
}
void DbgDX11ShowStridedVertexData(const VertexBufferData& data, void* rawData)
{
		/*
	DX11_LOG_OUTPUT("VBO has %d vertices with stride %d", data.vertexCount, data.vertexStride);

	UInt8* p = (UInt8*) rawData;

	int offset[kShaderChannelCount];
	int curOffset = 0;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		if (data.channel[i] == NULL) offset[i] = -1;
		else
		{
			offset[i] = curOffset;
			curOffset += VBO::GetChannelByteSize(i);
		}
	}
	Assert (VBO::GetChannelByteSize(kShaderChannelVertex) == sizeof(Vector3f));
	Assert (VBO::GetChannelByteSize(kShaderChannelNormal) == sizeof(Vector3f));
	Assert (VBO::GetChannelByteSize(kShaderChannelColor) == sizeof(ColorRGBA32));
	Assert (VBO::GetChannelByteSize(kShaderChannelTexCoord0) == sizeof(Vector2f));
	Assert (VBO::GetChannelByteSize(kShaderChannelTexCoord1) == sizeof(Vector2f));
	Assert (VBO::GetChannelByteSize(kShaderChannelTangent) == sizeof(Vector4f));
	for (int i = 0; i < data.vertexCount; i++)
	{
		if (offset[kShaderChannelVertex] != -1)
		{
			Vector3f v = *(Vector3f*)(p + offset[kShaderChannelVertex]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - Vertex] - %.2f %.2f %.2f", i, v.x, v.y, v.z); 
		}

		if (offset[kShaderChannelNormal] != -1)
		{
			Vector3f n = *(Vector3f*)(p + offset[kShaderChannelNormal]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - Normal] - %.2f %.2f %.2f", i, n.x, n.y, n.z); 
		}
		if (offset[kShaderChannelColor] != -1)
		{
			ColorRGBA32 c = *(ColorRGBA32*)(p + offset[kShaderChannelColor]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - Color] - %d %d %d %d", i, c.r, c.g, c.b, c.a); 
		}
		if (offset[kShaderChannelTexCoord0] != -1)
		{
			Vector2f uv = *(Vector2f*)(p + offset[kShaderChannelTexCoord0]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - UV0] - %.2f %.2f", i, uv.x, uv.x, uv.y); 
		}
		if (offset[kShaderChannelTexCoord1] != -1)
		{
			Vector2f uv = *(Vector2f*)(p + offset[kShaderChannelTexCoord1]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - UV1] - %.2f %.2f", i, uv.x, uv.x, uv.y); 
		}
		if (offset[kShaderChannelTangent] != -1)
		{
			Vector4f t = *(Vector4f*)(p + offset[kShaderChannelTangent]  + i * data.vertexStride);
			DX11_LOG_OUTPUT("[%d - Tangent] - %.2f %.2f %.2f %.2f", i, t.x, t.y, t.z, t.w); 
		}
	}
	*/
	FatalErrorMsg("Stop");
}
void DbgDX11ShowStridedIndexData(const IndexBufferData& data, void* rawData)
{
	/*
	DX11_LOG_OUTPUT("VBO has %d indices with stride %d", data.count, data.stride);
	UInt8* p = (UInt8*) rawData;
	for (int i = 0; i < data.count; i++)
	{
		Assert (data.stride == sizeof(UInt16));
		DX11_LOG_OUTPUT("[%d] - %d", i, *(UInt16*)(p + data.stride * i));
	}
	*/
	FatalErrorMsg("Stop");
}



void DbgDX11ShowCurrentGfxState()
{
	/*
	const DeviceStateWiiU& state = GetWiiUDeviceState(GetGfxDevice());

	int	blendingEnabled = state.currBlendState->blendingEnabled;
	DbgDX11ShowColorControlReg(state.colorControlReg, blendingEnabled);
	if (blendingEnabled > 0)
	{
		DbgDX11ShowBlendControlReg(state.blendControlReg);
	}

	DbgDX11ShowTargetChannelMaskReg(state.targetChannelMaskReg);
	DbgDX11ShowAlphaTestReg(state.alphaTestReg);
	DbgDX11ShowPolygonControlReg(state.polygonControlReg);
	//DbgDX11ShowPolygonOffsetReg(state.polygonOffsetReg);
	DbgDX11ShowDepthStencilControlReg(state.depthStencilReg);
	*/
}


#endif
