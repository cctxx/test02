#pragma once

#include <string>
#include "External/shaderlab/Library/FastPropertyName.h"
#include "GfxDeviceTypes.h"
#include "External/shaderlab/Library/shadertypes.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/vector_set.h"
#include "BuiltinShaderParams.h"
#include "GfxDeviceConfigure.h"
#include "Runtime/GfxDevice/threaded/ClientIDMapper.h"

class ShaderErrors;
class GpuProgram;
class GpuProgramParameters;
class CreateGpuProgramOutput;
class GfxPatchInfo;
class ChannelAssigns;
using ShaderLab::FastPropertyName;
namespace ShaderLab { class PropertySheet; class SubProgram; }


enum GpuProgramLevel {
	kGpuProgramNone = 0, // fixed function
	kGpuProgramSM1,
	kGpuProgramSM2,
	kGpuProgramSM3,
	kGpuProgramSM4,
	kGpuProgramSM5,
	kGpuProgramCount // keep this last
};


struct PropertyNamesSet {
	PropertyNamesSet() : valueSize(0) { }
	vector_set<int> names; // IDs of shader property names
	UInt32 valueSize; // Buffer size needed to hold all property values of the above
};


// ------------------------------------------------------------------------

bool CheckGpuProgramUsable (const char* str);

// Create GPU programs with this function. This can return NULL if the program is not
// recognized. When done with a program call Release() on it, never delete directly.
GpuProgram* CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output );


// ------------------------------------------------------------------------

class GpuProgramParameters {
public:
	struct ValueParameter {
		FastPropertyName m_Name;	// The name of the value to look up.
		int m_Index;				// The index of the first parameter to upload to
		int m_ArraySize;			// The index of the first parameter to upload to
		ShaderParamType m_Type;
		UInt8 m_RowCount;
		UInt8 m_ColCount;
		ValueParameter (FastPropertyName n, ShaderParamType type, int idx, int arraySize, int rowCount, int colCount) : m_Name(n), m_Index(idx), m_ArraySize(arraySize), m_Type(type), m_RowCount(rowCount), m_ColCount(colCount) {}
		ValueParameter () : m_Index(0), m_ArraySize(0), m_RowCount(0), m_ColCount(0) {}
		bool operator == (const ValueParameter& other) const { return m_Name.index == other.m_Name.index; }
		bool operator < (const ValueParameter& other) const { return m_Name.index < other.m_Name.index; }
	};
	typedef dynamic_array<ValueParameter> ValueParameterArray;
	
	struct TextureParameter {
		FastPropertyName m_Name;
		int	m_Index;
		int m_SamplerIndex;
		TextureDimension m_Dim;
		TextureParameter (FastPropertyName n, int idx, int samplerIdx, TextureDimension dim) : m_Name(n), m_Index(idx), m_SamplerIndex(samplerIdx), m_Dim(dim) { }
		TextureParameter () : m_Index(0), m_SamplerIndex(0), m_Dim(kTexDimNone) {}
	};
	typedef std::vector<TextureParameter> TextureParameterList;

	struct BufferParameter {
		FastPropertyName m_Name;
		int m_Index;
		BufferParameter (FastPropertyName n, int idx) : m_Name(n), m_Index(idx) { }
		BufferParameter () : m_Index(0) {}
	};
	typedef dynamic_array<BufferParameter> BufferParameterArray;
	

	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	struct ConstantBuffer {
		FastPropertyName	m_Name;
		ValueParameterArray	m_ValueParams;
		int					m_Size;
		int					m_BindIndex;
	};
	typedef std::vector<ConstantBuffer> ConstantBufferList;
	#endif
	
public:
	GpuProgramParameters () : m_ValuesSize(0), m_Status(kBlank)
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
			,m_InternalHandle(0)
#endif
	{ }

	void AddValueParam (const ValueParameter& param);
	void AddTextureParam (const TextureParameter& param);
	void AddVectorParam (int index, ShaderParamType type, int dimension, const char* nameStr, int cbIndex, PropertyNamesSet* outNames);
	void AddMatrixParam (int index, const char* nameStr, int rowCount, int colCount, int cbIndex, PropertyNamesSet* outNames);
	void AddTextureParam (int index, int samplerIndex, const char* nameStr, TextureDimension dim, PropertyNamesSet* outNames);
	void AddBufferParam (int index, const char* nameStr, PropertyNamesSet* outNames);
	const ValueParameterArray&  GetValueParams() const;
	const TextureParameterList& GetTextureParams() const { return m_TextureParams; }
	TextureParameterList& GetTextureParams() { return m_TextureParams; }
	const BufferParameterArray& GetBufferParams() const { return m_BufferParams; }

	const ValueParameter* FindParam(const FastPropertyName& name, int* outCBIndex = NULL) const;
	const TextureParameter* FindTextureParam(const FastPropertyName& name, TextureDimension dim) const;

	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	const ConstantBufferList& GetConstantBuffers() const { return m_ConstantBuffers; }
	ConstantBufferList& GetConstantBuffers() { return m_ConstantBuffers; }
	void SetConstantBufferCount (size_t count)
	{
		m_ConstantBuffers.resize (count);
	}
	#endif

	const BuiltinShaderParamIndices& GetBuiltinParams() const { return m_BuiltinParams; }

	int GetValuesSize() const { return m_ValuesSize; }
	UInt8* PrepareValues (
		const ShaderLab::PropertySheet* props,
		UInt8* buffer,
		const UInt8* bufferStart = NULL,
		GfxPatchInfo* outPatchInfo = NULL,
		bool* outMissingTextures = NULL) const;

	bool IsReady() const { return m_Status == kReady; }
	bool IsDirty() const { return m_Status == kDirty; }

	virtual void MakeReady();

#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	// Reference to the created GpuPogramParameters on the worker.
	ClientIDWrapper(GpuProgramParameters) m_InternalHandle;
#endif
			
private:
	enum State {
		kBlank,
		kDirty,
		kReady
	};

	struct NameToValueIndex {
		int nameIndex;
		SInt16	cbIndex;
		UInt16	valueIndex;
		bool operator == (const NameToValueIndex& other) const { return nameIndex == other.nameIndex; }
		bool operator < (const NameToValueIndex& other) const { return nameIndex < other.nameIndex; }
	};

	void MakeValueParamsReady (ValueParameterArray& values, int cbIndex);
	ValueParameterArray& GetValuesArray (int cbIndex) {
		ValueParameterArray* values = &m_ValueParams;
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		if (cbIndex >= 0)
		{
			DebugAssert (cbIndex < m_ConstantBuffers.size());
			values = &m_ConstantBuffers[cbIndex].m_ValueParams;
		}
		#endif
		return *values;
	}
	const ValueParameterArray& GetValuesArray (int cbIndex) const {
		const ValueParameterArray* values = &m_ValueParams;
		#if GFX_SUPPORTS_D3D11
		if (cbIndex >= 0)
		{
			DebugAssert (cbIndex < m_ConstantBuffers.size());
			values = &m_ConstantBuffers[cbIndex].m_ValueParams;
		}
		#endif
		return *values;
	}

private:

	typedef dynamic_array<NameToValueIndex> NameToValueIndexMap;
	ValueParameterArray		m_ValueParams;
	NameToValueIndexMap		m_NamedParams;
	TextureParameterList	m_TextureParams;
	BufferParameterArray	m_BufferParams;

	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	ConstantBufferList			m_ConstantBuffers;
	#endif

	BuiltinShaderParamIndices	m_BuiltinParams;
	int		m_ValuesSize;
	State	m_Status;
};


// ------------------------------------------------------------------------


// Base class for GPU programs.
class GpuProgram {
public:
	virtual ~GpuProgram();
	
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer) = 0;
	
	ShaderImplType GetImplType() const { return m_ImplType; }
	virtual bool IsSupported () const;
	GpuProgramLevel GetLevel() const { return m_GpuProgramLevel; }	
	
	void SetNotSupported (bool v) { m_NotSupported = v; }

protected:
	GpuProgram();
	
protected:
	ShaderImplType	m_ImplType; // Actual implementation type
	bool m_NotSupported;
	bool m_WasDestroyed;
	GpuProgramLevel	m_GpuProgramLevel;
};


class CreateGpuProgramOutput
{
public:
	CreateGpuProgramOutput();
	~CreateGpuProgramOutput();

	bool GetPerFogModeParamsEnabled() const			{ return m_PerFogModeParamsEnabled; }
	void SetPerFogModeParamsEnabled(bool enable)	{ m_PerFogModeParamsEnabled = enable; }

	const GpuProgramParameters* GetParams() const	{ return m_Params; }
	GpuProgramParameters& CreateParams();

	const ShaderErrors* GetShaderErrors() const		{ return m_ShaderErrors; }
	ShaderErrors& CreateShaderErrors();

	PropertyNamesSet* GetOutNames() const		{ return m_OutNames; }
	const void SetOutNames(PropertyNamesSet* value) {  m_OutNames = value; }

	const ChannelAssigns* GetChannelAssigns() const	{ return m_ChannelAssigns; }
	ChannelAssigns& CreateChannelAssigns();

private:
	bool m_PerFogModeParamsEnabled;
	GpuProgramParameters* m_Params;
	ShaderErrors* m_ShaderErrors;
	ChannelAssigns* m_ChannelAssigns;
	PropertyNamesSet* m_OutNames;
};

#if GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30

typedef unsigned int GLShaderID;

class GpuProgramGL : public GpuProgram {
public:
	virtual ~GpuProgramGL();

	GLShaderID	GetGLProgramIfCreated(FogMode fog) const	{ return m_Programs[fog]; }

protected:
	GpuProgramGL();

protected:
	std::string		m_SourceForFog; // original source, used for fog patching if needed
	GLShaderID		m_Programs[kFogModeCount];
};

#endif
