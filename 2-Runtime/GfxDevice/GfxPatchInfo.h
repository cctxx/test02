#pragma once

#include "GfxDeviceTypes.h"
#include "External/shaderlab/Library/shadertypes.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Utilities/dynamic_array.h"

struct GfxPatch
{
	GfxPatch(const ShaderLab::FastPropertyName& name, const void* src, size_t ofs)
		: nameIndex(name.index), source(src), patchOffset(ofs) {}

	enum Type
	{
		kTypeFloat,
		kTypeVector,
		kTypeMatrix,
		kTypeBuffer,
		kTypeCount
	};

	int nameIndex;
	const void* source;
	size_t patchOffset;
};

struct GfxTexEnvPatch
{
	GfxTexEnvPatch(const ShaderLab::FastPropertyName& name, const ShaderLab::FastPropertyName& matName,
			ShaderLab::TexEnv* tex, TextureDimension dim, size_t ofs, UInt32 flags)
		: nameIndex(name.index), matrixName(matName), texEnv(tex), texDim(dim), patchOffset(ofs), patchFlags(flags) {}

	enum PatchFlag
	{
		kPatchProperties	= 1 << 0,
		kPatchMatrix		= 1 << 1
	};

	//ShaderLab::FastPropertyName textureName;
	int nameIndex;
	ShaderLab::FastPropertyName matrixName;
	ShaderLab::TexEnv* texEnv;
	TextureDimension texDim;
	size_t patchOffset;
	UInt32 patchFlags;
};

class GfxPatchInfo
{
public:
	FORCE_INLINE size_t				GetPatchCount(GfxPatch::Type type) const			{ return m_Patches[type].size(); }
	FORCE_INLINE const GfxPatch&	GetPatch(GfxPatch::Type type, int index) const		{ return m_Patches[type][index]; }
	FORCE_INLINE GfxPatch&			GetPatch(GfxPatch::Type type, int index)			{ return m_Patches[type][index]; }
	FORCE_INLINE void				AddPatch(GfxPatch::Type type, const GfxPatch& p)	{ m_Patches[type].push_back(p); }

	FORCE_INLINE size_t					GetTexEnvPatchCount() const						{ return m_TexEnvPatches.size(); }
	FORCE_INLINE const GfxTexEnvPatch&	GetTexEnvPatch(int index) const					{ return m_TexEnvPatches[index]; }
	FORCE_INLINE GfxTexEnvPatch&		GetTexEnvPatch(int index)						{ return m_TexEnvPatches[index]; }
	FORCE_INLINE void					AddTexEnvPatch(const GfxTexEnvPatch& p)			{ m_TexEnvPatches.push_back(p); }

	void Reset();

	void AddPatchableFloat(const ShaderLab::FloatVal& val, float& dest, const void* bufferStart,  
		const ShaderLab::PropertySheet* props);

	void AddPatchableVector(const ShaderLab::VectorVal& val, Vector4f& dest, const void* bufferStart,  
		const ShaderLab::PropertySheet* props);

	bool AddPatchableTexEnv(const ShaderLab::FastPropertyName& name, const ShaderLab::FastPropertyName& matrixName,
		TextureDimension dim, TexEnvData* dest, const void* bufferStart, const ShaderLab::PropertySheet* props);

private:
	typedef dynamic_array<GfxPatch> PatchArray;
	typedef dynamic_array<GfxTexEnvPatch> TexEnvPatchArray;
	PatchArray m_Patches[GfxPatch::kTypeCount];
	TexEnvPatchArray m_TexEnvPatches;
};

