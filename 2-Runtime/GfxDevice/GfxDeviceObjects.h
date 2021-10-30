#pragma once

#include "Runtime/Camera/Lighting.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector4.h"
#include "GfxDeviceTypes.h"

// --------------------------------------------------------------------------

// A type safe opaque pointer to something.
// ID type is used just so that you can have handles to different types, all type safe.
template<typename ID, typename ObjectPtrType=void*>
struct ObjectHandle
{
	explicit ObjectHandle(ObjectPtrType obj = 0) : object(obj) {}
	bool IsValid() const { return object != 0; }
	void Reset() { object = 0; }
	bool operator==( const ObjectHandle<ID,ObjectPtrType>& o ) const { return object == o.object; }
	bool operator!=( const ObjectHandle<ID,ObjectPtrType>& o ) const { return object != o.object; }

	ObjectPtrType object;
};

#define OBJECT_FROM_HANDLE(handle,type) reinterpret_cast<type*>((handle).object)


// --------------------------------------------------------------------------

struct RenderSurfaceBase;
struct RenderSurface_Tag;
typedef ObjectHandle<RenderSurface_Tag, RenderSurfaceBase*> RenderSurfaceHandle;

struct GraphicsContext_Tag;
typedef ObjectHandle<GraphicsContext_Tag> GraphicsContextHandle;

struct TextureCombiners_Tag;
typedef ObjectHandle<TextureCombiners_Tag> TextureCombinersHandle;

struct ComputeProgram_Tag;
typedef ObjectHandle<ComputeProgram_Tag> ComputeProgramHandle;

struct ConstantBuffer_Tag;
typedef ObjectHandle<ConstantBuffer_Tag> ConstantBufferHandle;

// --------------------------------------------------------------------------


struct SimpleVec4
{
	float val[4];

	void set( const float *v )
	{
		val[0] = v[0];
		val[1] = v[1];
		val[2] = v[2];
		val[3] = v[3];
	}
	void set( float v0, float v1, float v2, float v3 )
	{
		val[0] = v0;
		val[1] = v1;
		val[2] = v2;
		val[3] = v3;
	}

	bool operator==(const SimpleVec4& o) const {
		return val[0] == o.val[0] && val[1] == o.val[1] && val[2] == o.val[2] && val[3] == o.val[3];
	}
	bool operator!=(const SimpleVec4& o) const {
		return val[0] != o.val[0] || val[1] != o.val[1] || val[2] != o.val[2] || val[3] != o.val[3];
	}
	bool operator==(const float* o) const {
		return val[0] == o[0] && val[1] == o[1] && val[2] == o[2] && val[3] == o[3];
	}
	bool operator!=(const float* o) const {
		return val[0] != o[0] || val[1] != o[1] || val[2] != o[2] || val[3] != o[3];
	}

	const float* GetPtr() const { return val; }
};



// --------------------------------------------------------------------------


struct GfxVertexLight
{
	Vector4f		position;	// directional: direction (w=0); others: position (w=1)
	Vector4f		spotDirection; // w = 0
	Vector4f		color;		// diffuse&specular color
	float			range;		// range
	float			quadAtten;	// quadratic attenuation (constant = 1, linear = 0)
	float			spotAngle;	// in degrees of full cone; -1 if not spot light
	LightType		type;

	GfxVertexLight()
		: position(Vector3f::zero,1.0f)
		, spotDirection (Vector3f::zAxis,1.0f)
		, color (Vector3f::zero,1.0f)
		, range (0.0f)
		, quadAtten	(0.0f)
		, spotAngle (0.0f)
		, type (kLightDirectional)
	{}
};


struct GfxMaterialParams
{
	Vector4f ambient;
	Vector4f diffuse;
	Vector4f specular;
	Vector4f emissive;
	float shininess;

	void Invalidate()
	{
		ambient.Set( -1, -1, -1, -1 );
		diffuse.Set( -1, -1, -1, -1 );
		specular.Set( -1, -1, -1, -1 );
		emissive.Set( -1, -1, -1, -1 );
		shininess = -1.0f;
	}
};


struct GfxFogParams
{
	FogMode		mode;
	Vector4f	color;
	float		start;
	float		end;
	float		density;

	void Invalidate()
	{
		mode = kFogUnknown;
		color.Set( -1, -1, -1, -1 );
		start = -1.0f;
		end = -1.0f;
		density = -1.0f;
	}
};
