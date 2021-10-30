#include "UnityPrefix.h"
#include "VertexPipeD3D9.h"
#include "ShaderGenerator.h"
#include "D3D9Utils.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "External/DirectX/builds/dx9include/d3dx9.h"
#include <map>



#define PRINT_VERTEX_PIPE_STATS 0

#define PRINT_AMD_SHADER_ANALYZER_OUTPUT 0




// GpuProgramsD3D.cpp
ID3DXBuffer* AssembleD3DShader( const std::string& source );


#if PRINT_AMD_SHADER_ANALYZER_OUTPUT
void PrintAMDShaderAnalyzer( const std::string& source )
{
	const char* kPath = "C:\\Program Files\\AMD\\GPU ShaderAnalyzer 1.45\\GPUShaderAnalyzer.exe";
	const char* kInputPath = "ShaderInput.txt";
	const char* kOutputPath = "ShaderOutput.txt";
	DeleteFileA(kInputPath);
	DeleteFileA(kOutputPath);
	FILE* fout = fopen(kInputPath, "wt");
	fwrite(source.c_str(), source.size(), 1, fout);
	fclose(fout);

	std::string commandLine = std::string(kPath) + " " + kInputPath + " -Analyze " + kOutputPath + " -Module Latest -ASIC HD3870";

	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory( &pi, sizeof(pi) );

	if( CreateProcessA(
		NULL,					// name of executable module
		(char*)commandLine.c_str(),	// command line string
		NULL,					// process attributes
		NULL,					// thread attributes
		FALSE,					// handle inheritance option
		0,						// creation flags
		NULL,					// new environment block
		NULL,					// current directory name
		&si,					// startup information
		&pi ) )					// process information
	{
		WaitForSingleObject( pi.hProcess, INFINITE );
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );

		FILE* fin = fopen(kOutputPath, "rt");
		if( fin ) {
			fseek(fin, 0, SEEK_END);
			int length = ftell(fin);
			fseek(fin, 0, SEEK_SET);
			char* buffer = new char[length+1];
			memset(buffer, 0,length+1);
			fread(buffer, length, 1, fin);
			fclose(fin);
		}
	}
	//DeleteFileA(kInputPath);
	//DeleteFileA(kOutputPath);
}
#endif



static inline D3DCOLOR ColorToD3D( const float color[4] )
{
	return D3DCOLOR_RGBA( NormalizedToByte(color[0]), NormalizedToByte(color[1]), NormalizedToByte(color[2]), NormalizedToByte(color[3]) );
}


static void ResetDeviceVertexPipeStateD3D9 (IDirect3DDevice9* dev, const TransformState& state, const BuiltinShaderParamValues& builtins, const VertexPipeConfig& config, const VertexPipeDataD3D9& data)
{
	DebugAssertIf (!dev);

	data.haveToResetDeviceState = false;

	dev->SetTransform( D3DTS_WORLD, (const D3DMATRIX*)state.worldViewMatrix.GetPtr() );
	Matrix4x4f dummyViewMatrix;
	dummyViewMatrix.SetIdentity(); dummyViewMatrix.Get(2,2) = -1.0f;
	dev->SetTransform( D3DTS_VIEW, (const D3DMATRIX*)dummyViewMatrix.GetPtr() );
	dev->SetTransform( D3DTS_PROJECTION, (const D3DMATRIX*)builtins.GetMatrixParam(kShaderMatProj).GetPtr() );

	dev->SetRenderState( D3DRS_COLORVERTEX, FALSE );

	for( int i = 0; i < kMaxSupportedVertexLights; ++i )
		dev->LightEnable( i, FALSE );

	dev->SetRenderState( D3DRS_AMBIENT, 0 );
	dev->SetRenderState( D3DRS_LIGHTING, FALSE );
	dev->SetRenderState( D3DRS_SPECULARENABLE, FALSE );

	for( int i = 0; i < kMaxSupportedTextureCoords; ++i ) {
		dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i );
		dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTSS_TCI_PASSTHRU );
	}
}

void ResetVertexPipeStateD3D9 (IDirect3DDevice9* dev, TransformState& state, BuiltinShaderParamValues& builtins, VertexPipeConfig& config, VertexPipeDataD3D9& data, VertexPipePrevious& previous)
{
	config.Reset();
	data.Reset();
	state.Invalidate(builtins);
	previous.Reset();

	data.haveToResetDeviceState = true;
	if (dev)
		ResetDeviceVertexPipeStateD3D9 (dev, state, builtins, config, data);
}


void SetupFixedFunctionD3D9 (
	IDirect3DDevice9* dev,
	TransformState& state,
	BuiltinShaderParamValues& builtins,
	const VertexPipeConfig& config,
	const VertexPipeDataD3D9& data,
	VertexPipePrevious& previous,
	bool vsActive, bool immediateMode)
{
	if (dev && data.haveToResetDeviceState)
		ResetDeviceVertexPipeStateD3D9 (dev, state, builtins, config, data);

	// matrices
	if (!vsActive)
	{
		D3D9_CALL(dev->SetTransform( D3DTS_WORLD, (const D3DMATRIX*)state.worldViewMatrix.GetPtr() ));
	}

	// set color material first, then material, then color
	if( config.colorMaterial != previous.config.colorMaterial )
	{
		if( config.colorMaterial != kColorMatDisabled )
		{
			D3DMATERIALCOLORSOURCE srcAmbient, srcDiffuse, srcEmission;
			switch( config.colorMaterial )
			{
			case kColorMatEmission:
				srcAmbient = D3DMCS_MATERIAL;
				srcDiffuse = D3DMCS_MATERIAL;
				srcEmission = D3DMCS_COLOR1;
				break;
			case kColorMatAmbientAndDiffuse:
				srcAmbient = D3DMCS_COLOR1;
				srcDiffuse = D3DMCS_COLOR1;
				srcEmission = D3DMCS_MATERIAL;
				break;
			default:
				return;
			}
			D3D9_CALL(dev->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, srcAmbient ));
			D3D9_CALL(dev->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, srcDiffuse ));
			D3D9_CALL(dev->SetRenderState( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL ));
			D3D9_CALL(dev->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, srcEmission ));
			D3D9_CALL(dev->SetRenderState( D3DRS_COLORVERTEX, TRUE ));
		}
		else
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_COLORVERTEX, FALSE ));
		}
	}

	// material
	if( !vsActive && config.hasLighting )
		D3D9_CALL(dev->SetMaterial( &data.material ));

	// lights
	D3DLIGHT9 d3dlight;
	d3dlight.Ambient.r = d3dlight.Ambient.g = d3dlight.Ambient.b = d3dlight.Ambient.a = 0.0f;
	d3dlight.Falloff = 1.0f;
	d3dlight.Attenuation0 = 1.0f;
	d3dlight.Attenuation1 = 0.0f;

	const UInt32 lightsEnabled = (1<<data.vertexLightCount)-1;
	const UInt32 lightsPrevious = (1<<previous.vertexLightCount)-1;
	const UInt32 lightsDifferent = lightsPrevious ^ lightsEnabled;
	UInt32 lightMask = 1;
	for (int i = 0; i < kMaxSupportedVertexLights; ++i, lightMask <<= 1)
	{
		const UInt32 lightDiff = lightsDifferent & lightMask;
		if( lightsEnabled & lightMask )
		{
			const GfxVertexLight& l = data.lights[i];
			static D3DLIGHTTYPE kD3DTypes[kLightTypeCount] = { D3DLIGHT_SPOT, D3DLIGHT_DIRECTIONAL, D3DLIGHT_POINT };
			d3dlight.Type = kD3DTypes[l.type];
			d3dlight.Diffuse = *(const D3DCOLORVALUE*)&l.color;
			d3dlight.Specular = *(const D3DCOLORVALUE*)&l.color;
			d3dlight.Position = *(const D3DVECTOR*)&l.position;
			d3dlight.Direction = *(const D3DVECTOR*)&l.spotDirection;
			d3dlight.Range = l.range;
			d3dlight.Attenuation2 = l.quadAtten;
			d3dlight.Theta = Deg2Rad(l.spotAngle) * 0.5f;
			d3dlight.Phi = Deg2Rad(l.spotAngle);
			D3D9_CALL(dev->SetLight (i,&d3dlight));
			if (lightDiff)
				D3D9_CALL(dev->LightEnable (i,TRUE));
		}
		else
		{
			if (lightDiff)
				D3D9_CALL(dev->LightEnable (i, FALSE));
		}
	}
	previous.vertexLightCount = data.vertexLightCount;


	// ambient, lighting & specular
	if( data.ambient != previous.ambient )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_AMBIENT, ColorToD3D(data.ambient.GetPtr()) ));
		previous.ambient = data.ambient;
	}
	if( config.hasLighting != previous.config.hasLighting )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_LIGHTING, config.hasLighting ? TRUE : FALSE ));
	}
	if( config.hasSpecular != previous.config.hasSpecular )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_SPECULARENABLE, config.hasSpecular ? TRUE : FALSE ));
	}
	if (config.hasNormalization != previous.config.hasNormalization)
	{
		D3D9_CALL(dev->SetRenderState (D3DRS_NORMALIZENORMALS, config.hasNormalization ? TRUE : FALSE));
	}


	UInt32 textureMatrixModes = config.textureMatrixModes;
	UInt32 projectedTextures = data.projectedTextures;
	UInt32 textureSources = config.textureSources;
	for( int i = 0; i < config.texCoordCount; ++i )
	{
		// texgen
		UInt32 texSource = (textureSources >> (i*3)) & 0x7;
		if( !vsActive )
		{
			static DWORD kTexSourceFlags[kTexSourceTypeCount] = { 0, 1, D3DTSS_TCI_SPHEREMAP, D3DTSS_TCI_CAMERASPACEPOSITION, D3DTSS_TCI_CAMERASPACEPOSITION, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR, D3DTSS_TCI_CAMERASPACENORMAL };
			DWORD d3dsource = kTexSourceFlags[texSource];
			if( immediateMode && texSource <= kTexSourceUV1 )
				d3dsource = i;
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, d3dsource ));
		}
		else
		{
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i ));
		}

		// matrix
		unsigned matmode = (textureMatrixModes >> (i*2)) & 3;
		static DWORD kTexFlags[kTexMatrixTypeCount] = { D3DTTFF_DISABLE, D3DTTFF_COUNT2, D3DTTFF_COUNT3, D3DTTFF_COUNT4 };
		DWORD textureTransformFlags = kTexFlags[matmode];
		if (projectedTextures & (1<<i))
			textureTransformFlags |= D3DTTFF_PROJECTED;
		if (vsActive)
			textureTransformFlags = D3DTTFF_DISABLE;
		D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, textureTransformFlags ));

		if( !vsActive )
		{
			if( texSource == kTexSourceObject )
			{
				// D3D has no "object space" texture generation.
				// So instead we use camera space, and multiply the matrix so it matches:
				//   newMatrix = matrix * inverse(modelview) * mirrorZ
				// Mirror along Z is required to match OpenGL's generation (eye space Z is negative).
				Matrix4x4f mv = state.worldViewMatrix;
				mv.Invert_Full();
				// Negate Z axis (mv = mv * Scale(1,1,-1))
				mv.Get(0,2) = -mv.Get(0,2);
				mv.Get(1,2) = -mv.Get(1,2);
				mv.Get(2,2) = -mv.Get(2,2);
				mv.Get(3,2) = -mv.Get(3,2);
				Matrix4x4f texmat;
				MultiplyMatrices4x4 (&state.texMatrices[i], &mv, &texmat);
				D3D9_CALL(dev->SetTransform( (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + i), (const D3DMATRIX*)texmat.GetPtr() ));
			}
			else
			{
				D3D9_CALL(dev->SetTransform( (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + i), (const D3DMATRIX*)state.texMatrices[i].GetPtr() ));
			}
		}
	}
	if( config.texCoordCount != previous.config.texCoordCount )
	{
		for( int i = config.texCoordCount; i < kMaxSupportedTextureCoords; ++i ) 
		{
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE ));
		}
	}

	if( !vsActive )
		D3D9_CALL(dev->SetVertexShader(NULL));
	previous.vertexShader = NULL;
	previous.config = config;
}





// ----------------------------------------------------------------------




struct VSLightData {
	Vector4f pos;
	Vector4f dir;
	Vector4f color;
	Vector4f params;
};

struct ShaderData {
	IDirect3DVertexShader9* shader;
	unsigned int usedConstants;
	std::string text;
	//std::string debug;
};


struct VertexPipeKeyCompare {
	union {
		VertexPipeConfig	key;
		UInt64				asint;
	} u;
	VertexPipeKeyCompare() { u.asint = 0; }
	bool operator <( const VertexPipeKeyCompare& r ) const { return u.asint < r.u.asint; }
};

typedef std::map<VertexPipeKeyCompare, ShaderData> ShaderCache;
static ShaderCache g_Shaders;


static IDirect3DVertexShader9* GetShaderForConfig( const VertexPipeConfig& config, IDirect3DDevice9* dev, unsigned int& usedConstants )
{
	VertexPipeKeyCompare key;
	key.u.key = config;
	ShaderCache::iterator it = g_Shaders.find(key);
	if( it != g_Shaders.end() ) {
		const ShaderData& sdata = it->second;
		usedConstants = sdata.usedConstants;
		return sdata.shader;
	}

	ShaderGenerator gen;
	gen.AddFragment( &kVS_Pos );

	// lighting
	if( config.hasLighting )
	{
		// normalize normals?
		if (config.hasNormalization)
			gen.AddFragment (&kVS_Normalize_Normal);

		UInt32 hasLightType = config.hasLightType;
		if( config.hasSpecular )
		{
			gen.AddFragment( &kVS_Light_Specular_Pre );
			if( hasLightType & (1<<kLightDirectional) )
				gen.AddFragment( &kVS_Light_Specular_Dir );
			if( hasLightType & (1<<kLightPoint) )
				gen.AddFragment( &kVS_Light_Specular_Point );
			if( hasLightType & (1<<kLightSpot) )
				gen.AddFragment( &kVS_Light_Specular_Spot );
		}
		else
		{
			gen.AddFragment( &kVS_Light_Diffuse_Pre );
			if( hasLightType & (1<<kLightDirectional) )
				gen.AddFragment( &kVS_Light_Diffuse_Dir );
			if( hasLightType & (1<<kLightPoint) )
				gen.AddFragment( &kVS_Light_Diffuse_Point );
			if( hasLightType & (1<<kLightSpot) )
				gen.AddFragment( &kVS_Light_Diffuse_Spot );
		}

		const ShaderFragment* frag = NULL;
		if( config.hasVertexColor ) {
			switch( config.colorMaterial ) {
			case kColorMatAmbientAndDiffuse: frag = &kVS_Out_Diffuse_Lighting_ColorDiffuseAmbient; break;
			case kColorMatEmission: frag = &kVS_Out_Diffuse_Lighting_ColorEmission; break;
			default: frag = &kVS_Out_Diffuse_Lighting; break;
			}
		} else {
			frag = &kVS_Out_Diffuse_Lighting;
		}
		gen.AddFragment( frag );

		if( config.hasSpecular ) {
			gen.AddFragment( &kVS_Out_Specular_Lighting );
		}

	}
	else
	{
		if( config.hasVertexColor )
			gen.AddFragment( &kVS_Out_Diffuse_VertexColor );
		else
			gen.AddFragment( &kVS_Out_Diffuse_White );
	}
	// texgen
	static const ShaderFragment* kFragSources[kTexSourceTypeCount] = {
		&kVS_Load_UV0,
		&kVS_Load_UV1,
		&kVS_Temp_SphereMap,
		&kVS_Temp_ObjSpacePos,
		&kVS_Temp_CamSpacePos,
		&kVS_Temp_CamSpaceRefl,
		&kVS_Temp_CamSpaceN,
	};
	static const char* kFragSourceNames[kTexSourceTypeCount] = {
		"UV0",
		"UV1",
		"SPHR",
		"OPOS",
		"CPOS",
		"REFL",
		"CNOR",
	};
	static const ShaderFragment* kFragMatrices[kTexMatrixTypeCount] = {
		&kVS_Out_TexCoord,
		&kVS_Out_Matrix2,
		&kVS_Out_Matrix3,
		&kVS_Out_Matrix3
	};
	for( int i = 0; i < config.texCoordCount; ++i )
	{
		unsigned src = (config.textureSources >> (i*3)) & 7;
		// normalize normals?
		if (config.hasNormalization)
		{
			if (src == kTexSourceSphereMap || src == kTexSourceCubeReflect || src == kTexSourceCubeNormal)
				gen.AddFragment (&kVS_Normalize_Normal);
		}
		gen.AddFragment( kFragSources[src] );
	}
	for( int i = 0; i < config.texCoordCount; ++i )
	{
		unsigned src = (config.textureSources >> (i*3)) & 7;
		unsigned matmode = (config.textureMatrixModes >> (i*2)) & 3;
		gen.AddFragment (kFragMatrices[matmode], kFragSourceNames[src], i);
	}
	ShaderData data;
	data.shader = NULL;
	gen.GenerateShader( data.text, data.usedConstants );

	ID3DXBuffer* compiledShader = AssembleD3DShader( data.text );
	if( compiledShader ) {
		dev->CreateVertexShader( (const DWORD*)compiledShader->GetBufferPointer(), &data.shader );
		compiledShader->Release();
	}

	AssertIf(!data.shader);
	g_Shaders.insert( std::make_pair(key, data) );

	#if PRINT_AMD_SHADER_ANALYZER_OUTPUT
	PrintAMDShaderAnalyzer( data.text );
	#endif

	usedConstants = data.usedConstants;
	return data.shader;
}

void SetupVertexShaderD3D9 (
	IDirect3DDevice9* dev,
	TransformState& state,
	const BuiltinShaderParamValues& builtins,
	VertexPipeConfig& config,
	const VertexPipeDataD3D9& data,
	VertexPipePrevious& previous,
	VertexShaderConstantCache& cache,
	bool vsActive, bool immediateMode)
{
	if( vsActive )
		return;

	D3D9_CALL(dev->SetTransform( D3DTS_WORLD, (const D3DMATRIX*)state.worldViewMatrix.GetPtr() ));

	// figure out which light types do we have
	if( !config.hasLighting ) {
		config.hasLightType = 0;
	} else {
		UInt32 hasLightType = 0;
		for (int i = 0; i < data.vertexLightCount; ++i)
		{
			hasLightType |= (1<<data.lights[i].type);
		}
		config.hasLightType = hasLightType;
	}

	// create vertex shader
	unsigned int usedConstants;
	IDirect3DVertexShader9* shader = GetShaderForConfig(config, dev, usedConstants);
	AssertIf(!shader);

	// set shader
	if( shader != previous.vertexShader )
	{
		D3D9_CALL(dev->SetVertexShader( shader ));
		previous.vertexShader = shader;
	}

	// matrices
	Matrix4x4f mvp;
	MultiplyMatrices4x4 (&builtins.GetMatrixParam(kShaderMatProj), &state.worldViewMatrix, &mvp );
	mvp.Transpose();
	cache.SetValues( kConstantLocations[kConstMatrixMVP], mvp.GetPtr(), 4 );

	const Matrix4x4f& mv = state.worldViewMatrix;
	cache.SetValues( kConstantLocations[kConstMatrixMV], mv.GetPtr(), 4 );

	if( usedConstants & (1<<kConstMatrixMV_IT) )
	{
		Matrix4x4f matrixTemp;
		Matrix4x4f::Invert_General3D( mv, matrixTemp );
		matrixTemp.Transpose();
		if (data.normalization == kNormalizationScale)
		{
			// Inverse transpose of modelview is only used to transform the normals
			// in our generated shader. We can just stuff mesh scale in there.
			float scale = Magnitude (state.worldMatrix.GetAxisX());
			matrixTemp.Get (0, 0) *= scale;
			matrixTemp.Get (1, 0) *= scale;
			matrixTemp.Get (2, 0) *= scale;
			matrixTemp.Get (0, 1) *= scale;
			matrixTemp.Get (1, 1) *= scale;
			matrixTemp.Get (2, 1) *= scale;
			matrixTemp.Get (0, 2) *= scale;
			matrixTemp.Get (1, 2) *= scale;
			matrixTemp.Get (2, 2) *= scale;
		}
		cache.SetValues( kConstantLocations[kConstMatrixMV_IT], matrixTemp.GetPtr(), 4 );
	}

	// misc
	float misc[4] = { 0, 4, 1, 0.5f };
	cache.SetValues( kConstantLocations[kConstLightMisc], misc, 1 );

	// if lighting is used:
	if( config.hasLighting )
	{
		// ambient
		if( config.colorMaterial != kColorMatAmbientAndDiffuse )
		{
			SimpleVec4 amb;
			amb.val[0] = data.ambientClamped.val[0] * data.material.Ambient.r;
			amb.val[1] = data.ambientClamped.val[1] * data.material.Ambient.g;
			amb.val[2] = data.ambientClamped.val[2] * data.material.Ambient.b;
			amb.val[3] = data.ambientClamped.val[3] * data.material.Ambient.a;
			if( config.colorMaterial != kColorMatEmission ) {
				amb.val[0] += data.material.Emissive.r;
				amb.val[1] += data.material.Emissive.g;
				amb.val[2] += data.material.Emissive.b;
				amb.val[3] += data.material.Emissive.a;
			}
			cache.SetValues( kConstantLocations[kConstAmbient], amb.GetPtr(), 1 );
		}
		else
		{
			cache.SetValues( kConstantLocations[kConstColorMatAmbient], data.ambientClamped.GetPtr(), 1 );
			cache.SetValues( kConstantLocations[kConstAmbient], &data.material.Emissive.r, 1 );
		}
		previous.ambient = data.ambient;

		// material
		cache.SetValues( kConstantLocations[kConstMatDiffuse], &data.material.Diffuse.r, 1 );
			D3D9_CALL(dev->SetVertexShaderConstantF( kConstantLocations[kConstMatDiffuse], &data.material.Diffuse.r, 1 ));
		if( usedConstants & (1<<kConstMatSpecular) )
		{
			D3DCOLORVALUE specAndPower = data.material.Specular;
			specAndPower.a = data.material.Power;
			cache.SetValues( kConstantLocations[kConstMatSpecular], &specAndPower.r, 1 );
		}

		// pack the lights
		int lightCounts[kLightTypeCount];
		float lightStart[kLightTypeCount];
		int lightsTotal = 0;
		float lightsTotalF = 0;
		memset(lightCounts, 0, sizeof(lightCounts));
		memset(lightStart, 0, sizeof(lightStart));
		VSLightData lights[kMaxSupportedVertexLights];
		for( int t = 0; t < kLightTypeCount; ++t )
		{
			lightStart[t] = lightsTotalF;
			for( int i = 0; i < data.vertexLightCount; ++i )
			{
				const GfxVertexLight& src = data.lights[i];
				if( src.type != t )
					continue;

				VSLightData& dst = lights[lightsTotal];
				// position
				dst.pos.Set( src.position.x, src.position.y, src.position.z, 1.0f );
				// direction
				dst.dir.Set( -src.spotDirection.x, -src.spotDirection.y, -src.spotDirection.z, 0.0f );
				// color
				dst.color.Set( src.color.x, src.color.y, src.color.z, 1.0f );
				// params: 1/(cos(theta/2)-cos(phi/2), cos(phi/2), range^2, d^2 attenuation
				float sqrRange = src.range * src.range;
				if( src.type == kLightSpot )
				{
					float cosTheta = cosf(Deg2Rad(src.spotAngle)*0.25f);
					float cosPhi = cosf(Deg2Rad(src.spotAngle)*0.5f);
					float cosDiff = cosTheta - cosPhi;
					dst.params.Set(
						cosDiff != 0.0f ? 1.0f / cosDiff : 0.0f,
						cosPhi,
						src.range * src.range,
						src.quadAtten
					);
				}
				else
				{
					dst.params.Set(
						0.0f,
						0.0f,
						src.range * src.range,
						src.quadAtten
					);
				}

				++lightCounts[t];
				++lightsTotal;
				++lightsTotalF;
			}
		}

		// light indices
		int miscI[kLightTypeCount][4];
		for( int t = 0; t < kLightTypeCount; ++t ) {
			miscI[t][0] = lightCounts[t];
			miscI[t][1] = 0;
			miscI[t][2] = 0;
			miscI[t][3] = 0;
		}
		D3D9_CALL(dev->SetVertexShaderConstantI( 0, miscI[0], kLightTypeCount ));

		if (lightsTotal)
			cache.SetValues(  60, (const float*)lights, 4*lightsTotal );
		misc[0] = lightStart[0] * 4.0f;
		misc[1] = lightStart[1] * 4.0f;
		misc[2] = lightStart[2] * 4.0f;
		misc[3] = 0.0f;
		cache.SetValues(kConstantLocations[kConstLightIndexes], misc, 1);
	}

	// texture matrices & transform flags
	UInt32 matrixModes = config.textureMatrixModes;
	UInt32 projectedTextures = data.projectedTextures;
	UInt32 textureSources = config.textureSources;
	for( int i = 0; i < config.texCoordCount; ++i )
	{
		unsigned matmode = (matrixModes >> (i*2)) & 0x3;
		if( matmode != kTexMatrixNone )
		{
			cache.SetValues(kConstantLocations[kConstMatrixTexture]+i*4, state.texMatrices[i].GetPtr(), 4);
		}
		D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i ));
		// projected texture flag
		DWORD textureTransformFlags = (projectedTextures & (1<<i)) ? D3DTTFF_PROJECTED : D3DTTFF_DISABLE;
		D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, textureTransformFlags ));
	}

	if( config.texCoordCount != previous.config.texCoordCount )
	{
		for( int i = config.texCoordCount; i < kMaxSupportedTextureCoords; ++i ) 
		{
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE ));
		}
	}

	previous.config = config;
}


void CleanupVertexShadersD3D9 ()
{
	#if PRINT_VERTEX_PIPE_STATS
	printf_console("Vertex pipe shader cache: %i shaders generated\n", g_Shaders.size());
	#endif
	ShaderCache::iterator it, itEnd = g_Shaders.end();
	for( it = g_Shaders.begin(); it != itEnd; ++it )
	{
		IDirect3DVertexShader9* vs = it->second.shader;
		if( vs ) {
			ULONG refCount = vs->Release();
			AssertIf( refCount != 0 );
		}
	}
	g_Shaders.clear ();
}

