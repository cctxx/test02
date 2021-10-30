#include "UnityPrefix.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/mecanim/animation/blendtree.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/Utilities/dynamic_array.h"

namespace mecanim
{

namespace animation
{
	void GetWeightsFreeformDirectional (const Blend2dDataConstant& blendConstant,
										float* weightArray, int* cropArray, Vector2f* workspaceBlendVectors,
										float blendValueX, float blendValueY, bool preCompute);
	void GetWeightsFreeformCartesian   (const Blend2dDataConstant& blendConstant,
										float* weightArray, int* cropArray, Vector2f* workspaceBlendVectors,
										float blendValueX, float blendValueY, bool preCompute);
	
	void PrecomputeFreeform (int type, Blend2dDataConstant& out, memory::Allocator& alloc)
	{
		const Vector2f* positionArray = out.m_ChildPositionArray.Get();
		mecanim::uint32_t count = out.m_ChildCount;
		float* constantMagnitudes = out.m_ChildMagnitudeArray.Get();
		Vector2f* constantChildPairVectors = out.m_ChildPairVectorArray.Get();
		float* constantChildPairAvgMagInv = out.m_ChildPairAvgMagInvArray.Get();
		MotionNeighborList* constantChildNeighborLists = out.m_ChildNeighborListArray.Get();
		
		if (type == 2)
		{
			for (int i=0; i<count; i++)
				constantMagnitudes[i] = Magnitude (positionArray[i]);
			
			for (int i=0; i<count; i++)
			{
				for (int j=0; j<count; j++)
				{
					int pairIndex = i + j*count;
					
					// Calc avg magnitude for pair
					float magSum = constantMagnitudes[j] + constantMagnitudes[i];
					if (magSum > 0)
						constantChildPairAvgMagInv[pairIndex] = 2.0f / magSum;
					else
						constantChildPairAvgMagInv[pairIndex] = 2.0f / magSum;
					
					// Calc mag of vector and divide by avg magnitude
					float mag = (constantMagnitudes[j] - constantMagnitudes[i]) * constantChildPairAvgMagInv[pairIndex];
					
					if (constantMagnitudes[j] == 0 || constantMagnitudes[i] == 0)
						constantChildPairVectors[pairIndex] = Vector2f (0, mag);
					else
					{
						float angle = Angle (positionArray[i], positionArray[j]);
						if (positionArray[i].x * positionArray[j].y - positionArray[i].y * positionArray[j].x < 0)
							angle = -angle;
						constantChildPairVectors[pairIndex] = Vector2f (angle, mag);
					}
				}
			}
		}
		else if (type == 3)
		{
			for (int i=0; i<count; i++)
			{
				for (int j=0; j<count; j++)
				{
					int pairIndex = i + j*count;
					constantChildPairAvgMagInv[pairIndex] = 1 / SqrMagnitude (positionArray[j] - positionArray[i]);
					constantChildPairVectors[pairIndex] = positionArray[j] - positionArray[i];
				}
			}
		}
		
		float* weightArray;
		ALLOC_TEMP (weightArray, float, count);
		
		int* cropArray;
		ALLOC_TEMP (cropArray, int, count);
		
		Vector2f* workspaceBlendVectors;
		ALLOC_TEMP (workspaceBlendVectors, Vector2f, count);
		
		bool* neighborArray;
		ALLOC_TEMP (neighborArray, bool, count*count);
		for (int c=0; c<count*count; c++)
			neighborArray[c] = false;
		
		float minX = 10000.0f;
		float maxX = -10000.0f;
		float minY = 10000.0f;
		float maxY = -10000.0f;
		for (int c=0; c<count; c++)
		{
			minX = min (minX, positionArray[c].x);
			maxX = max (maxX, positionArray[c].x);
			minY = min (minY, positionArray[c].y);
			maxY = max (maxY, positionArray[c].y);
		}
		float xRange = (maxX - minX) * 0.5f;
		float yRange = (maxY - minY) * 0.5f;
		minX -= xRange;
		maxX += xRange;
		minY -= yRange;
		maxY += yRange;
		
		for (int i=0; i<=100; i++)
		{
			for (int j=0; j<=100; j++)
			{
				float x = i*0.01f;
				float y = j*0.01f;
				if (type == 2)
					GetWeightsFreeformDirectional (out, weightArray, cropArray, workspaceBlendVectors, minX * (1-x) + maxX * x, minY * (1-y) + maxY * y, true);
				else if (type == 3)
					GetWeightsFreeformCartesian (out, weightArray, cropArray, workspaceBlendVectors, minX * (1-x) + maxX * x, minY * (1-y) + maxY * y, true);
				for (int c=0; c<count; c++)
					if (cropArray[c] >= 0)
						neighborArray[c * count + cropArray[c]] = true;
			}
		}
		for (int c=0; c<count; c++)
		{
			dynamic_array<int> nList;
			for (int d=0; d<count; d++)
				if (neighborArray[c * count + d])
					nList.push_back (d);
			
			constantChildNeighborLists[c].m_Count = nList.size ();
			constantChildNeighborLists[c].m_NeighborArray = alloc.ConstructArray<mecanim::uint32_t>(nList.size ());
			
			for (int d=0; d<nList.size (); d++)
				constantChildNeighborLists[c].m_NeighborArray[d] = nList[d];
		}
	}
	
	void GetAllBlendValue(uint32_t nodeIndex, OffsetPtr<BlendTreeNodeConstant>* const allTreeNodes,  dynamic_array<int> &arBlendValueIds)
	{
		BlendTreeNodeConstant* const currentNode = allTreeNodes[nodeIndex].Get();

		if(currentNode->m_BlendEventID != -1)
		{
			dynamic_array<int>::const_iterator it = std::find(arBlendValueIds.begin(), arBlendValueIds.end(), currentNode->m_BlendEventID);
			if(it == arBlendValueIds.end())
				arBlendValueIds.push_back(currentNode->m_BlendEventID);

			if(currentNode->m_BlendType >= 1)
			{
				dynamic_array<int>::const_iterator itY = std::find(arBlendValueIds.begin(), arBlendValueIds.end(), currentNode->m_BlendEventYID);
				if(itY ==  arBlendValueIds.end())
					arBlendValueIds.push_back(currentNode->m_BlendEventYID);
			}

			for(mecanim::uint32_t i = 0 ; i < currentNode->m_ChildCount; i++)
			{
				GetAllBlendValue(currentNode->m_ChildIndices[i], allTreeNodes, arBlendValueIds);
			}
		}

	}
	void GetAllBlendValue(BlendTreeConstant* const constant, dynamic_array<int> &arBlendValueIds)
	{
		if(constant->m_NodeCount > 0)
			GetAllBlendValue(0, constant->m_NodeArray.Get(), arBlendValueIds);
	}


	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t blendValueID, uint32_t childCount, uint32_t* childIndices, float* blendTreeThresholdArray,  memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeNodeConstant);

		BlendTreeNodeConstant *blendTreeNodeConstant	= alloc.Construct<BlendTreeNodeConstant>();

		blendTreeNodeConstant->m_BlendEventID	= blendValueID;
		blendTreeNodeConstant->m_ChildCount		= childCount;

		blendTreeNodeConstant->m_ChildIndices	= alloc.ConstructArray<uint32_t>(childCount);
		memcpy(&blendTreeNodeConstant->m_ChildIndices[0], &childIndices[0], sizeof(uint32_t)*childCount);		

		// Setup blend 1d data constant
		blendTreeNodeConstant->m_BlendType = 0;
		blendTreeNodeConstant->m_Blend1dData = alloc.Construct<Blend1dDataConstant>();
		Blend1dDataConstant& data = *blendTreeNodeConstant->m_Blend1dData;
		
		// Populate blend 1d data constant
		data.m_ChildCount = childCount;
		data.m_ChildThresholdArray = alloc.ConstructArray<float>(data.m_ChildCount);
		memcpy(&data.m_ChildThresholdArray[0], &blendTreeThresholdArray[0], sizeof(float)*childCount);

		return blendTreeNodeConstant ;
	}
	
	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t blendValueID, uint32_t blendValueYID, int blendType, uint32_t childCount, uint32_t* childIndices, Vector2f* blendTreePositionArray, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeNodeConstant);

		BlendTreeNodeConstant *blendTreeNodeConstant	= alloc.Construct<BlendTreeNodeConstant>();
		
		blendTreeNodeConstant->m_BlendEventID	= blendValueID;
		blendTreeNodeConstant->m_BlendEventYID	= blendValueYID;
		blendTreeNodeConstant->m_ChildCount		= childCount;
		
		blendTreeNodeConstant->m_ChildIndices	= alloc.ConstructArray<uint32_t>(childCount);		
		memcpy(&blendTreeNodeConstant->m_ChildIndices[0], &childIndices[0], sizeof(uint32_t)*childCount);
		
		// Setup blend 2d data constant
		blendTreeNodeConstant->m_BlendType = blendType;
		blendTreeNodeConstant->m_Blend2dData = alloc.Construct<Blend2dDataConstant>();
		Blend2dDataConstant& data = *blendTreeNodeConstant->m_Blend2dData;
		
		// Populate blend 2d data constant
		data.m_ChildCount = childCount;
		data.m_ChildPositionArray = alloc.ConstructArray<Vector2f>(data.m_ChildCount);
		memcpy(&data.m_ChildPositionArray[0], &blendTreePositionArray[0], sizeof(Vector2f)*childCount);
		
		if (blendType == 2 || blendType == 3)
		{
			// Populate blend 2d precomputed data for type 2 or 3
			if (blendType == 2)
			{
				data.m_ChildMagnitudeCount		= childCount;
				data.m_ChildMagnitudeArray		= alloc.ConstructArray<float>(data.m_ChildMagnitudeCount);
			}
			data.m_ChildPairAvgMagInvCount	= childCount * childCount;
			data.m_ChildPairVectorCount		= childCount * childCount;
			data.m_ChildNeighborListCount	= childCount;
			data.m_ChildPairAvgMagInvArray	= alloc.ConstructArray<float>(data.m_ChildPairAvgMagInvCount);
			data.m_ChildPairVectorArray		= alloc.ConstructArray<Vector2f>(data.m_ChildPairVectorCount);
			data.m_ChildNeighborListArray	= alloc.ConstructArray<MotionNeighborList>(data.m_ChildNeighborListCount);
			PrecomputeFreeform (blendType, data, alloc);
		}
		
		return blendTreeNodeConstant;
	}

	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t clipID, float duration, bool mirror, float cycle, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeNodeConstant);
		BlendTreeNodeConstant *blendTreeNodeConstant = alloc.Construct<BlendTreeNodeConstant>();

		blendTreeNodeConstant->m_ChildCount = 0;
		blendTreeNodeConstant->m_ClipID = clipID;
		blendTreeNodeConstant->m_Duration = duration;
		blendTreeNodeConstant->m_Mirror = mirror;
		blendTreeNodeConstant->m_CycleOffset = cycle;

		return blendTreeNodeConstant ;
	}

	BlendTreeConstant* CreateBlendTreeConstant(BlendTreeNodeConstant** nodeArray, uint32_t nodeCount, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeConstant);

		BlendTreeConstant *blendTreeConstant	= alloc.Construct<BlendTreeConstant>();

		blendTreeConstant->m_NodeCount	= nodeCount;
		blendTreeConstant->m_NodeArray	= alloc.ConstructArray< OffsetPtr<BlendTreeNodeConstant> >(nodeCount);		
		
		uint32_t i;
		for(i = 0; i < nodeCount; i++)
			blendTreeConstant->m_NodeArray[i] = nodeArray[i];

		dynamic_array<int> blendValueIds;	
		GetAllBlendValue(blendTreeConstant, blendValueIds);

		blendTreeConstant->m_BlendEventArrayConstant = CreateValueArrayConstant(kFloatType, blendValueIds.size(), alloc);

		for(i = 0;  i < blendValueIds.size(); i++)
		{
			blendTreeConstant->m_BlendEventArrayConstant->m_ValueArray[i].m_ID = blendValueIds[i];
		}

		return blendTreeConstant ;
	}

	BlendTreeConstant* CreateBlendTreeConstant(uint32_t clipID, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeConstant);

		BlendTreeConstant *blendTreeConstant	= alloc.Construct<BlendTreeConstant>();
		blendTreeConstant->m_NodeCount	= 1;
		blendTreeConstant->m_NodeArray	= alloc.ConstructArray<  OffsetPtr<BlendTreeNodeConstant> >(1);

		blendTreeConstant->m_NodeArray[0] = CreateBlendTreeNodeConstant(clipID,1.0f,false,0,alloc);

		return blendTreeConstant;

	}

	void DestroyBlendTreeNodeConstant(BlendTreeNodeConstant * constant, memory::Allocator& alloc)
	{
		alloc.Deallocate(constant->m_ChildIndices);
		
		if (!constant->m_Blend1dData.IsNull ())
		{
			alloc.Deallocate(constant->m_Blend1dData->m_ChildThresholdArray);
		}
		
		if (!constant->m_Blend2dData.IsNull ())
		{
			alloc.Deallocate(constant->m_Blend2dData->m_ChildPositionArray);
			alloc.Deallocate(constant->m_Blend2dData->m_ChildMagnitudeArray);
			alloc.Deallocate(constant->m_Blend2dData->m_ChildPairVectorArray);
			alloc.Deallocate(constant->m_Blend2dData->m_ChildPairAvgMagInvArray);
			
			if (!constant->m_Blend2dData->m_ChildNeighborListArray.IsNull ())
			{
				for (int i=0; i<constant->m_Blend2dData->m_ChildNeighborListCount; i++)
					alloc.Deallocate(constant->m_Blend2dData->m_ChildNeighborListArray[i].m_NeighborArray);
				alloc.Deallocate(constant->m_Blend2dData->m_ChildNeighborListArray);
			}
		}
		
		alloc.Deallocate(constant);	
	}
	
	void DestroyBlendTreeConstant(BlendTreeConstant * constant, memory::Allocator& alloc)
	{
		if(constant)
		{
			if(!constant->m_BlendEventArrayConstant.IsNull())
			{
				DestroyValueArrayConstant(constant->m_BlendEventArrayConstant.Get(), alloc);
			}

			for(uint32_t i = 0; i < constant->m_NodeCount; i++)
			{
				DestroyBlendTreeNodeConstant(constant->m_NodeArray[i].Get(), alloc);
			}

			alloc.Deallocate(constant->m_NodeArray);
			alloc.Deallocate(constant);
		}

	}

	BlendTreeMemory* CreateBlendTreeMemory(BlendTreeConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeMemory);
		BlendTreeMemory *blendTreeMemory = alloc.Construct<BlendTreeMemory>();

		blendTreeMemory->m_NodeCount = GetLeafCount(*constant);
		blendTreeMemory->m_NodeDurationArray = alloc.ConstructArray<float>(blendTreeMemory->m_NodeCount);		

		return blendTreeMemory ;
	}

	void DestroyBlendTreeMemory(BlendTreeMemory *memory, memory::Allocator& alloc)
	{
		if(memory)
		{			
			alloc.Deallocate(memory->m_NodeDurationArray);
			alloc.Deallocate(memory);
		}
	}

	BlendTreeInput* CreateBlendTreeInput(BlendTreeConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeInput);

		BlendTreeInput *blendTreeInput = alloc.Construct<BlendTreeInput>();
		
		if(!constant->m_BlendEventArrayConstant.IsNull())
			blendTreeInput->m_BlendValueArray = CreateValueArray(constant->m_BlendEventArrayConstant.Get(), alloc);

		return blendTreeInput ;
	}
	
	void DestroyBlendTreeInput(BlendTreeInput * input, memory::Allocator& alloc)
	{	
		if(input)
		{
			if(input->m_BlendValueArray)
			{
				DestroyValueArray(input->m_BlendValueArray, alloc);
			}

			alloc.Deallocate(input);
		}
	}

	BlendTreeOutput* CreateBlendTreeOutput(BlendTreeConstant const* constant, uint32_t maxBlendedClip, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeOutput);
		BlendTreeOutput *blendTreeOutput = alloc.Construct<BlendTreeOutput>();

		blendTreeOutput->m_MaxBlendedClip = maxBlendedClip;
		blendTreeOutput->m_OutputBlendArray = alloc.ConstructArray< BlendTreeNodeOutput >(maxBlendedClip);		

		return blendTreeOutput ;
	}

	void DestroyBlendTreeOutput(BlendTreeOutput *output, memory::Allocator& alloc)
	{
		if(output)
		{			
			alloc.Deallocate(output->m_OutputBlendArray);
			alloc.Deallocate(output);
		}
	}

	BlendTreeWorkspace* CreateBlendTreeWorkspace(BlendTreeConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendTreeWorkspace);

		BlendTreeWorkspace *blendTreeWorkspace = alloc.Construct<BlendTreeWorkspace>();
		blendTreeWorkspace->m_BlendArray	= alloc.ConstructArray<float>(constant->m_NodeCount);
		// Optimize later to only have room for worst case number of immediate children instead of m_NodeCount:
		blendTreeWorkspace->m_TempWeightArray	= alloc.ConstructArray<float>(constant->m_NodeCount);
		blendTreeWorkspace->m_TempCropArray	= alloc.ConstructArray<int>(constant->m_NodeCount);
		blendTreeWorkspace->m_ChildInputVectorArray	= alloc.ConstructArray<Vector2f>(constant->m_NodeCount);

		return blendTreeWorkspace ;
	}

	void DestroyBlendTreeWorkspace(BlendTreeWorkspace * workspace, memory::Allocator& alloc)
	{
		if(workspace != 0)
		{
			alloc.Deallocate(workspace->m_BlendArray);
			alloc.Deallocate(workspace->m_TempWeightArray);
			alloc.Deallocate(workspace->m_TempCropArray);
			alloc.Deallocate(workspace->m_ChildInputVectorArray);
			alloc.Deallocate(workspace);
		}		
	}

	float WeightForIndex( const float* thresholdArray, mecanim::uint32_t count, mecanim::uint32_t index, float blend)
	{		
		if( blend >= thresholdArray[index])
		{
			if(index+1 == count)
			{
				return 1.0f;
			}
			else if(thresholdArray[index+1] < blend)
			{
				return 0.0f;
			}
			else
			{
				if(thresholdArray[index]-thresholdArray[index+1] != 0)
				{
					return (blend - thresholdArray[index+1]) / (thresholdArray[index]-thresholdArray[index+1]);
				}
				else
				{
					return thresholdArray[index];
				}
				
			}
		}
		else
		{
			if(index == 0)
			{
				return 1.0f;
			}
			else if(thresholdArray[index-1] > blend)
			{
				return 0.0f;
			}
			else
			{
				if(( thresholdArray[index]-thresholdArray[index-1]) != 0)
				{
					return (blend - thresholdArray[index-1]) / (thresholdArray[index]-thresholdArray[index-1]);
				}
				else
				{
					return thresholdArray[index];
				}
			}
		}

	}

	void GetWeightsSimpleDirectional (const Blend2dDataConstant& blendConstant,
									  float* weightArray, int* cropArray, Vector2f* workspaceBlendVectors,
									  float blendValueX, float blendValueY, bool preCompute = false)
	{
		// Get constants
		const Vector2f* positionArray = blendConstant.m_ChildPositionArray.Get();
		mecanim::uint32_t count = blendConstant.m_ChildCount;
		
		if (weightArray == NULL || positionArray == NULL)
			return;

		// Initialize all weights to 0
		for (int i=0; i<count; i++)
			weightArray[i] = 0;
		
		// Handle fallback
		if (count < 2)
		{
			if (count == 1)
				weightArray[0] = 1;
			return;
		}
		
		Vector2f blendPosition = Vector2f (blendValueX, blendValueY);
		
		// Handle special case when sampled ecactly in the middle
		if (blendPosition == Vector2f::zero)
		{
			// If we have a center motion, give that one all the weight
			for (int i=0; i<count; i++)
			{
				if (positionArray[i] == Vector2f::zero)
				{
					weightArray[i] = 1;
					return;
				}
			}
			
			// Otherwise divide weight evenly
			float sharedWeight = 1.0f / count;
			for (int i=0; i<count; i++)
				weightArray[i] = sharedWeight;
			return;
		}
		
		int indexA = -1;
		int indexB = -1;
		int indexCenter = -1;
		float maxDotForNegCross = -100000.0f;
		float maxDotForPosCross = -100000.0f;
		for (int i=0; i<count; i++)
		{
			if (positionArray[i] == Vector2f::zero)
			{
				if (indexCenter >= 0)
					return;
				indexCenter = i;
				continue;
			}
			Vector2f posNormalized = Normalize (positionArray[i]);
			float dot = Dot (posNormalized, blendPosition);
			float cross = posNormalized.x * blendPosition.y - posNormalized.y * blendPosition.x;
			if (cross > 0)
			{
				if (dot > maxDotForPosCross)
				{
					maxDotForPosCross = dot;
					indexA = i;
				}
			}
			else
			{
				if (dot > maxDotForNegCross)
				{
					maxDotForNegCross = dot;
					indexB = i;
				}
			}
		}
		
		float centerWeight = 0;
		
		if (indexA < 0 || indexB < 0)
		{
			// Fallback if sampling point is not inside a triangle
			centerWeight = 1;
		}
		else
		{
			Vector2f a = positionArray[indexA];
			Vector2f b = positionArray[indexB];
			
			// Calculate weights using barycentric coordinates
			// (formulas from http://en.wikipedia.org/wiki/Barycentric_coordinate_system_%28mathematics%29 )
			float det = b.y*a.x - b.x*a.y;        // Simplified from: (b.y-0)*(a.x-0) + (0-b.x)*(a.y-0);
			float wA = (b.y*blendValueX - b.x*blendValueY) / det; // Simplified from: ((b.y-0)*(l.x-0) + (0-b.x)*(l.y-0)) / det;
			float wB = (a.x*blendValueY - a.y*blendValueX) / det; // Simplified from: ((0-a.y)*(l.x-0) + (a.x-0)*(l.y-0)) / det;
			centerWeight = 1 - wA - wB;
			
			// Clamp to be inside triangle
			if (centerWeight < 0)
			{
				centerWeight = 0;
				float sum = wA + wB;
				wA /= sum;
				wB /= sum;
			}
			else if (centerWeight > 1)
			{
				centerWeight = 1;
				wA = 0;
				wB = 0;
			}
			
			// Give weight to the two vertices on the periphery that are closest
			weightArray[indexA] = wA;
			weightArray[indexB] = wB;
		}
		
		if (indexCenter >= 0)
		{
			weightArray[indexCenter] = centerWeight;
		}
		else
		{
			// Give weight to all children when input is in the center
			float sharedWeight = 1.0f / count;
			for (int i=0; i<count; i++)
				weightArray[i] += sharedWeight * centerWeight;
		}
	}
	
	const float kInversePI = 1 / kPI;
	float GetWeightFreeformDirectional (const Blend2dDataConstant& blendConstant, Vector2f* workspaceBlendVectors, int i, int j, Vector2f blendPosition)
	{
		int pairIndex = i + j*blendConstant.m_ChildCount;
		Vector2f vecIJ = blendConstant.m_ChildPairVectorArray[pairIndex];
		Vector2f vecIO = workspaceBlendVectors[i];
		vecIO.y *= blendConstant.m_ChildPairAvgMagInvArray[pairIndex];
		
		if (blendConstant.m_ChildPositionArray[i] == Vector2f::zero)
			vecIJ.x = workspaceBlendVectors[j].x;
		else if (blendConstant.m_ChildPositionArray[j] == Vector2f::zero)
			vecIJ.x = workspaceBlendVectors[i].x;
		else if (vecIJ.x == 0 || blendPosition == Vector2f::zero)
			vecIO.x = vecIJ.x;
		
		return 1 - Dot (vecIJ, vecIO) / SqrMagnitude (vecIJ);
	}
	
	void GetWeightsFreeformDirectional (const Blend2dDataConstant& blendConstant,
										float* weightArray, int* cropArray, Vector2f* workspaceBlendVectors,
										float blendValueX, float blendValueY, bool preCompute = false)
	{
		// Get constants
		const Vector2f* positionArray = blendConstant.m_ChildPositionArray.Get();
		mecanim::uint32_t count = blendConstant.m_ChildCount;
		const float* constantMagnitudes = blendConstant.m_ChildMagnitudeArray.Get();
		const MotionNeighborList* constantChildNeighborLists = blendConstant.m_ChildNeighborListArray.Get();
		
		Vector2f blendPosition = Vector2f (blendValueX, blendValueY);
		float magO = Magnitude (blendPosition);
		
		if (blendPosition == Vector2f::zero)
		{
			for (int i=0; i<count; i++)
				workspaceBlendVectors[i] = Vector2f (0, magO - constantMagnitudes[i]);
		}
		else
		{
			for (int i=0; i<count; i++)
			{
				if (positionArray[i] == Vector2f::zero)
					workspaceBlendVectors[i] = Vector2f (0, magO - constantMagnitudes[i]);
				else
				{
					float angle = Angle (positionArray[i], blendPosition);
					if (positionArray[i].x * blendPosition.y - positionArray[i].y * blendPosition.x < 0)
						angle = -angle;
					workspaceBlendVectors[i] = Vector2f (angle, magO - constantMagnitudes[i]);
				}
			}
		}
		
		if (preCompute)
		{
			for (int i=0; i<count; i++)
			{
				// Fade out over 180 degrees away from example
				float value = 1 - abs (workspaceBlendVectors[i].x) * kInversePI;
				cropArray[i] = -1;
				for (int j=0; j<count; j++)
				{
					if (i==j)
						continue;
					
					float newValue = GetWeightFreeformDirectional (blendConstant, workspaceBlendVectors, i, j, blendPosition);
					
					if (newValue <= 0)
					{
						value = 0;
						cropArray[i] = -1;
						break;
					}
					// Used for determining neighbors
					if (newValue < value)
						cropArray[i] = j;
					value = min (value, newValue);
				}
			}
			return;
		}
		
		for (int i=0; i<count; i++)
		{
			// Fade out over 180 degrees away from example
			float value = 1 - abs (workspaceBlendVectors[i].x) * kInversePI;
			for (int jIndex=0; jIndex<constantChildNeighborLists[i].m_Count; jIndex++)
			{
				int j = constantChildNeighborLists[i].m_NeighborArray[jIndex];
				float newValue = GetWeightFreeformDirectional (blendConstant, workspaceBlendVectors, i, j, blendPosition);
				if (newValue <= 0)
				{
					value = 0;
					break;
				}
				value = min (value, newValue);
			}
			weightArray[i] = value;
		}
		
		// Normalize weights
		float summedWeight = 0;
		for (int i=0; i<count; i++)
			summedWeight += weightArray[i];
		
		if (summedWeight > 0)
		{
			summedWeight = 1.0f / summedWeight; // Do division once instead of for every sample
			for (int i=0; i<count; i++)
				weightArray[i] *= summedWeight;
		}
		else
		{
			// Give weight to all children as fallback when no children have any weight.
			// This happens when sampling in the center if no center motion is provided.
			float evenWeight = 1.0f / count;
			for (int i=0; i<count; i++)
				weightArray[i] = evenWeight;
		}
	}
	
	void GetWeightsFreeformCartesian (const Blend2dDataConstant& blendConstant,
									  float* weightArray, int* cropArray, Vector2f* workspaceBlendVectors,
									  float blendValueX, float blendValueY, bool preCompute = false)
	{
		// Get constants
		const Vector2f* positionArray = blendConstant.m_ChildPositionArray.Get();
		mecanim::uint32_t count = blendConstant.m_ChildCount;
		const MotionNeighborList* constantChildNeighborLists = blendConstant.m_ChildNeighborListArray.Get();
		
		Vector2f blendPosition = Vector2f (blendValueX, blendValueY);
		for (int i=0; i<count; i++)
			workspaceBlendVectors[i] = blendPosition - positionArray[i];
		
		if (preCompute)
		{
			for (int i=0; i<count; i++)
			{
				cropArray[i] = -1;
				Vector2f vecIO = workspaceBlendVectors[i];
				float value = 1;
				for (int j=0; j<count; j++)
				{
					if (i==j)
						continue;
					
					int pairIndex = i + j*blendConstant.m_ChildCount;
					Vector2f vecIJ = blendConstant.m_ChildPairVectorArray[pairIndex];
					float newValue = 1 - Dot (vecIJ, vecIO) * blendConstant.m_ChildPairAvgMagInvArray[pairIndex];
					if (newValue <= 0)
					{
						value = 0;
						cropArray[i] = -1;
						break;
					}
					// Used for determining neighbors
					if (newValue < value)
						cropArray[i] = j;
					value = min (value, newValue);
				}
			}
			return;
		}
		
		for (int i=0; i<count; i++)
		{
			Vector2f vecIO = workspaceBlendVectors[i];
			float value = 1;
			for (int jIndex=0; jIndex<constantChildNeighborLists[i].m_Count; jIndex++)
			{
				int j = constantChildNeighborLists[i].m_NeighborArray[jIndex];
				if (i==j)
					continue;
				
				int pairIndex = i + j*blendConstant.m_ChildCount;
				Vector2f vecIJ = blendConstant.m_ChildPairVectorArray[pairIndex];
				float newValue = 1 - Dot (vecIJ, vecIO) * blendConstant.m_ChildPairAvgMagInvArray[pairIndex];
				if (newValue < 0)
				{
					value = 0;
					break;
				}
				value = min (value, newValue);
			}
			weightArray[i] = value;
		}
		
		// Normalize weights
		float summedWeight = 0;
		for (int i=0; i<count; i++)
			summedWeight += weightArray[i];
		summedWeight = 1.0f / summedWeight; // Do division once instead of for every sample
		for (int i=0; i<count; i++)
			weightArray[i] *= summedWeight;
	}
	
	void GetWeights1d (const Blend1dDataConstant& blendConstant, float* weightArray, float blendValue)
	{
		blendValue = math::clamp (blendValue, blendConstant.m_ChildThresholdArray[0], blendConstant.m_ChildThresholdArray[blendConstant.m_ChildCount-1]);
		for (mecanim::uint32_t j = 0 ; j < blendConstant.m_ChildCount; j++)
			weightArray[j] = WeightForIndex (blendConstant.m_ChildThresholdArray.Get (), blendConstant.m_ChildCount, j, blendValue);
	}
	
	void GetWeights (const BlendTreeNodeConstant& nodeConstant, BlendTreeWorkspace &workspace, float* weightArray, float blendValueX, float blendValueY)
	{
		if (nodeConstant.m_BlendType == 0)
			GetWeights1d (*nodeConstant.m_Blend1dData.Get(), weightArray, blendValueX);
		else if (nodeConstant.m_BlendType == 1)
			GetWeightsSimpleDirectional   (*nodeConstant.m_Blend2dData.Get(), weightArray, workspace.m_TempCropArray, workspace.m_ChildInputVectorArray, blendValueX, blendValueY);
		else if (nodeConstant.m_BlendType == 2)
			GetWeightsFreeformDirectional (*nodeConstant.m_Blend2dData.Get(), weightArray, workspace.m_TempCropArray, workspace.m_ChildInputVectorArray, blendValueX, blendValueY);
		else if (nodeConstant.m_BlendType == 3)
			GetWeightsFreeformCartesian   (*nodeConstant.m_Blend2dData.Get(), weightArray, workspace.m_TempCropArray, workspace.m_ChildInputVectorArray, blendValueX, blendValueY);
	}
	
	uint32_t ComputeBlends(const BlendTreeConstant& constant, const BlendTreeInput &input, const BlendTreeMemory &memory, BlendTreeOutput &output, BlendTreeWorkspace &workspace)
	{		
		uint32_t leafIndex = 0;
		uint32_t currentOutputIndex = 0 ;
		workspace.m_BlendArray[0] = 1; 		

		uint32_t i = 0;
		for(i = 0 ; i < constant.m_NodeCount; i ++)
		{
			const BlendTreeNodeConstant* nodeConstant = constant.m_NodeArray[i].Get();

			if(nodeConstant->m_ClipID != -1)
			{
				if(workspace.m_BlendArray[i] > 0) 
				{				
					float duration = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1) ? memory.m_NodeDurationArray[leafIndex] * nodeConstant->m_Duration : nodeConstant->m_Duration;

					output.m_OutputBlendArray[currentOutputIndex].m_ID = nodeConstant->m_ClipID;
					output.m_OutputBlendArray[currentOutputIndex].m_BlendValue = workspace.m_BlendArray[i];
					output.m_OutputBlendArray[currentOutputIndex].m_Reverse = duration < 0;
					output.m_OutputBlendArray[currentOutputIndex].m_CycleOffset = nodeConstant->m_CycleOffset;
					output.m_OutputBlendArray[currentOutputIndex].m_Mirror = nodeConstant->m_Mirror;
			
					output.m_Duration += math::abs(duration) * workspace.m_BlendArray[i];
					currentOutputIndex++;
				}

				leafIndex++;
			}
			else if(nodeConstant->m_ChildCount> 0)
			{
				if (nodeConstant->m_BlendType == 0)
				{
					// 1D blending
					int32_t index =  FindValueIndex(constant.m_BlendEventArrayConstant.Get(), nodeConstant->m_BlendEventID);
					float blendValue;
					input.m_BlendValueArray->ReadData(blendValue, constant.m_BlendEventArrayConstant->m_ValueArray[index].m_Index);								

					GetWeights (*nodeConstant, workspace, workspace.m_TempWeightArray, blendValue, 0);
				}
				else if (nodeConstant->m_BlendType >= 1)
				{
					// 2D blending
					int32_t indexX =  FindValueIndex(constant.m_BlendEventArrayConstant.Get(), nodeConstant->m_BlendEventID);
					int32_t indexY =  FindValueIndex(constant.m_BlendEventArrayConstant.Get(), nodeConstant->m_BlendEventYID);
					float blendValueX, blendValueY;
					input.m_BlendValueArray->ReadData(blendValueX, constant.m_BlendEventArrayConstant->m_ValueArray[indexX].m_Index);
					input.m_BlendValueArray->ReadData(blendValueY, constant.m_BlendEventArrayConstant->m_ValueArray[indexY].m_Index);
					
					GetWeights (*nodeConstant, workspace, workspace.m_TempWeightArray, blendValueX, blendValueY);
				}

				for(mecanim::uint32_t j = 0 ; j < nodeConstant->m_ChildCount; j++)
				{					
					float w = workspace.m_TempWeightArray[j];
					workspace.m_BlendArray[nodeConstant->m_ChildIndices[j]] = w * workspace.m_BlendArray[i];					
				}
			}
		}

		return currentOutputIndex;
	}
	
	void EvaluateBlendTree(const BlendTreeConstant& constant, const BlendTreeInput &input, const BlendTreeMemory &memory, BlendTreeOutput &output, BlendTreeWorkspace &workspace)
	{
		for(uint32_t i = 0 ; i < output.m_MaxBlendedClip ; i++) output.m_OutputBlendArray[i].m_ID = -1;

		output.m_Duration = 0;
		uint32_t currentOutputIndex = 0;

		if(constant.m_NodeCount >0)
			currentOutputIndex = ComputeBlends(constant, input, memory, output, workspace);			

		if(currentOutputIndex == 0) 
			output.m_Duration = 1;
	}


	mecanim::uint32_t GetLeafCount(const BlendTreeConstant& constant)
	{
		mecanim::uint32_t leafCount = 0 ;

		for(int i = 0 ; i < constant.m_NodeCount ; i++)
		{
			if(constant.m_NodeArray[i]->m_ClipID != -1)
				leafCount++;
		}	
		
		return leafCount;
	}

	void FillLeafIDArray(const BlendTreeConstant& constant, uint32_t* leafIDArray)
	{		
		uint32_t baseIndex = 0 ;
		uint32_t i;
		for(i = 0 ; i < constant.m_NodeCount ; i++)
		{
			if(constant.m_NodeArray[i]->m_ClipID != -1)
			{
				leafIDArray[baseIndex] = constant.m_NodeArray[i]->m_ClipID;
				baseIndex++;
			}			
		}					
	}

	mecanim::uint32_t GetMaxBlendCount(const BlendTreeConstant& constant, const BlendTreeNodeConstant& node)
	{
		uint32_t maxBlendCount = node.m_ClipID != -1 ? 1 : 0;

		// Blending occur between closest sibbling only
		uint32_t current = 0;
		uint32_t previous = 0;

		if(node.m_BlendType == 0 )
		{
		for(int i = 0 ; i < node.m_ChildCount ; i++)
		{
			current = GetMaxBlendCount(constant, *constant.m_NodeArray[node.m_ChildIndices[i]]);
			maxBlendCount = math::maximum( maxBlendCount, previous + current);
			previous = current;
		}
		}
		else
		{
			for(int i = 0 ; i < node.m_ChildCount ; i++)
			{
				maxBlendCount += GetMaxBlendCount(constant, *constant.m_NodeArray[node.m_ChildIndices[i]]);
			}
		}

		return maxBlendCount;
	}

	mecanim::uint32_t GetMaxBlendCount(const BlendTreeConstant& constant)
	{
		uint32_t maxBlendCount = 0;
		if(constant.m_NodeCount)
			maxBlendCount = GetMaxBlendCount(constant, *constant.m_NodeArray[0]);
		return maxBlendCount;
	}

	
}// namespace animation

}//namespace mecanim
