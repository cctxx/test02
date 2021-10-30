// Written by Aras Pranckevicius
// I hereby place this code in the public domain

#pragma once

#include "External/Wodka/wodka_WinHelper.h"

#if WODKA_USE_D3DCOMPILER_47
#define kD3D11CompilerDLL "D3DCompiler_47.dll"
#elif WODKA_USE_D3DCOMPILER_46
#define kD3D11CompilerDLL "D3DCompiler_46.dll"
#else
#define kD3D11CompilerDLL "D3DCompiler_43.dll"
#endif

// Copied from c:\Program Files (x86)\Windows Kits\8.0\Include\um\d3d10shader.h

#define D3D10_SHADER_DEBUG                          (1 << 0)
#define D3D10_SHADER_SKIP_VALIDATION                (1 << 1)
#define D3D10_SHADER_SKIP_OPTIMIZATION              (1 << 2)
#define D3D10_SHADER_PACK_MATRIX_ROW_MAJOR          (1 << 3)
#define D3D10_SHADER_PACK_MATRIX_COLUMN_MAJOR       (1 << 4)
#define D3D10_SHADER_PARTIAL_PRECISION              (1 << 5)
#define D3D10_SHADER_FORCE_VS_SOFTWARE_NO_OPT       (1 << 6)
#define D3D10_SHADER_FORCE_PS_SOFTWARE_NO_OPT       (1 << 7)
#define D3D10_SHADER_NO_PRESHADER                   (1 << 8)
#define D3D10_SHADER_AVOID_FLOW_CONTROL             (1 << 9)
#define D3D10_SHADER_PREFER_FLOW_CONTROL            (1 << 10)
#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#define D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY (1 << 12)
#define D3D10_SHADER_IEEE_STRICTNESS                (1 << 13)
#define D3D10_SHADER_WARNINGS_ARE_ERRORS            (1 << 18)


// optimization level flags
#define D3D10_SHADER_OPTIMIZATION_LEVEL0            (1 << 14)
#define D3D10_SHADER_OPTIMIZATION_LEVEL1            0
#define D3D10_SHADER_OPTIMIZATION_LEVEL2            ((1 << 14) | (1 << 15))
#define D3D10_SHADER_OPTIMIZATION_LEVEL3            (1 << 15)

enum D3DCOMPILER_STRIP_FLAGS
{
	D3DCOMPILER_STRIP_REFLECTION_DATA = 1,
	D3DCOMPILER_STRIP_DEBUG_INFO      = 2,
	D3DCOMPILER_STRIP_TEST_BLOBS      = 4,
	D3DCOMPILER_STRIP_FORCE_DWORD     = 0x7fffffff,
};


struct PEModule;
struct D3D11ShaderReflection;

struct D3D10_SHADER_MACRO {
	const char* Name;
	const char* Definition;
};

enum D3D10_INCLUDE_TYPE
{
	D3D10_INCLUDE_LOCAL,
	D3D10_INCLUDE_SYSTEM,
	// force 32-bit size enum
	D3D10_INCLUDE_FORCE_DWORD = 0x7fffffff
};

struct GUIDImpl;

struct D3D10Blob
{
	virtual HRESULT WINAPI_IMPL QueryInterface(const GUIDImpl& iid, void** ppv) = 0;
	virtual ULONG WINAPI_IMPL AddRef() = 0;
	virtual ULONG WINAPI_IMPL Release() = 0;
	virtual void* WINAPI_IMPL GetBufferPointer() = 0;
	virtual SIZE_T WINAPI_IMPL GetBufferSize() = 0;
};

struct D3D10Include
{
	virtual HRESULT WINAPI_IMPL Open(D3D10_INCLUDE_TYPE IncludeType, const char* pFileName, const void* pParentData, const void* *ppData, UINT *pBytes) = 0;
	virtual HRESULT WINAPI_IMPL Close(const void* pData) = 0;
};



class D3D11Compiler
{
public:
	void Initialize (const char* dllName);
	void Shutdown ();
	bool IsValid() const { return m_Exports.IsValid(); }
	void SetUseRealDLL (bool v) { m_UseRealDLL = v; }
	bool GetUseRealDLL () const { return m_UseRealDLL; }

	HRESULT D3DCompile (
						const void* pSrcData,
						unsigned long SrcDataSize,
						const char* pFileName,
						const D3D10_SHADER_MACRO* pDefines,
						D3D10Include* pInclude,
						const char* pEntrypoint,
						const char* pTarget,
						unsigned int Flags1,
						unsigned int Flags2,
						D3D10Blob** ppCode,
						D3D10Blob** ppErrorMsgs);

	HRESULT D3DDisassemble(
		const void* pSrcData,
		SIZE_T SrcDataSize,
		UINT flags,
		const char* szComments,
		D3D10Blob** ppDisassembly);

	HRESULT D3DReflect(
		const void* src,
		SIZE_T size,
		void** reflector);

	HRESULT D3DStripShader(const void* pShaderBytecode,
		SIZE_T BytecodeLength,
		UINT uStripFlags,
		D3D10Blob** ppStrippedBlob);

private:
	typedef HRESULT (WINAPI_IMPL *D3DCompileFunc)(
											 const void* pSrcData,
											 unsigned long SrcDataSize,
											 const char* pFileName,
											 const D3D10_SHADER_MACRO* pDefines,
											 D3D10Include* pInclude,
											 const char* pEntrypoint,
											 const char* pTarget,
											 unsigned int Flags1,
											 unsigned int Flags2,
											 D3D10Blob** ppCode,
											 D3D10Blob** ppErrorMsgs);
	typedef HRESULT (WINAPI_IMPL *D3DDisassembleFunc)(
		const void* pSrcData,
		SIZE_T SrcDataSize,
		UINT flags,
		const char* szComments,
		D3D10Blob** ppDisassembly);
	typedef HRESULT (WINAPI_IMPL *D3DReflectFunc)(
		const void* src,
		SIZE_T size,
		const GUIDImpl& iface,
		void** reflector);
	typedef HRESULT (WINAPI_IMPL *D3DStripShaderFunc)(const void* pShaderBytecode,
		SIZE_T BytecodeLength,
		UINT uStripFlags,
		D3D10Blob** ppStrippedBlob);

	struct Exports {
		Exports();
		bool IsValid() const { return compileFunc != 0 && disassembleFunc != 0 && reflectFunc != 0 && stripFunc != 0; }
		D3DCompileFunc		compileFunc;
		D3DDisassembleFunc	disassembleFunc;
		D3DReflectFunc		reflectFunc;
		D3DStripShaderFunc	stripFunc;
	};

	Exports& GetExports() {
		#if WODKA_WINDOWS
		if (m_UseRealDLL)
			return m_RealExports;
		#endif
		return m_Exports;
	}

	PEModule*	m_Dll;
	Exports		m_Exports;
	void*		m_IncludeOpenThunk;
	void*		m_IncludeCloseThunk;
	void*		m_IncludeOpenPatch;
	void*		m_IncludeClosePatch;

	#if WODKA_WINDOWS
	void*	m_RealDll;
	Exports	m_RealExports;
	#endif
	bool	m_UseRealDLL;
};


char* ReadFile (const char* filename, unsigned* outSize);
