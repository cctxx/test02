#include "UnityPrefix.h"
#include "BaseRenderer.h"
#include "Runtime/Shaders/Material.h"
#include "UnityScene.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/shaderlab/Library/subshader.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/MurmurHash/MurmurHash2.h"

using namespace Unity;


BaseRenderer::BaseRenderer (RendererType type)
:	m_RendererType(type)
,	m_LightmapST(1.0f,1.0f,0.0f,0.0f)
,	m_LightmapIndex (0xFF)
,	m_CastShadows(true)
,	m_ReceiveShadows(true)
,	m_IsVisibleInScene(false)
,	m_CustomProperties(NULL)
,	m_CustomPropertiesHash(0)
,	m_TransformDirty(true)
,	m_BoundsDirty(true)
,	m_GlobalLayeringData(GlobalLayeringDataCleared())
{
	Assert(type <= 0xFF);
	#if UNITY_EDITOR
	m_ScaleInLightmap = 1.0f;
	#endif
}

BaseRenderer::~BaseRenderer ()
{
}

void BaseRenderer::GetLocalAABB (AABB& aabb)
{
	GetTransformInfo (); // updates if needed
	aabb = m_TransformInfo.localAABB;
}

void BaseRenderer::GetWorldAABB (AABB& aabb)
{
	GetTransformInfo (); // updates if needed
	aabb = m_TransformInfo.worldAABB;
}


int BaseRenderer::GetLightmapIndexInt() const
{
	if (m_LightmapIndex == 0xFF)
		return -1;
	else
		return m_LightmapIndex;
}

void BaseRenderer::SetLightmapIndexIntNoDirty(int index)
{
	if (index == -1)
		m_LightmapIndex = 0xFF;
	else if (index < 0 || index > 0xFF)
	{
		m_LightmapIndex = 0xFF;
		ErrorString("Lightmap index must be less than 256");
	}
	else
		m_LightmapIndex = index;
}

// Treats objects that actually _use_ lightmaps as lightmapped.
bool BaseRenderer::IsLightmappedForRendering() const
{
	// Special indices:
	// 0xFF: object does not use lightmaps
	// 0xFE: object only influences lightmaps, but does not use them itself

	return m_LightmapIndex != 0xFF && m_LightmapIndex != 0xFE;
}

// Treats objects that _influence_ lightmaps as lightmapped.
bool BaseRenderer::IsLightmappedForShadows() const
{
	return m_LightmapIndex != 0xFF;
}

bool operator == (const TransformInfo& a, const TransformInfo& b){
	return a.invScale == b.invScale
		&& a.localAABB.GetCenter() == b.localAABB.GetCenter()
		&& a.localAABB.GetExtent() == b.localAABB.GetExtent()
		&& a.transformType == b.transformType
		&& a.worldAABB.GetCenter() == b.worldAABB.GetCenter()
		&& a.worldAABB.GetExtent() == b.worldAABB.GetExtent()
		&& a.worldMatrix.GetAxisX() == b.worldMatrix.GetAxisX();
}


void BaseRenderer::ComputeCustomPropertiesHash()
{
	if (m_CustomProperties)
	{
		const float* buf = m_CustomProperties->GetBufferBegin();
		const float* bufEnd = m_CustomProperties->GetBufferEnd();
		m_CustomPropertiesHash = MurmurHash2A (buf, (const UInt8*)bufEnd - (const UInt8*)buf, 0x9747b28c);
	}
	else
	{
		m_CustomPropertiesHash = 0;
	}
}


void BaseRenderer::ApplyCustomProperties (Unity::Material& mat, Shader* shader, int subshaderIndex) const
{
	if (!m_CustomProperties)
		return;

	// Hopefully most of per-instance custom properties only go into shader constants; those
	// are applied later on in BeforeDrawCall. This is a fast path since it does not involve changing
	// the material. Some properties however might affect fixed function state (alpha test reference, texture
	// combiner colors, fixed function material etc.), those need to be applied here.

	const dynamic_array<int>& fixedFunctionProps = shader->GetShaderLabShader()->GetSubShader(subshaderIndex).GetPropsAffectingFF();
	if (fixedFunctionProps.empty())
		return; // no props that affect fixed function state, great!

	//@TODO: slow implementation for now just to get this working properly!
	const MaterialPropertyBlock::Property* curProp = m_CustomProperties->GetPropertiesBegin();
	const MaterialPropertyBlock::Property* propEnd = m_CustomProperties->GetPropertiesEnd();
	const float* propBuffer = m_CustomProperties->GetBufferBegin();
	for (; curProp != propEnd; ++curProp)
	{
		if (std::find(fixedFunctionProps.begin(), fixedFunctionProps.end(), curProp->nameIndex) == fixedFunctionProps.end())
			continue; // this property does not affect fixed function state

		ShaderLab::FastPropertyName name;
		name.index = curProp->nameIndex;
		const float* src = &propBuffer[curProp->offset];
		if (curProp->rows == 1 && curProp->cols == 1)
		{
			mat.SetFloat (name, *src);
		}
		else if (curProp->rows == 1 && curProp->cols == 4)
		{
			mat.SetColor (name, ColorRGBAf(src));
		}
		else if (curProp->rows == 4 && curProp->cols == 4)
		{
			mat.SetMatrix (name, Matrix4x4f(src));
		}
		else
		{
			AssertString ("Unknown property dimensions");
		}
	}
}
