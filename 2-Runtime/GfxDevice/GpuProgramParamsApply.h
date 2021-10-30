#pragma once


template<typename SetValuesFunctor>
void ApplyMaterialPropertyBlockValues(
	MaterialPropertyBlock& propblock,
	GpuProgram* activeGpuProgram[kShaderTypeCount],
	const GpuProgramParameters* activeGpuProgramParams[kShaderTypeCount],
	SetValuesFunctor& functor)
{
	const MaterialPropertyBlock::Property* curProp = propblock.GetPropertiesBegin();
	const MaterialPropertyBlock::Property* propEnd = propblock.GetPropertiesEnd();
	const float* propBuffer = propblock.GetBufferBegin();
	while (curProp != propEnd)
	{
		FastPropertyName name;
		name.index = curProp->nameIndex;
		for (int shaderType = kShaderVertex; shaderType < kShaderTypeCount; ++shaderType)
		{
			GpuProgram* GpuProgram = activeGpuProgram[shaderType];
			if (!GpuProgram)
				continue;
			const GpuProgramParameters* params = activeGpuProgramParams[shaderType];

			if (curProp->texDim != kTexDimNone)
			{
				// texture parameter
				const GpuProgramParameters::TextureParameter* param = params->FindTextureParam(name, (TextureDimension)curProp->texDim);
				if (!param)
					continue;
				const float* val = &propBuffer[curProp->offset];
				functor.SetTextureVal ((ShaderType)shaderType, param->m_Index, param->m_SamplerIndex, param->m_Dim, TextureID(*(unsigned int*)val));
			}
			else
			{
				// value parameter
				int cbIndex;
				const GpuProgramParameters::ValueParameter* param = params->FindParam(name, &cbIndex);
				if (!param)
					continue;
				if (curProp->rows == 1)
				{
					const float* src = &propBuffer[curProp->offset];
					Vector4f val;
					if (curProp->cols == 4)
						val = Vector4f(src);
					else
					{
						DebugAssert(curProp->cols == 1);
						val = Vector4f(*src, 0, 0, 0);
					}
					functor.SetVectorVal ((ShaderType)shaderType, param->m_Type, param->m_Index, val.GetPtr(), curProp->cols, *params, cbIndex);
				}
				else if (curProp->rows == 4)
				{
					DebugAssert(curProp->cols == 4);
					const Matrix4x4f* src = (const Matrix4x4f*)&propBuffer[curProp->offset];
					functor.SetMatrixVal ((ShaderType)shaderType, param->m_Index, src, param->m_RowCount, *params, cbIndex);
				}
				else
				{
					AssertString("Unknown property dimensions");
				}
			}
		}
		++curProp;
	}

	propblock.Clear();
}


// GL ES is different from everyone else, in that shader variables are always for the whole
// "program" (all shader stages at once).
template<typename SetValuesFunctor>
void ApplyMaterialPropertyBlockValuesES(
	MaterialPropertyBlock& propblock,
	const GpuProgram* activeProgram,
	const GpuProgramParameters* activeProgramParams,
	SetValuesFunctor& functor)
{
	if (activeProgram)
	{
		const MaterialPropertyBlock::Property* curProp = propblock.GetPropertiesBegin();
		const MaterialPropertyBlock::Property* propEnd = propblock.GetPropertiesEnd();
		const float* propBuffer = propblock.GetBufferBegin();
		while (curProp != propEnd)
		{
			FastPropertyName name;
			name.index = curProp->nameIndex;

			if (curProp->texDim != kTexDimNone)
			{
				// texture parameter
				const GpuProgramParameters::TextureParameter* param = activeProgramParams->FindTextureParam(name, (TextureDimension)curProp->texDim);
				if (param)
				{
					const float* val = &propBuffer[curProp->offset];
					functor.SetTextureVal (kShaderFragment, param->m_Index, param->m_SamplerIndex, param->m_Dim, TextureID(*(unsigned int*)val));
				}
			}
			else
			{
				// value parameter
				const GpuProgramParameters::ValueParameter* param = activeProgramParams->FindParam(name);
				if (param && curProp->rows == param->m_RowCount)
				{
					if (curProp->rows == 1)
					{
						const float* src = &propBuffer[curProp->offset];
						functor.SetVectorVal (param->m_Type, param->m_Index, src, param->m_ColCount);
					}
					else if (curProp->rows == 4)
					{
						DebugAssert(curProp->cols == 4);
						const Matrix4x4f* src = (const Matrix4x4f*)&propBuffer[curProp->offset];
						functor.SetMatrixVal (param->m_Index, src, param->m_RowCount);
					}
					else
					{
						AssertString("Unknown property dimensions");
					}
				}
			}
			++curProp;
		}
	}

	propblock.Clear();
}
