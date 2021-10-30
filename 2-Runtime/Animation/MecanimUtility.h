#ifndef MECANIM_UTILITY_H
#define MECANIM_UTILITY_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Allocator/BaseAllocator.h"
#include "Runtime/Misc/AllocatorLabels.h"

#include "Runtime/mecanim/memory.h"
#include "Runtime/Math/Simd/xform.h"

#include "Runtime/Serialize/Blobification/BlobWrite.h"

#include <stack>
#include <algorithm>

std::string BuildTransitionName(std::string srcStateName, std::string dstStateName);

typedef std::map<mecanim::uint32_t, UnityStr> TOSVector;

static inline Vector3f float4ToVector3f(math::float4 const& v)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float buf[4];
	math::store(v, buf);
	//return Vector3f(-buf[0],buf[1],buf[2]);
	return Vector3f(buf[0],buf[1],buf[2]);
}

static inline Quaternionf float4ToQuaternionf(math::float4 const& q)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float buf[4];
	math::store(math::normalize(q), buf);
	//return Quaternionf(-buf[0],buf[1],buf[2],-buf[3]);
	return Quaternionf(buf[0],buf[1],buf[2],buf[3]);
}

static inline Quaternionf float4ToQuaternionfNoNormalize(math::float4 const& q)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float buf[4];
	math::store(q, buf);
	return Quaternionf(buf[0],buf[1],buf[2],buf[3]);
}

static inline void xform2unity(math::xform const& x, Vector3f& t, Quaternionf& q, Vector3f& s)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float bufS[4];	

	math::store(x.s, bufS);		

	t = float4ToVector3f(x.t);
	q = float4ToQuaternionf(x.q);
	s.Set(bufS[0], bufS[1], bufS[2]);		
}

static inline void xform2unityNoNormalize(math::xform const& x, Vector3f& t, Quaternionf& q, Vector3f& s)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float bufS[4];	

	math::store(x.s, bufS);		

	t = float4ToVector3f(x.t);
	q = float4ToQuaternionfNoNormalize(x.q);
	s.Set(bufS[0], bufS[1], bufS[2]);		
}

static inline void xform2unity(math::xform const& x, Matrix4x4f& matrix)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float bufS[4];	
	
	Vector3f t;
	Quaternionf q;
	Vector3f s;
	
	math::store(x.s, bufS);		
	
	t = float4ToVector3f(x.t);
	q = float4ToQuaternionf(x.q);
	s.Set(bufS[0], bufS[1], bufS[2]);
	matrix.SetTRS(t, q, s);
}


static inline math::float4 Vector3fTofloat4(Vector3f const& v, float w = 0)
{
	//ATTRIBUTE_ALIGN(ALIGN4F) float buf[4] = {-v.x, v.y, v.z, 0};
	ATTRIBUTE_ALIGN(ALIGN4F) float buf[4] = {v.x, v.y, v.z, w};
	return math::load(buf);
}

static inline math::float4 QuaternionfTofloat4(Quaternionf  const& q)
{
	//ATTRIBUTE_ALIGN(ALIGN4F) float buf[4] = {-q.x, q.y, q.z, -q.w};
	ATTRIBUTE_ALIGN(ALIGN4F) float buf[4] = {q.x, q.y, q.z, q.w};
	return math::load(buf);
}

static inline math::xform xformFromUnity(Vector3f const& t, Quaternionf const& q, Vector3f const& s)
{
	ATTRIBUTE_ALIGN(ALIGN4F) float bufS[4];

	bufS[0] = s.x; bufS[1] = s.y; bufS[2] = s.z; bufS[3] = 1.f; 
	return math::xform(Vector3fTofloat4(t), QuaternionfTofloat4(q), math::load(bufS));
}

std::string FileName(const std::string &fullpath);
std::string FileNameNoExt(const std::string &fullpath);

unsigned int ProccessString(TOSVector& tos, std::string const& str);
std::string FindString(TOSVector const& tos, unsigned int crc32);
template <typename T> inline T* CopyBlob(T const& data, mecanim::memory::Allocator& allocator, size_t& size)
{
	BlobWrite::container_type blob;
	BlobWrite blobWrite (blob, kNoTransferInstructionFlags, kBuildNoTargetPlatform);
	blobWrite.Transfer( const_cast<T&>(data), "Base");

	UInt8* ptr = reinterpret_cast<UInt8*>(allocator.Allocate(blob.size(), ALIGN_OF(T)));
	if(ptr != 0)
		memcpy(ptr, blob.begin(), blob.size());
	size = blob.size();
	return reinterpret_cast<T*>(ptr);
}

#endif
