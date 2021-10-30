#pragma once

#define ENABLE_DX11_DEBUGGING (!UNITY_RELEASE && UNITY_METRO) && 0

#if ENABLE_DX11_DEBUGGING

	enum DbgFrameDebugType
	{
		kFrameDebugOutputNone,
		kFrameDebugOutputDX11Commands,
		kFrameDebugOutputDX11CommandsWithStepping
	};

	void DbgDX11DisableLogging();
	void DbgDX11EnableLogging();
	void DbgDX11LogOutput(const char* format,...);
	void DbgDX11IncreaseIndentation();
	void DbgDX11DecreaseIndentation();
	void DbgDX11MarkDrawing(UInt32 triCount, UInt32 vertexCount);
	void DbgDX11MarkFrameBegin();
	void DbgDX11MarkFrameEnd();
	void DbgDX11ShowCurrentGfxState();

	class DbgDX11AutoIndentation
	{
	public:
		DbgDX11AutoIndentation() {DbgDX11IncreaseIndentation();}
		~DbgDX11AutoIndentation() {DbgDX11DecreaseIndentation();}
	};

	inline const char* GetDX11BoolString (bool val) {return val == true ? "true" : "false";}
	
	#define DX11_DISABLE_LOGGING() DbgDX11DisableLogging()
	#define DX11_ENABLE_LOGGING() DbgDX11EnableLogging()
	#define DX11_LOG_OUTPUT(...) DbgDX11LogOutput(__VA_ARGS__); 
	#define DX11_LOG_ENTER_FUNCTION(...) DbgDX11LogOutput(__VA_ARGS__); DbgDX11AutoIndentation sDbgDX11AutoIndentation; 
	#define DX11_MARK_DRAWING(TriCount, VertexCount) DbgDX11MarkDrawing(TriCount, VertexCount);
	#define DX11_MARK_FRAME_BEGIN() DbgDX11MarkFrameBegin();
	#define DX11_MARK_FRAME_END() DbgDX11MarkFrameEnd();
	#define DX11_CHK(x) {DbgDX11LogOutput(#x); x; }

#else
	#define DX11_DISABLE_LOGGING() {}
	#define DX11_ENABLE_LOGGING() {}
	#define DX11_LOG_OUTPUT(...) {}
	#define DX11_LOG_OUTPUT_MTX3x4(m) {}
	#define DX11_LOG_OUTPUT_MTX4x4(m) {}
	#define DX11_LOG_ENTER_FUNCTION(...) {}
	#define DX11_MARK_FRAME_BEGIN() {}
	#define DX11_MARK_FRAME_END() {}
	#define DX11_CHK(x) x
	#define DX11_MARK_DRAWING(TriCount, VertexCount) {}
	#define DX11_SET_FRAME_DEBUG(Type) {}
#endif
