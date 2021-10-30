#pragma once

#if UNITY_WIN

#if UNITY_WP8

#pragma message("WP8 TODO: implement")	// ?!-

#define kD3D11CompilerDLL "dummy.dll"	// ?!-

struct D3D11Compiler	// ?!-
{
public:
	void Initialize (const char* dllName) {}
	void Shutdown () {}
	bool IsValid() const { return false; }
};

#else

#if UNITY_WINRT
#include <d3dcompiler.h>
#else
#include "External/DirectX/builds/dx11include/d3d11.h"
#endif

#if UNITY_WINRT
#define kD3D11CompilerDLL "D3DCompiler_45.dll"
#else
#define kD3D11CompilerDLL "D3DCompiler_43.dll"
#endif

struct D3D11Compiler
{
public:
	#if UNITY_WINRT
	typedef HRESULT (WINAPI* D3DCompileFunc)(
		_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
		_In_ SIZE_T SrcDataSize,
		_In_opt_ LPCSTR pSourceName,
		_In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
		_In_opt_ ID3DInclude* pInclude,
		_In_ LPCSTR pEntrypoint,
		_In_ LPCSTR pTarget,
		_In_ UINT Flags1,
		_In_ UINT Flags2,
		_Out_ ID3DBlob** ppCode,
		_Out_opt_ ID3DBlob** ppErrorMsgs);

	typedef HRESULT (WINAPI* D3DStripShaderFunc)(
		_In_reads_bytes_(BytecodeLength) LPCVOID pShaderBytecode,
		_In_ SIZE_T BytecodeLength,
		_In_ UINT uStripFlags,
		_Out_ ID3DBlob** ppStrippedBlob);

	typedef HRESULT (WINAPI* D3DReflectFunc)(
		_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
		_In_ SIZE_T SrcDataSize,
		_In_ REFIID pInterface,
		_Out_ void** ppReflector);
	#else
	typedef HRESULT (WINAPI *D3DCompileFunc)(
		const void* pSrcData,
		unsigned long SrcDataSize,
		const char* pFileName,
		const D3D10_SHADER_MACRO* pDefines,
		ID3D10Include* pInclude,
		const char* pEntrypoint,
		const char* pTarget,
		UINT Flags1,
		UINT Flags2,
		ID3D10Blob** ppCode,
		ID3D10Blob** ppErrorMsgs);

	typedef HRESULT (WINAPI *D3DStripShaderFunc)(
		__in_bcount(BytecodeLength) const void* pShaderBytecode,
		__in unsigned long BytecodeLength,
		__in unsigned int uStripFlags,
		__out ID3D10Blob** ppStrippedBlob);

	typedef HRESULT (WINAPI *D3DReflectFunc)(
		__in_bcount(SrcDataSize) const void* pSrcData,
		__in unsigned long SrcDataSize,
		__in REFIID pInterface,
		__out void** ppReflector);
	#endif

	typedef HRESULT (WINAPI *D3DDisassembleFunc)(
		__in   const void* pSrcData,
		__in   SIZE_T SrcDataSize,
		__in   UINT Flags,
		__in   const char* szComments,
		__out  ID3D10Blob** ppDisassembly);

	typedef HRESULT (WINAPI *D3DCreateBlobFunc)(SIZE_T size, ID3D10Blob** blob);

public:
	void Initialize (const char* dllName);
	void Shutdown ();
	bool IsValid() const { return compileFunc && reflectFunc; }

public:
	#if !UNITY_WINRT
	HINSTANCE			dll;
	#endif
	D3DCompileFunc		compileFunc;
	D3DStripShaderFunc	stripShaderFunc;
	D3DReflectFunc		reflectFunc;
	D3DDisassembleFunc	disassembleFunc;
	D3DCreateBlobFunc	createBlobFunc;
};

extern GUID kIID_ID3D11ShaderReflection;

#endif

#endif // UNITY_WIN
