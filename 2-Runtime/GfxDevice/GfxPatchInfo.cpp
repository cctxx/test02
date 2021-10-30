#include "UnityPrefix.h"
#include "GfxPatchInfo.h"
#include "External/shaderlab/Library/shaderlab.h"


void GfxPatchInfo::Reset()
{
	for (int pt = 0; pt < GfxPatch::kTypeCount; pt++)
		m_Patches[pt].resize_uninitialized(0);
	m_TexEnvPatches.resize_uninitialized(0);
}

void GfxPatchInfo::AddPatchableFloat(const ShaderLab::FloatVal& val, float& dest, const void* bufferStart,  
	const ShaderLab::PropertySheet* props)
{
	if (val.var.IsValid())
	{
		using namespace ShaderLab::shaderprops;
		PropertyLocation location;
		bool missing;
		const float& src = GetFloat(props, val.var, location);
		dest = src;
		if (IsPatchable(location, missing))
		{
			size_t patchOffset = reinterpret_cast<UInt8*>(&dest) - static_cast<const UInt8*>(bufferStart);
			AddPatch(GfxPatch::kTypeFloat, GfxPatch(val.var, missing ? NULL : &src, patchOffset));
		}
	}
	else
		dest = val.val;
}

void GfxPatchInfo::AddPatchableVector(const ShaderLab::VectorVal& val, Vector4f& dest, const void* bufferStart,  
	const ShaderLab::PropertySheet* props)
{
	if (val.var.IsValid())
	{
		using namespace ShaderLab::shaderprops;
		PropertyLocation location;
		bool missing;
		const Vector4f& src = GetVector(props, val.var, location);
		dest = src;
		if (IsPatchable(location, missing))
		{
			size_t patchOffset = reinterpret_cast<UInt8*>(&dest) - static_cast<const UInt8*>(bufferStart);
			AddPatch(GfxPatch::kTypeVector, GfxPatch(val.var, missing ? NULL : &src, patchOffset));
		}
	}
	else
	{
		AddPatchableFloat(val.x, dest.x, bufferStart, props);
		AddPatchableFloat(val.y, dest.y, bufferStart, props);
		AddPatchableFloat(val.z, dest.z, bufferStart, props);
		AddPatchableFloat(val.w, dest.w, bufferStart, props);
	}
}

bool GfxPatchInfo::AddPatchableTexEnv(const ShaderLab::FastPropertyName& name, const ShaderLab::FastPropertyName& matrixName,
	TextureDimension dim, TexEnvData* dest, const void* bufferStart, const ShaderLab::PropertySheet* props)
{
	using ShaderLab::TexEnv;	
	using namespace ShaderLab::shaderprops;	

	// Get TexEnv and prepare data
	PropertyLocation texEnvLocation;
	TexEnv* texEnv = GetTexEnv(props, name, dim, texEnvLocation);
	Assert(texEnv != NULL);
	texEnv->PrepareData(name.index, matrixName, props, dest);

	bool missing;
	bool patchable = IsPatchable(texEnvLocation, missing);
	if (!missing)
	{
		UInt32 patchFlags = 0;
		if (patchable)
			patchFlags |= GfxTexEnvPatch::kPatchProperties;

		if (matrixName.IsValid() || texEnv->GetMatrixName().IsValid())
		{
			ShaderLab::FastPropertyName finalMatrixName = matrixName.IsValid() ? matrixName : texEnv->GetMatrixName();
			PropertyLocation matLocation;
			int size;
			GetValueProp (props, finalMatrixName, 16, &size, matLocation);
			bool matMissing;
			if (IsPatchable(matLocation, matMissing))
			{
				patchFlags |= GfxTexEnvPatch::kPatchMatrix;
			}
		}
		if (patchFlags != 0)
		{
			size_t offset = reinterpret_cast<const UInt8*>(dest) - static_cast<const UInt8*>(bufferStart);
			GfxTexEnvPatch patch(name, matrixName, texEnv, dim, offset, patchFlags);
			AddTexEnvPatch(patch);
		}
		return true;
	}
	return false;
}
