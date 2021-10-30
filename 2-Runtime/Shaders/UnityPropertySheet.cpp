#include "UnityPrefix.h"
#include "UnityPropertySheet.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/SLParserData.h"
#include "Runtime/Math/ColorSpaceConversion.h"

UnityPropertySheet::UnityTexEnv::UnityTexEnv()
{
	m_Scale = Vector2f(1,1);
	m_Offset = Vector2f(0,0);
	m_Texture = 0;
}


#if UNITY_EDITOR
void UnityPropertySheet::CullUnusedProperties (const ShaderLab::ParserShader* source)
{
	if (!source)
		return;
	
	TexEnvMap::iterator	curTex = m_TexEnvs.begin();
	TexEnvMap::iterator	texEnd = m_TexEnvs.end();
	
	while (curTex != texEnd)
	{
		const char*	propName = curTex->first.GetName(); 
		
		bool propFound = false;
		for (unsigned i = 0; i < source->m_PropInfo.m_Props.size(); ++i)
		{
			const ShaderLab::ParserProperty& srcProp = source->m_PropInfo.m_Props[i];
			if(srcProp.m_Type == ShaderLab::ParserProperty::kTexture 
				&& ::strcmp( srcProp.m_Name.c_str(), propName ) == 0 
			  )
			{
				propFound = true;
				break;
			}
		}
		
		if (propFound)
			++curTex;
		else
			m_TexEnvs.erase(curTex++);
		
	}
}
#endif

void UnityPropertySheet::AssignDefinedPropertiesTo (ShaderLab::PropertySheet &target)
{
	for (ShaderLab::PropertySheet::Floats::iterator i = target.GetFloatsMap().begin(); i != target.GetFloatsMap().end(); i++)
	{
		FloatMap::iterator j = m_Floats.find(i->first);
		if (j != m_Floats.end())
		{
			i->second = j->second;
		}
	}
	for (ShaderLab::PropertySheet::Vectors::iterator i = target.GetVectorMap().begin(); i != target.GetVectorMap().end(); i++)
	{
		ColorMap::iterator j = m_Colors.find(i->first);
		if (j != m_Colors.end())
		{
			if (target.GetColorTag(i->first))
				target.SetVector (i->first, GammaToActiveColorSpace(j->second).GetPtr());				
			else
				target.SetVector (i->first, j->second.GetPtr());
		}
	}
	for (ShaderLab::PropertySheet::TexEnvs::iterator i = target.GetTexEnvsMap().begin(); i != target.GetTexEnvsMap().end(); i++)
	{
		TexEnvMap::iterator j = m_TexEnvs.find(i->first);
		if (j != m_TexEnvs.end())
		{
			target.SetTextureWithPlacement( i->first, j->second.m_Texture, j->second.m_Scale, j->second.m_Offset );
		}
	}
}


bool UnityPropertySheet::AddNewShaderlabProps (const ShaderLab::PropertySheet &source)
{
	bool addedAny = false;
	for (ShaderLab::PropertySheet::Floats::const_iterator i = source.GetFloatsMap().begin(); i != source.GetFloatsMap().end(); i++)
	{
		if (m_Floats.insert( std::make_pair(i->first, i->second) ).second)
			addedAny = true;
	}
	for (ShaderLab::PropertySheet::Vectors::const_iterator i = source.GetVectorMap().begin(); i != source.GetVectorMap().end(); i++)
	{
		// skip texture _ST & _TexelSize properties
		bool isTextureProp = false;
		for (ShaderLab::PropertySheet::TexEnvs::const_iterator j = source.GetTexEnvsMap().begin(); j != source.GetTexEnvsMap().end(); ++j)
		{
			if (j->second.scaleOffsetValue == &i->second || j->second.texelSizeValue == &i->second)
			{
				isTextureProp = true;
				break;
			}
		}
		if (isTextureProp)
			continue;

		if (m_Colors.insert( std::make_pair(i->first, ColorRGBAf(i->second.x, i->second.y, i->second.z, i->second.w)) ).second)
			addedAny = true;
	}
	for (ShaderLab::PropertySheet::TexEnvs::const_iterator i = source.GetTexEnvsMap().begin(); i != source.GetTexEnvsMap().end(); i++)
	{
		if (m_TexEnvs.find (i->first) == m_TexEnvs.end())
		{
			UnityTexEnv ut;
			const Matrix4x4f& mat = i->second.texEnv->GetMatrix();
			ut.m_Scale.Set( mat.Get(0,0), mat.Get(1,1) );
			Vector3f pos = mat.GetPosition();
			ut.m_Offset = Vector2f (pos.x, pos.y);
			m_TexEnvs [i->first] = ut;
			addedAny = true;
		}
	}

	return addedAny;
}

void UnityPropertySheet::AddNewSerializedProps (const UnityPropertySheet &source)
{
	for (UnityPropertySheet::FloatMap::const_iterator i = source.m_Floats.begin(); i != source.m_Floats.end(); i++)
	{
		if (m_Floats.find (i->first) == m_Floats.end())
			m_Floats [i->first] = i->second;
	}
	for (UnityPropertySheet::ColorMap::const_iterator i = source.m_Colors.begin(); i != source.m_Colors.end(); i++)
	{
		if (m_Colors.find (i->first) == m_Colors.end())
			m_Colors [i->first] = i->second;
	}
	for (UnityPropertySheet::TexEnvMap::const_iterator i = source.m_TexEnvs.begin(); i != source.m_TexEnvs.end(); i++)
	{
		if (m_TexEnvs.find (i->first) == m_TexEnvs.end())
			m_TexEnvs [i->first] = i->second;
	}
}
