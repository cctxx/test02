// Written by Aras Pranckevicius
// I hereby place this code in the public domain

#include "D3D11Compiler.h"
#include "External/Wodka/wodka_PELoader.h"
#include "External/Wodka/wodka_KnownImports.h"
#include <stdio.h>
#include <stdlib.h>
#if WODKA_WINDOWS
#include <windows.h>
#endif

struct GUIDImpl {
	DWORD Data1;
	WORD  Data2;
	WORD  Data3;
	BYTE  Data4[8];
};

#if WODKA_USE_D3DCOMPILER_46 || WODKA_USE_D3DCOMPILER_47
// matches D3DCompiler_46.dll
static GUIDImpl kIID_ID3D11ShaderReflection = {0x8d536ca1, 0x0cca, 0x4956, {0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84}};
#else
// matches D3DCompiler_43.dll
static GUIDImpl kIID_ID3D11ShaderReflection = {0x0a233719, 0x3960, 0x4578, {0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1}};
#endif


char* ReadFile (const char* filename, unsigned* outSize)
{
	FILE* f = fopen(filename, "rb");
	if (!f)
		return NULL;
	fseek (f, 0, SEEK_END);
	size_t size = ftell (f);
	fseek (f, 0, SEEK_SET);

	char* buf = (char*)malloc (size);
	if (!buf)
		return NULL;

	size_t res = fread (buf, 1, size, f);
	if (res != size)
	{
		free (buf);
		return NULL;
	}

	fclose (f);
	*outSize = size;
	return buf;
}





D3D11Compiler::Exports::Exports()
{
	memset (this, 0, sizeof(*this));
}



void D3D11Compiler::Initialize (const char* dllName)
{
	m_UseRealDLL = false;

	m_Exports = Exports();
	m_IncludeOpenThunk = NULL;
	m_IncludeCloseThunk = NULL;
	m_IncludeOpenPatch = NULL;
	m_IncludeClosePatch = NULL;

	unsigned dllSize;
	char* dllData = ReadFile (dllName, &dllSize);
	if (!dllData)
		return;

	PESetupFS();
	InitializeWodkaImports();
	m_Dll = PELoadLibrary (dllData, dllSize, kWodkaKnownImports, kWodkaKnownImportsCount);
	free (dllData);

	if (m_Dll)
	{
		m_Exports.compileFunc = (D3DCompileFunc)PEGetProcAddress (m_Dll, "D3DCompile");
		m_Exports.disassembleFunc = (D3DDisassembleFunc)PEGetProcAddress (m_Dll, "D3DDisassemble");
		m_Exports.reflectFunc = (D3DReflectFunc)PEGetProcAddress (m_Dll, "D3DReflect");
		m_Exports.stripFunc = (D3DStripShaderFunc)PEGetProcAddress (m_Dll, "D3DStripShader");

		m_IncludeOpenThunk = PECreateCallThunk (m_Dll, NULL, 24, &m_IncludeOpenPatch);
		m_IncludeCloseThunk = PECreateCallThunk (m_Dll, NULL, 8, &m_IncludeClosePatch);
	}

#if WODKA_WINDOWS
	HMODULE m = LoadLibraryA (dllName);
	m_RealDll = m;
	if (m_RealDll)
	{
		m_RealExports.compileFunc = (D3DCompileFunc)GetProcAddress (m, "D3DCompile");
		m_RealExports.disassembleFunc = (D3DDisassembleFunc)GetProcAddress (m, "D3DDisassemble");
		m_RealExports.reflectFunc = (D3DReflectFunc)GetProcAddress (m, "D3DReflect");
		m_RealExports.stripFunc = (D3DStripShaderFunc)GetProcAddress (m, "D3DStripShader");
	}
#endif
}


void D3D11Compiler::Shutdown ()
{
	if (m_Dll)
	{
		PEFreeLibrary (m_Dll);
		m_Dll = NULL;
	}
}


HRESULT D3D11Compiler::D3DCompile (
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
	D3D10Blob** ppErrorMsgs)
{
	Exports& exports = GetExports();
	if (!exports.IsValid())
		return -1;
	PESetupFS();

	size_t size = 0;


	#if WODKA_USE_ALIGN_THUNKS
	// Include handler code is something called from inside of the HLSL compiler,
	// so we have to thunk it to get stack alignment if needed.
	void* newVtable[] = {
		m_IncludeOpenThunk,
		m_IncludeCloseThunk,
	};
	size_t** includeVTable = NULL;
	size_t* savedVTable = NULL;
	if (pInclude && m_IncludeOpenThunk && m_IncludeCloseThunk)
	{
		includeVTable = (size_t**)&*pInclude;
		savedVTable = *includeVTable;

		// patch relative destination addresses in our thunks
		size_t relAddr;
		relAddr = savedVTable[0] - ((size_t)m_IncludeOpenPatch + sizeof(size_t));
		*(size_t*)m_IncludeOpenPatch = relAddr;
		relAddr = savedVTable[1] - ((size_t)m_IncludeClosePatch + sizeof(size_t));
		*(size_t*)m_IncludeClosePatch = relAddr;

		// use new vtable in the passed object
		*includeVTable = (size_t*)newVtable;
	}
	#endif

	HRESULT hr = exports.compileFunc (pSrcData, SrcDataSize, pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
	if (ppCode && *ppCode)
		size = (*ppCode)->GetBufferSize();

	#if WODKA_USE_ALIGN_THUNKS
	if (savedVTable)
	{
		// restore original vtable in the passed object
		*includeVTable = savedVTable;
	}
	#endif

	return hr;
}

HRESULT D3D11Compiler::D3DDisassemble(
   const void* pSrcData,
   SIZE_T SrcDataSize,
   UINT flags,
   const char* szComments,
   D3D10Blob** ppDisassembly)
{
	Exports& exports = GetExports();
	if (!exports.IsValid())
		return -1;
	PESetupFS();

	HRESULT hr = exports.disassembleFunc (pSrcData, SrcDataSize, flags, szComments, ppDisassembly);
	return hr;
}

HRESULT D3D11Compiler::D3DReflect(
	const void* src,
	SIZE_T size,
	void** reflector)
{
	Exports& exports = GetExports();
	if (!exports.IsValid())
		return -1;
	PESetupFS();

	HRESULT hr = exports.reflectFunc (src, size, kIID_ID3D11ShaderReflection, reflector);
	return hr;
}


HRESULT D3D11Compiler::D3DStripShader(const void* pShaderBytecode,
					   SIZE_T BytecodeLength,
					   UINT uStripFlags,
					   D3D10Blob** ppStrippedBlob)
{
	Exports& exports = GetExports();
	if (!exports.IsValid())
		return -1;
	PESetupFS();

	HRESULT hr = exports.stripFunc (pShaderBytecode, BytecodeLength, uStripFlags, ppStrippedBlob);
	return hr;
}
