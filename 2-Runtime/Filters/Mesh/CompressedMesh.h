#ifndef COMPRESSEDMESH_H
#define COMPRESSEDMESH_H

#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Animation/AnimationClip.h"
class Mesh;
class AnimationClip;

enum
{
	kMeshCompressionOff = 0,
	kMeshCompressionLow = 1,
	kMeshCompressionMed = 2,
	kMeshCompressionHigh = 3,
};

typedef std::vector<UInt8> DataVector;

class PackedFloatVector 
{
public: 
	DECLARE_SERIALIZE (PackedBitVector)
	
	PackedFloatVector() { m_NumItems = 0; m_Range = 0; m_Start = 0; m_BitSize = 0; }
	
	void PackFloats(float *data, int chunkSize, int chunkStride, int chunkCount, int bitSize, bool adjustBitSize);
	void UnpackFloats(float *data, int chunkSize, int chunkStride, int start = 0, int count = -1);
	int Count() {return m_NumItems;}
	
private:
	UInt32 m_NumItems;
	float m_Range;
	float m_Start;
	UInt8 m_BitSize;
	std::vector<UInt8> m_Data;
};

class PackedIntVector 
{
public: 
	DECLARE_SERIALIZE (PackedBitVector)
	
	PackedIntVector() { m_NumItems = 0; m_BitSize = 0; }
	
	template <class IntSize> void PackInts(IntSize *data, int numItems);
	template <class IntSize> void UnpackInts(IntSize *data);
	int Count() {return m_NumItems;}
	
private:
	UInt32 m_NumItems;
	UInt8 m_BitSize;
	std::vector<UInt8> m_Data;
};

class PackedQuatVector 
{
public: 
	DECLARE_SERIALIZE (PackedBitVector)
	
	PackedQuatVector() {m_NumItems = 0;}
	
	void PackQuats(Quaternionf *data, int numItems);
	void UnpackQuats(Quaternionf *data);
	int Count() {return m_NumItems;}
	
private:
	UInt32 m_NumItems;
	std::vector<UInt8> m_Data;
};

class CompressedMesh
{
public:
	DECLARE_SERIALIZE (CompressedMesh)
		
	void Compress(Mesh &src, int quality);
	void Decompress(Mesh &src);	
	
private:
	PackedFloatVector m_Vertices;
	PackedFloatVector m_UV;

	// TODO: This never gets written. Unity 3.4 and 3.5 never wrote this data.
	// Most likely no version ever did. Remove code and bindpose serialization.
	PackedFloatVector m_BindPoses;

	PackedFloatVector m_Normals;
	PackedIntVector m_NormalSigns;
	PackedFloatVector m_Tangents;
	PackedIntVector m_TangentSigns;
	PackedIntVector m_Weights;
	PackedIntVector m_BoneIndices;
	PackedIntVector m_Triangles;
	PackedIntVector m_Colors;
};

template<class TransferFunc>
void PackedFloatVector::Transfer (TransferFunc& transfer) {
	TRANSFER ( m_NumItems );
	TRANSFER( m_Range );
	TRANSFER( m_Start );
	TRANSFER( m_Data );
	TRANSFER( m_BitSize );
	transfer.Align();
}

template<class TransferFunc>
void PackedIntVector::Transfer (TransferFunc& transfer) {
	TRANSFER( m_NumItems );
	TRANSFER( m_Data );
	TRANSFER( m_BitSize );
	transfer.Align();
}

template<class TransferFunc>
void PackedQuatVector::Transfer (TransferFunc& transfer) {
	TRANSFER( m_NumItems );
	TRANSFER( m_Data );
	transfer.Align();
}

template<class TransferFunc>
void CompressedMesh::Transfer (TransferFunc& transfer) {
	TRANSFER( m_Vertices );
	TRANSFER( m_UV );
	TRANSFER( m_BindPoses );
	TRANSFER( m_Normals );
	TRANSFER( m_Tangents );
	TRANSFER( m_Weights );
	TRANSFER( m_NormalSigns );
	TRANSFER( m_TangentSigns );
	TRANSFER( m_BoneIndices );
	TRANSFER( m_Triangles );
	TRANSFER( m_Colors );
}

class CompressedAnimationCurve
{
public:
	DECLARE_SERIALIZE (CompressedAnimationCurve)

	CompressedAnimationCurve() { m_PreInfinity = 0; m_PostInfinity = 0; }
		
	void CompressQuatCurve(AnimationClip::QuaternionCurve &src);
	void DecompressQuatCurve(AnimationClip::QuaternionCurve &src);	
		
private:

	template <class T> void CompressTimeKeys(AnimationCurveTpl<T> &src);
	template <class T> void DecompressTimeKeys(AnimationCurveTpl<T> &src);	

	PackedIntVector m_Times;
	PackedQuatVector m_Values;
	PackedFloatVector m_Slopes;
	
	int   m_PreInfinity;
	int   m_PostInfinity;	
	
	UnityStr m_Path;
};

template<class TransferFunc>
void CompressedAnimationCurve::Transfer (TransferFunc& transfer) {
	
	TRANSFER( m_Path );

	TRANSFER( m_Times );
	TRANSFER( m_Values );
	TRANSFER( m_Slopes );

	TRANSFER( m_PreInfinity );
	TRANSFER( m_PostInfinity );
}

#endif
