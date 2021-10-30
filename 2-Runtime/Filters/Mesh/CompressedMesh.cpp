#include "UnityPrefix.h"
#include "CompressedMesh.h"
#include "LodMesh.h"
#include "Runtime/Animation/AnimationCurveUtility.h"


#define sqr(x) ((x)*(x))

void PackedFloatVector::PackFloats(float *data, int itemCountInChunk, int chunkStride, int numChunks, int bitSize, bool adjustBitSize)
{ 
	float maxf = -std::numeric_limits<float>::infinity();
	float minf = std::numeric_limits<float>::infinity();
	float* end = Stride (data, numChunks * chunkStride);
	for(float* it = data; it != end; it = Stride (it, chunkStride))
	{
		for (int i=0; i<itemCountInChunk; ++i)
		{
			if(maxf < it[i])
				maxf = it[i];
			if(minf > it[i])
				minf = it[i];
		}
	}
	
	m_Range = maxf-minf;

	if(adjustBitSize)
		bitSize += int(ceilf(Log2(m_Range)));
	if(bitSize > 32)
		bitSize = 32;
		
	m_Start = minf;
	m_NumItems = numChunks * itemCountInChunk;
	m_BitSize = bitSize;
	m_Data.resize((m_NumItems * bitSize + 7)/8, 0);
	
	
	float scale = 1.0/m_Range;
	
	int indexPos = 0;
	int bitPos = 0;
		
	for(float* it = data; it != end; it = Stride (it, chunkStride))
	{
		for(int i=0; i<itemCountInChunk; ++i)
		{
			float scaled = (it[i] - m_Start) * scale;
			if(scaled < 0) scaled = 0;
			if(scaled > 1) scaled = 1;

			UInt32 x = UInt32(scaled * ((1 << (m_BitSize)) - 1));

			int bits = 0;
			while(bits < m_BitSize)
			{
				m_Data[indexPos] |= (x >> bits) << bitPos;
				int num = std::min( m_BitSize-bits, 8-bitPos);
				bitPos += num;
				bits += num;
				if(bitPos == 8)
				{
					indexPos++;
					bitPos = 0;
				}
			}
		}
	}
}

void PackedFloatVector::UnpackFloats(float *data, int itemCountInChunk, int chunkStride, int start, int numChunks)
{
	int bitPos = m_BitSize*start;
	int indexPos = bitPos/8;
	bitPos %= 8;
	
	float scale = 1.0/m_Range;
	if (numChunks == -1)
		numChunks = m_NumItems / itemCountInChunk;
	
	for(float* end = Stride (data, chunkStride * numChunks); data != end; data = Stride (data, chunkStride))
	{
		for (int i=0; i<itemCountInChunk; ++i)
		{
			UInt32 x = 0;
					
			int bits = 0;
			while(bits < m_BitSize)
			{
				x |= (m_Data[indexPos] >> bitPos) << bits;
				int num = std::min( m_BitSize-bits, 8-bitPos);
				bitPos += num;
				bits += num;
				if(bitPos == 8)
				{
					indexPos++;
					bitPos = 0;
				}
			}
			x &= (1 << m_BitSize) - 1;
			data[i] = (x / (scale * ((1 << (m_BitSize)) - 1))) + m_Start;
		}
	}
}

template <class IntSize> void PackedIntVector::PackInts(IntSize *data, int numItems)
{ 
	// make sure that the intsize is an unsigned type
	Assert( (IntSize)0 < (IntSize)-1 );

	UInt32 maxi = 0;
	for(int i=0; i<numItems; i++)
		if(maxi < data[i])
			maxi = data[i];
	
	m_NumItems = numItems;
	//Prevent overflow
	m_BitSize = UInt8(maxi == 0xFFFFFFFF ? 32 : ceilf(Log2(maxi+1)));
	m_Data.resize((numItems * m_BitSize + 7)/8, 0);

	
	int indexPos = 0;
	int bitPos = 0;
	for(int i=0; i<numItems; i++)
	{
		int bits = 0;
		while(bits < m_BitSize)
		{
			m_Data[indexPos] |= (data[i] >> bits) << bitPos;
			int num = std::min( m_BitSize-bits, 8-bitPos);
			bitPos += num;
			bits += num;
			if(bitPos == 8)
			{
				indexPos++;
				bitPos = 0;
			}
		}
	}
}

template <class IntSize> void PackedIntVector::UnpackInts(IntSize *data)
{
	int indexPos = 0;
	int bitPos = 0;
	for(int i=0; i<m_NumItems; i++)
	{				
		int bits = 0;
		data[i] = 0;
		while(bits < m_BitSize)
		{
			data[i] |= (m_Data[indexPos] >> bitPos) << bits;
			int num = std::min( m_BitSize-bits, 8-bitPos);
			bitPos += num;
			bits += num;
			if(bitPos == 8)
			{
				indexPos++;
				bitPos = 0;
			}
		}
		data[i] &= (1ULL << m_BitSize) - 1;
	}
}


void PackedQuatVector::PackQuats(Quaternionf *data, int numItems)
{ 
	m_NumItems = numItems;
	m_Data.resize(numItems * (32/8), 0);
			
	int indexPos = 0;
	int bitPos = 0;
		
	for(int i=0; i<numItems; i++)
	{
		Quaternionf &q = data[i];
		UInt8 flags = q.x<0? 4:0;
		
		float max=fabs(q.x);
		if(fabs(q.y) > max)
		{
			max = fabs(q.y);
			flags = 1;
			if(q.y<0)
				flags |= 4;
		}
		if(fabs(q.z) > max)
		{
			max = fabs(q.z);
			flags = 2;
			if(q.z<0)
				flags |= 4;
		}
		if(fabs(q.w) > max)
		{
			max = fabs(q.w);
			flags = 3;
			if(q.w<0)
				flags |= 4;
		}
		int bits = 0;
		while(bits < 3)
		{
			m_Data[indexPos] |= (flags >> bits) << bitPos;
			int num = std::min( 3-bits, 8-bitPos);
			bitPos += num;
			bits += num;
			if(bitPos == 8)
			{
				indexPos++;
				bitPos = 0;
			}
		}		
		for(int j=0;j<4;j++)
		{
			if((flags&3) != j)
			{
				int bitSize = (((flags&3)+1)%4 == j)?9:10;
				float scaled = (q[j] + 1) * 0.5;
				if(scaled < 0) scaled = 0;
				if(scaled > 1) scaled = 1;
				
				UInt32 x = UInt32(scaled * ((1 << bitSize) - 1));
				
				bits = 0;
				while(bits < bitSize)
				{
					m_Data[indexPos] |= (x >> bits) << bitPos;
					int num = std::min( bitSize-bits, 8-bitPos);
					bitPos += num;
					bits += num;
					if(bitPos == 8)
					{
						indexPos++;
						bitPos = 0;
					}
				}
			}
		}
	}
}

void PackedQuatVector::UnpackQuats(Quaternionf *data)
{
	int indexPos = 0;
	int bitPos = 0;

	for(int i=0; i<m_NumItems; i++)
	{
		UInt32 flags = 0;
				
		int bits = 0;
		while(bits < 3)
		{
			flags |= (m_Data[indexPos] >> bitPos) << bits;
			int num = std::min( 3-bits, 8-bitPos);
			bitPos += num;
			bits += num;
			if(bitPos == 8)
			{
				indexPos++;
				bitPos = 0;
			}
		}
		flags &= 7;
		
				
		Quaternionf &q = data[i];
		float sum = 0;
		for(int j=0;j<4;j++)
		{
			if((flags&3) != j)
			{
				int bitSize = (((flags&3)+1)%4 == j)?9:10;
				UInt32 x = 0;
				
				bits = 0;
				while(bits < bitSize)
				{
					x |= (m_Data[indexPos] >> bitPos) << bits;
					int num = std::min( bitSize-bits, 8-bitPos);
					bitPos += num;
					bits += num;
					if(bitPos == 8)
					{
						indexPos++;
						bitPos = 0;
					}
				}
				x &= (1 << bitSize) - 1;
				q[j] = (x / (0.5 * ((1 << (bitSize)) - 1))) - 1;
				sum += sqr(q[j]);
			}
		}
		
		int lastComponent = flags&3;
		q[lastComponent] = FastSqrt(1 - sum);
		if(flags & 4)
			q[lastComponent] = -q[lastComponent];
	}
}

void CompressedMesh::Compress(Mesh &src, int compression)
{
	int numVertices = src.GetVertexCount();
	
	int vertexBits = 0;
	switch(compression)
	{
		case kMeshCompressionHigh: vertexBits = 10; break;
		case kMeshCompressionMed: vertexBits = 16; break;
		case kMeshCompressionLow: vertexBits = 20; break;
	}
	m_Vertices.PackFloats((float*)src.GetChannelPointer(kShaderChannelVertex), 3, src.GetStride (kShaderChannelVertex), numVertices, vertexBits, false);

	//Possible optimization: use Edgebreaker algorithm 
	//for 1.8 bits per triangle connectivity information
	//http://www.gvu.gatech.edu/~jarek/edgebreaker/eb/
	
	int numIndices = src.m_IndexBuffer.size();
	numIndices/=2;
		
	m_Triangles.PackInts<UInt16>((UInt16*)&src.m_IndexBuffer[0],numIndices);
	
	if(src.IsAvailable(kShaderChannelTexCoord0))
	{
		int uvBits = 0;
		switch(compression)
		{
			case kMeshCompressionHigh: uvBits = 8; break;
			case kMeshCompressionMed: uvBits = 10; break;
			case kMeshCompressionLow: uvBits = 16; break;
		}
		if(src.IsAvailable(kShaderChannelTexCoord1))
		{
			Vector2f *uv12 = new Vector2f[numVertices*2];
			src.ExtractUvArray(0, uv12);
			src.ExtractUvArray(1, uv12 + numVertices);
			m_UV.PackFloats(&uv12->x, 2, sizeof(Vector2f), numVertices*2, uvBits, true);
			delete[] uv12;
		}
		else
			m_UV.PackFloats((float*)src.GetChannelPointer (kShaderChannelTexCoord0), 2, src.GetStride (kShaderChannelTexCoord0), numVertices, uvBits, true);
	}
	else if(src.IsAvailable(kShaderChannelTexCoord1))
		ErrorString( "Mesh compression doesn't work on Meshes wich only have a UV1 channel but no UV0 channel. UVs will be dropped." );
		
	if(src.IsAvailable (kShaderChannelNormal))
	{
		int normalBits = 0;
		switch(compression)
		{
			case kMeshCompressionHigh: normalBits = 6; break;
			case kMeshCompressionMed: normalBits = 8; break;
			case kMeshCompressionLow: normalBits = 8; break;
		}

		float *normals = new float[numVertices*2];
		UInt32 *signs = new UInt32[numVertices];
		StrideIterator<Vector3f> n = src.GetNormalBegin ();
		for(int i=0;i<numVertices; ++i, ++n)
		{
			normals[i*2+0] = n->x;
			normals[i*2+1] = n->y;
			signs[i] = n->z>0?1:0;
		}
		m_Normals.PackFloats(normals, 2, sizeof (float) * 2, numVertices, normalBits, false);
		m_NormalSigns.PackInts(signs, numVertices);	
		delete[] normals;
		delete[] signs;
	}
	
	if(src.IsAvailable (kShaderChannelTangent))
	{
		int normalBits = 0;
		switch(compression)
		{
			case kMeshCompressionHigh: normalBits = 6; break;
			case kMeshCompressionMed: normalBits = 8; break;
			case kMeshCompressionLow: normalBits = 8; break;
		}

		float *tangents = new float[numVertices*2];
		UInt32 *signs = new UInt32[numVertices*2];
		StrideIterator<Vector4f> t = src.GetTangentBegin ();
		for(int i=0;i<numVertices; ++i, ++t)
		{
			tangents[i*2+0] = t->x;
			tangents[i*2+1] = t->y;
			signs[i*2+0] = t->z>0?1:0;
			signs[i*2+1] = t->w>0?1:0;
		}
		m_Tangents.PackFloats(tangents, 2, sizeof (float) * 2, numVertices, normalBits, false);	
		m_TangentSigns.PackInts(signs, numVertices*2);	
		delete[] tangents;
		delete[] signs;
	}

	// TODO: do an actual compression
	if(src.IsAvailable (kShaderChannelColor))
	{
		dynamic_array<UInt32> tempColors (numVertices, kMemTempAlloc);
		std::transform (src.GetColorBegin (), src.GetColorEnd (), tempColors.begin (), OpColorRGBA32ToUInt32());
		m_Colors.PackInts<UInt32> (tempColors.data (), tempColors.size ());
	}

	BoneInfluence* influence = src.GetBoneWeights();
	if(influence)
	{
		UInt32 *weights = new UInt32[numVertices*3];
		UInt32 *indices = new UInt32[numVertices*4];
		int weightPos = 0;
		int boneIndexPos = 0;
		for(int i=0;i<numVertices;i++)
		{
			int j;
			int sum = 0;
			
			//As all four bone weights always add up to 1, we can always calculate the fourth one
			// by subtracting the other three from 1. So we don't need to store it.

			//Furthermore, once the weights we stored add up to 1, we don't need to store further
			//weights or indices, as these will necessarily be zero. This is often the case, as many
			//vertices have only the first weight set to one, and all others to zero.
			
			//find last non-zero entry -- we don't need to store those after this.
			int lastNonZero;
			for(lastNonZero=3;lastNonZero>0&&influence[i].weight[lastNonZero]==0;lastNonZero--)
			{}
			
						
			for(j=0;j<3 && j<=lastNonZero && sum<31;j++)
			{
				weights[weightPos] = UInt32(influence[i].weight[j] * 31);
				indices[boneIndexPos++] = influence[i].boneIndex[j];
				sum += weights[weightPos++];
			}
			if(lastNonZero<3)
			{
				//we stored less then 3 weights, but they don't add up to one, due to quantization
				//inprecision.
				//Add the difference, so the math works out on decompression.
				if(sum<31)
					weights[weightPos-1] += 31-sum;
			}
			
			//we stored three weights, but they don't add up to one. we don't need to store the fourth weight
			//(as it can be calculated from the other three), but we need the bone index.
			else if(sum<31)
				indices[boneIndexPos++] = influence[i].boneIndex[j];				
		}
		
		m_Weights.PackInts(weights, weightPos);	
		m_BoneIndices.PackInts(indices, boneIndexPos);

		delete[] weights;
		delete[] indices;
	}
}

void CompressedMesh::Decompress(Mesh &src)
{	
	int numIndices = m_Triangles.Count();
	src.m_IndexBuffer.resize(numIndices * 2);
	m_Triangles.UnpackInts<UInt16>((UInt16*)&src.m_IndexBuffer[0]);
	
	int numVertices = m_Vertices.Count()/3;
	unsigned decompressedFormat = 0;
	if (m_Vertices.Count ()) decompressedFormat |= VERTEX_FORMAT1(Vertex);
	if (m_Normals.Count()) decompressedFormat |= VERTEX_FORMAT1(Normal);
	if (m_UV.Count()) decompressedFormat |= VERTEX_FORMAT1(TexCoord0);
	if (m_UV.Count() == numVertices * 4) decompressedFormat |= VERTEX_FORMAT1(TexCoord1);
	if (m_Tangents.Count()) decompressedFormat |= VERTEX_FORMAT1(Tangent);
	if (m_Colors.Count()) decompressedFormat |= VERTEX_FORMAT1(Color);
	
	src.ResizeVertices(numVertices, decompressedFormat);
	Assert (src.GetVertexCount () == numVertices);

	m_Vertices.UnpackFloats((float*)src.GetChannelPointer (kShaderChannelVertex), 3, src.GetStride (kShaderChannelVertex));
		
	if(m_UV.Count())
	{
		m_UV.UnpackFloats((float*)src.GetChannelPointer (kShaderChannelTexCoord0), 2, src.GetStride (kShaderChannelTexCoord0), 0, numVertices);

		if(m_UV.Count()==numVertices * 4)
		{
			m_UV.UnpackFloats((float*)src.GetChannelPointer (kShaderChannelTexCoord1), 2, src.GetStride (kShaderChannelTexCoord1), numVertices*2, numVertices);
		}
	}
	
	// TODO: This never gets written. Unity 3.4 and 3.5 never wrote this data.
	// Most likely no version ever did. Remove code and bindpose serialization.
	if(m_BindPoses.Count())
	{
		src.m_Bindpose.resize_initialized(m_BindPoses.Count()/16);
		m_BindPoses.UnpackFloats(src.m_Bindpose[0].m_Data, 16, sizeof(float) * 16);
	}

	if(m_Normals.Count())
	{
		float *normalData = new float[m_Normals.Count()];
		UInt32 *signs = new UInt32[m_NormalSigns.Count()];
		
		m_Normals.UnpackFloats(normalData, 2, sizeof(float) * 2);
		m_NormalSigns.UnpackInts(signs);

		StrideIterator<Vector3f> n = src.GetNormalBegin ();
		for(int i=0;i<m_Normals.Count()/2; ++i, ++n)
		{
			n->x = normalData[i*2+0];
			n->y = normalData[i*2+1];
			float zsqr = 1 - sqr(n->x) - sqr(n->y);
			if(zsqr >= 0)
				n->z = FastSqrt( zsqr );
			else
			{
				n->z = 0;
				*n = Normalize(*n);
			}
			if(signs[i]==0)
				n->z = -n->z;
		}
				
		delete[] normalData;
		delete[] signs;
	} 

	if(m_Tangents.Count())
	{
		float *tangentData = new float[m_Tangents.Count()];
		UInt32 *signs = new UInt32[m_TangentSigns.Count()];

		m_Tangents.UnpackFloats(tangentData, 2, sizeof(float) * 2);
		m_TangentSigns.UnpackInts(signs);
		
		StrideIterator<Vector4f> t = src.GetTangentBegin ();
		for(int i=0;i<m_Tangents.Count()/2; ++i, ++t)
		{
			t->x = tangentData[i*2+0];
			t->y = tangentData[i*2+1];
			float zsqr = 1-sqr(tangentData[i*2+0])-sqr(tangentData[i*2+1]);
			if(zsqr >= 0.0f)
				t->z = FastSqrt( zsqr );
			else
			{
				t->z = 0;
				*(Vector3f*)(&*t) = Normalize(*(Vector3f*)(&*t));
			}
			if(signs[i*2+0]==0)
				t->z = -t->z;

			t->w = signs[i*2+1]?1.0:-1.0;
		}
				
		delete[] tangentData;
		delete[] signs;
	}
	
	// TODO: do an actual compression
	if (m_Colors.Count())
	{
		dynamic_array<UInt32> tempColors (m_Colors.Count (), kMemTempAlloc);
		m_Colors.UnpackInts<UInt32> (tempColors.data ());
		Assert (tempColors.size () == src.GetVertexCount ());
		strided_copy ((ColorRGBA32*)tempColors.begin (), (ColorRGBA32*)tempColors.end (), src.GetColorBegin ());
	}

	if(m_Weights.Count())
	{
		UInt32 *weights = new UInt32[m_Weights.Count()];
		m_Weights.UnpackInts(weights);
		UInt32 *boneIndices = new UInt32[m_BoneIndices.Count()];
		m_BoneIndices.UnpackInts(boneIndices);
		src.m_Skin.resize_uninitialized(numVertices);
		int bonePos = 0;
		int boneIndexPos = 0;
		int j=0;
		int sum = 0;
		
		for(int i=0;i<m_Weights.Count();i++)
		{
			//read bone index and weight.
			src.m_Skin[bonePos].weight[j] = weights[i]/31.0;
			src.m_Skin[bonePos].boneIndex[j] = boneIndices[boneIndexPos++];
			j++;
			sum += weights[i];
			
			//the weights add up to one. fill the rest for this vertex with zero, and continue with next one.
			if(sum >= 31)
			{
				for(;j<4;j++)
				{
					src.m_Skin[bonePos].weight[j] = 0;
					src.m_Skin[bonePos].boneIndex[j] = 0;
				}
				bonePos++;
				j = 0;
				sum = 0;
			}
			//we read three weights, but they don't add up to one. calculate the fourth one, and read
			//missing bone index. continue with next vertex.
			else if(j==3)
			{
				src.m_Skin[bonePos].weight[j] = (31-sum)/31.0;
				src.m_Skin[bonePos].boneIndex[j] = boneIndices[boneIndexPos++];
				bonePos++;
				j = 0;
				sum = 0;
			}
		}
				
		delete[] weights;
		delete[] boneIndices;
	}
}

template <class T> void CompressedAnimationCurve::CompressTimeKeys(AnimationCurveTpl<T> &src)
{
	int numKeys = src.GetKeyCount();
	
	float minTime=0;
	for(int i=0;i<numKeys;i++)
	{
		float t = src.GetKey(i).time;
		if(t < minTime)
		{
			//negative time key. offset all keys by this, so math doesn't break - but it's still wrong.
			minTime = t;
		}
	}
	
	
	UInt32 *times = new UInt32[numKeys];
	UInt32 t=0;
	for(int i=0;i<numKeys;i++)
	{
		times[i] = UInt32((src.GetKey(i).time - minTime) * 100);
		times[i] -= t;
		t += times[i];
	}
	
	m_Times.PackInts(times, numKeys);		
	
	delete[] times;
}

template <class T> void CompressedAnimationCurve::DecompressTimeKeys(AnimationCurveTpl<T> &src)
{
	int numKeys = m_Times.Count();
	UInt32 *times = new UInt32[numKeys];
	m_Times.UnpackInts(times);
	
	UInt32 t=0;

	src.ResizeUninitialized(numKeys);
	
	for(int i=0;i<numKeys;i++)
	{
		t+=times[i];
		src.GetKey(i).time = t*0.01;
	}	
	delete[] times;
}

void CompressedAnimationCurve::CompressQuatCurve(AnimationClip::QuaternionCurve &src)
{
	CompressTimeKeys(src.curve);
	int numKeys = src.curve.GetKeyCount();
	
	Quaternionf *qkeys = new Quaternionf[numKeys];		
	for(int i=0;i<numKeys;i++)
		qkeys[i] = src.curve.GetKey(i).value;
	m_Values.PackQuats(qkeys, numKeys);		
	
	delete[] qkeys;
	
	bool same = true;

	for(int i=0;i<numKeys && same;i++)
	{
		Quaternionf &q1 = src.curve.GetKey(i).inSlope;
		Quaternionf &q2 = src.curve.GetKey(i).inSlope;
		if(q1.x!=q2.x)
			same = false;
		if(q1.y!=q2.y)
			same = false;
		if(q1.z!=q2.z)
			same = false;
		if(q1.w!=q2.w)
			same = false;
	}

	float *keys = new float[numKeys*8];
	for(int i=0;i<numKeys;i++)
	{
		Quaternionf q = src.curve.GetKey(i).inSlope;
		keys[i*4+0] = q.x;
		keys[i*4+1] = q.y;
		keys[i*4+2] = q.z;
		keys[i*4+3] = q.w;
		q = src.curve.GetKey(i).outSlope;
		keys[(i+numKeys)*4+0] = q.x;
		keys[(i+numKeys)*4+1] = q.y;
		keys[(i+numKeys)*4+2] = q.z;
		keys[(i+numKeys)*4+3] = q.w;
	}
	
	//if in and out slopes are all the same, pack only the first of the two.
	if(same)
		m_Slopes.PackFloats(keys, 1, sizeof(float), numKeys * 4, 6, false);
	else
		m_Slopes.PackFloats(keys, 1, sizeof(float), numKeys * 8, 6, false);
		
	delete[] keys;
	
	m_PreInfinity = src.curve.GetPreInfinityInternal();
	m_PostInfinity = src.curve.GetPostInfinityInternal();
	m_Path = src.path;
}

void CompressedAnimationCurve::DecompressQuatCurve(AnimationClip::QuaternionCurve &src)
{
	DecompressTimeKeys(src.curve);
	int numKeys = m_Values.Count();
	
	Quaternionf *qkeys = new Quaternionf[numKeys];		
	m_Values.UnpackQuats(qkeys);	
	for(int i=0;i<numKeys;i++)
		src.curve.GetKey(i).value = qkeys[i];
	delete[] qkeys;

	float *keys = new float[numKeys*8];
	m_Slopes.UnpackFloats(keys, 1, sizeof(float));
	
	//are there seperate in and out slopes?
	int offs = 0;
	if(m_Slopes.Count() == numKeys*8)
		offs = numKeys;
	for(int i=0;i<numKeys;i++)
	{
		src.curve.GetKey(i).inSlope.x = keys[i*4+0];
		src.curve.GetKey(i).inSlope.y = keys[i*4+1];
		src.curve.GetKey(i).inSlope.z = keys[i*4+2];
		src.curve.GetKey(i).inSlope.w = keys[i*4+3];
		src.curve.GetKey(i).outSlope.x = keys[(i+offs)*4+0];
		src.curve.GetKey(i).outSlope.y = keys[(i+offs)*4+1];
		src.curve.GetKey(i).outSlope.z = keys[(i+offs)*4+2];
		src.curve.GetKey(i).outSlope.w = keys[(i+offs)*4+3];
	}
	delete[] keys;
	
	src.curve.SetPreInfinityInternal( m_PreInfinity );
	src.curve.SetPostInfinityInternal( m_PostInfinity );
	src.path = m_Path;
}
