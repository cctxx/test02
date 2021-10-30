#include "UnityPrefix.h"
#include "CubemapProcessor.h"

//--------------------------------------------------------------------------------------
//Based on CCubeMapProcessor from CubeMapGen v1.4
// Class for filtering and processing cubemaps
//
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

//used to index cube faces
#define CP_FACE_X_POS 0
#define CP_FACE_X_NEG 1
#define CP_FACE_Y_POS 2
#define CP_FACE_Y_NEG 3
#define CP_FACE_Z_POS 4
#define CP_FACE_Z_NEG 5

//used to index image edges
// NOTE.. the actual number corresponding to the edge is important
//  do not change these, or the code will break
//
// CP_EDGE_LEFT   is u = 0
// CP_EDGE_RIGHT  is u = width-1
// CP_EDGE_TOP    is v = 0
// CP_EDGE_BOTTOM is v = height-1
#define CP_EDGE_LEFT   0
#define CP_EDGE_RIGHT  1
#define CP_EDGE_TOP    2
#define CP_EDGE_BOTTOM 3

//corners of CUBE map (P or N specifys if it corresponds to the 
//  positive or negative direction each of X, Y, and Z
#define CP_CORNER_NNN  0
#define CP_CORNER_NNP  1
#define CP_CORNER_NPN  2
#define CP_CORNER_NPP  3
#define CP_CORNER_PNN  4
#define CP_CORNER_PNP  5
#define CP_CORNER_PPN  6
#define CP_CORNER_PPP  7


//information about cube maps neighboring face after traversing
// across an edge
struct CPCubeMapNeighbor
{
    UInt8 m_Face;    //index of neighboring face
    UInt8 m_Edge;    //edge in neighboring face that abuts this face
};

//------------------------------------------------------------------------------
// D3D cube map face specification
//   mapping from 3D x,y,z cube map lookup coordinates 
//   to 2D within face u,v coordinates
//
//   --------------------> U direction 
//   |                   (within-face texture space)
//   |         _____
//   |        |     |
//   |        | +Y  |
//   |   _____|_____|_____ _____
//   |  |     |     |     |     |
//   |  | -X  | +Z  | +X  | -Z  |
//   |  |_____|_____|_____|_____|
//   |        |     |
//   |        | -Y  |
//   |        |_____|
//   |
//   v   V direction
//      (within-face texture space)
//------------------------------------------------------------------------------

//Information about neighbors and how texture coorrdinates change across faces 
//  in ORDER of left, right, top, bottom (e.g. edges corresponding to u=0, 
//  u=1, v=0, v=1 in the 2D coordinate system of the particular face.
//Note this currently assumes the D3D cube face ordering and orientation
CPCubeMapNeighbor sg_CubeNgh[6][4] =
{
    //XPOS face
    {{CP_FACE_Z_POS, CP_EDGE_RIGHT },
		{CP_FACE_Z_NEG, CP_EDGE_LEFT  },
		{CP_FACE_Y_POS, CP_EDGE_RIGHT },
		{CP_FACE_Y_NEG, CP_EDGE_RIGHT }},
    //XNEG face
    {{CP_FACE_Z_NEG, CP_EDGE_RIGHT },
		{CP_FACE_Z_POS, CP_EDGE_LEFT  },
		{CP_FACE_Y_POS, CP_EDGE_LEFT  },
		{CP_FACE_Y_NEG, CP_EDGE_LEFT  }},
    //YPOS face
    {{CP_FACE_X_NEG, CP_EDGE_TOP },
		{CP_FACE_X_POS, CP_EDGE_TOP },
		{CP_FACE_Z_NEG, CP_EDGE_TOP },
		{CP_FACE_Z_POS, CP_EDGE_TOP }},
    //YNEG face
    {{CP_FACE_X_NEG, CP_EDGE_BOTTOM},
		{CP_FACE_X_POS, CP_EDGE_BOTTOM},
		{CP_FACE_Z_POS, CP_EDGE_BOTTOM},
		{CP_FACE_Z_NEG, CP_EDGE_BOTTOM}},
    //ZPOS face
    {{CP_FACE_X_NEG, CP_EDGE_RIGHT  },
		{CP_FACE_X_POS, CP_EDGE_LEFT   },
		{CP_FACE_Y_POS, CP_EDGE_BOTTOM },
		{CP_FACE_Y_NEG, CP_EDGE_TOP    }},
    //ZNEG face
    {{CP_FACE_X_POS, CP_EDGE_RIGHT  },
		{CP_FACE_X_NEG, CP_EDGE_LEFT   },
		{CP_FACE_Y_POS, CP_EDGE_TOP    },
		{CP_FACE_Y_NEG, CP_EDGE_BOTTOM }}
};


//The 12 edges of the cubemap, (entries are used to index into the neighbor table)
// this table is used to average over the edges.
int sg_CubeEdgeList[12][2] = {
	{CP_FACE_X_POS, CP_EDGE_LEFT},
	{CP_FACE_X_POS, CP_EDGE_RIGHT},
	{CP_FACE_X_POS, CP_EDGE_TOP},
	{CP_FACE_X_POS, CP_EDGE_BOTTOM},
	
	{CP_FACE_X_NEG, CP_EDGE_LEFT},
	{CP_FACE_X_NEG, CP_EDGE_RIGHT},
	{CP_FACE_X_NEG, CP_EDGE_TOP},
	{CP_FACE_X_NEG, CP_EDGE_BOTTOM},
	
	{CP_FACE_Z_POS, CP_EDGE_TOP},
	{CP_FACE_Z_POS, CP_EDGE_BOTTOM},
	{CP_FACE_Z_NEG, CP_EDGE_TOP},
	{CP_FACE_Z_NEG, CP_EDGE_BOTTOM}
};


//Information about which of the 8 cube corners are correspond to the 
//  the 4 corners in each cube face
//  the order is upper left, upper right, lower left, lower right
int sg_CubeCornerList[6][4] = {
	{ CP_CORNER_PPP, CP_CORNER_PPN, CP_CORNER_PNP, CP_CORNER_PNN }, // XPOS face
	{ CP_CORNER_NPN, CP_CORNER_NPP, CP_CORNER_NNN, CP_CORNER_NNP }, // XNEG face
	{ CP_CORNER_NPN, CP_CORNER_PPN, CP_CORNER_NPP, CP_CORNER_PPP }, // YPOS face
	{ CP_CORNER_NNP, CP_CORNER_PNP, CP_CORNER_NNN, CP_CORNER_PNN }, // YNEG face
	{ CP_CORNER_NPP, CP_CORNER_PPP, CP_CORNER_NNP, CP_CORNER_PNP }, // ZPOS face
	{ CP_CORNER_PPN, CP_CORNER_NPN, CP_CORNER_PNN, CP_CORNER_NNN }  // ZNEG face
};



//==========================================================================================================
//void FixupCubeEdges(CImageSurface *a_CubeMap, int a_FixupType, int a_FixupWidth);
//
//Apply edge fixup to a cubemap mip level.
//
//a_CubeMap       [in/out] Array of 6 images comprising cubemap miplevel to apply edge fixup to.
//a_FixupType     [in]     Specifies the technique used for edge fixup.  Choose one of the following, 
//                         CP_FIXUP_NONE, CP_FIXUP_PULL_LINEAR, CP_FIXUP_PULL_HERMITE, CP_FIXUP_AVERAGE_LINEAR, 
//                         CP_FIXUP_AVERAGE_HERMITE 
//a_FixupWidth    [in]     Fixup width in texels
//
//==========================================================================================================


//--------------------------------------------------------------------------------------
// Fixup cube edges
//
// average texels on cube map faces across the edges
//--------------------------------------------------------------------------------------

void FixupCubeEdges(CImageSurface *a_CubeMap, int a_FixupType, int a_FixupWidth)
{
	int i, j, k;
	int face;
	int edge;
	int neighborFace;
	int neighborEdge;
	
	int nChannels = a_CubeMap[0].m_NumChannels;
	int size = a_CubeMap[0].m_Width;
	
	CPCubeMapNeighbor neighborInfo;
	
	CP_ITYPE* edgeStartPtr;
	CP_ITYPE* neighborEdgeStartPtr;
	
	int edgeWalk;
	int neighborEdgeWalk;
	
	//pointer walk to walk one texel away from edge in perpendicular direction
	int edgePerpWalk;
	int neighborEdgePerpWalk;
	
	//number of texels inward towards cubeface center to apply fixup to
	int fixupDist;
	int iFixup;   
	
	// note that if functionality to filter across the three texels for each corner, then 
	CP_ITYPE *cornerPtr[8][3];      //indexed by corner and face idx
	CP_ITYPE *faceCornerPtrs[4];    //corner pointers for face
	int cornerNumPtrs[8];         //indexed by corner and face idx
	int iCorner;                  //corner iterator
	int iFace;                    //iterator for faces
	int corner;
	
	//if there is no fixup, or fixup width = 0, do nothing
	if((a_FixupType == CP_FIXUP_NONE) ||
	   (a_FixupWidth == 0)  )
	{
		return;
	}
	
	//special case 1x1 cubemap, average face colors
	if( a_CubeMap[0].m_Width == 1 )
	{
		//iterate over channels
		for(k=0; k<nChannels; k++)
		{   
			CP_ITYPE accum = 0.0f;
			
			//iterate over faces to accumulate face colors
			for(iFace=0; iFace<6; iFace++)
			{
				accum += *(a_CubeMap[iFace].m_ImgData + k);
			}
			
			//compute average over 6 face colors
			accum /= 6.0f;
			
			//iterate over faces to distribute face colors
			for(iFace=0; iFace<6; iFace++)
			{
				*(a_CubeMap[iFace].m_ImgData + k) = accum;
			}
		}
		
		return;
	}
	
	
	//iterate over corners
	for(iCorner = 0; iCorner < 8; iCorner++ )
	{
		cornerNumPtrs[iCorner] = 0;
	}
	
	//iterate over faces to collect list of corner texel pointers
	for(iFace=0; iFace<6; iFace++ )
	{
		//the 4 corner pointers for this face
		faceCornerPtrs[0] = a_CubeMap[iFace].m_ImgData;
		faceCornerPtrs[1] = a_CubeMap[iFace].m_ImgData + ( (size - 1) * nChannels );
		faceCornerPtrs[2] = a_CubeMap[iFace].m_ImgData + ( (size) * (size - 1) * nChannels );
		faceCornerPtrs[3] = a_CubeMap[iFace].m_ImgData + ( (((size) * (size - 1)) + (size - 1)) * nChannels );
		
		//iterate over face corners to collect cube corner pointers
		for(i=0; i<4; i++ )
		{
			corner = sg_CubeCornerList[iFace][i];   
			cornerPtr[corner][ cornerNumPtrs[corner] ] = faceCornerPtrs[i];
			cornerNumPtrs[corner]++;
		}
	}
	
	
	//iterate over corners to average across corner tap values
	for(iCorner = 0; iCorner < 8; iCorner++ )
	{
		for(k=0; k<nChannels; k++)
		{             
			CP_ITYPE cornerTapAccum;
			
			cornerTapAccum = 0.0f;
			
			//iterate over corner texels and average results
			for(i=0; i<3; i++ )
			{
				cornerTapAccum += *(cornerPtr[iCorner][i] + k);
			}
			
			//divide by 3 to compute average of corner tap values
			cornerTapAccum *= (1.0f / 3.0f);
			
			//iterate over corner texels and average results
			for(i=0; i<3; i++ )
			{
				*(cornerPtr[iCorner][i] + k) = cornerTapAccum;
			}
		}
	}   
	
	
	//maximum width of fixup region is one half of the cube face size
	fixupDist = std::min( a_FixupWidth, size / 2);
	
	//iterate over the twelve edges of the cube to average across edges
	for(i=0; i<12; i++)
	{
		face = sg_CubeEdgeList[i][0];
		edge = sg_CubeEdgeList[i][1];
		
		neighborInfo = sg_CubeNgh[face][edge];
		neighborFace = neighborInfo.m_Face;
		neighborEdge = neighborInfo.m_Edge;
		
		edgeStartPtr = a_CubeMap[face].m_ImgData;
		neighborEdgeStartPtr = a_CubeMap[neighborFace].m_ImgData;
		edgeWalk = 0;
		neighborEdgeWalk = 0;
		
		//amount to pointer to sample taps away from cube face
		edgePerpWalk = 0;
		neighborEdgePerpWalk = 0;
		
		//Determine walking pointers based on edge type
		// e.g. CP_EDGE_LEFT, CP_EDGE_RIGHT, CP_EDGE_TOP, CP_EDGE_BOTTOM
		switch(edge)
		{
			case CP_EDGE_LEFT:
				// no change to faceEdgeStartPtr  
				edgeWalk = nChannels * size;
				edgePerpWalk = nChannels;
				break;
			case CP_EDGE_RIGHT:
				edgeStartPtr += (size - 1) * nChannels;
				edgeWalk = nChannels * size;
				edgePerpWalk = -nChannels;
				break;
			case CP_EDGE_TOP:
				// no change to faceEdgeStartPtr  
				edgeWalk = nChannels;
				edgePerpWalk = nChannels * size;
				break;
			case CP_EDGE_BOTTOM:
				edgeStartPtr += (size) * (size - 1) * nChannels;
				edgeWalk = nChannels;
				edgePerpWalk = -(nChannels * size);
				break;
		}
		
		//For certain types of edge abutments, the neighbor edge walk needs to 
		//  be flipped: the cases are 
		// if a left   edge mates with a left or bottom  edge on the neighbor
		// if a top    edge mates with a top or right edge on the neighbor
		// if a right  edge mates with a right or top edge on the neighbor
		// if a bottom edge mates with a bottom or left  edge on the neighbor
		//Seeing as the edges are enumerated as follows 
		// left   =0 
		// right  =1 
		// top    =2 
		// bottom =3            
		// 
		//If the edge enums are the same, or the sum of the enums == 3, 
		//  the neighbor edge walk needs to be flipped
		if( (edge == neighborEdge) || ((edge + neighborEdge) == 3) )
		{   //swapped direction neighbor edge walk
			switch(neighborEdge)
			{
				case CP_EDGE_LEFT:  //start at lower left and walk up
					neighborEdgeStartPtr += (size - 1) * (size) *  nChannels;
					neighborEdgeWalk = -(nChannels * size);
					neighborEdgePerpWalk = nChannels;
					break;
				case CP_EDGE_RIGHT: //start at lower right and walk up
					neighborEdgeStartPtr += ((size - 1)*(size) + (size - 1)) * nChannels;
					neighborEdgeWalk = -(nChannels * size);
					neighborEdgePerpWalk = -nChannels;
					break;
				case CP_EDGE_TOP:   //start at upper right and walk left
					neighborEdgeStartPtr += (size - 1) * nChannels;
					neighborEdgeWalk = -nChannels;
					neighborEdgePerpWalk = (nChannels * size);
					break;
				case CP_EDGE_BOTTOM: //start at lower right and walk left
					neighborEdgeStartPtr += ((size - 1)*(size) + (size - 1)) * nChannels;
					neighborEdgeWalk = -nChannels;
					neighborEdgePerpWalk = -(nChannels * size);
					break;
			}            
		}
		else
		{ //swapped direction neighbor edge walk
			switch(neighborEdge)
			{
				case CP_EDGE_LEFT: //start at upper left and walk down
					//no change to neighborEdgeStartPtr for this case since it points 
					// to the upper left corner already
					neighborEdgeWalk = nChannels * size;
					neighborEdgePerpWalk = nChannels;
					break;
				case CP_EDGE_RIGHT: //start at upper right and walk down
					neighborEdgeStartPtr += (size - 1) * nChannels;
					neighborEdgeWalk = nChannels * size;
					neighborEdgePerpWalk = -nChannels;
					break;
				case CP_EDGE_TOP:   //start at upper left and walk left
					//no change to neighborEdgeStartPtr for this case since it points 
					// to the upper left corner already
					neighborEdgeWalk = nChannels;
					neighborEdgePerpWalk = (nChannels * size);
					break;
				case CP_EDGE_BOTTOM: //start at lower left and walk left
					neighborEdgeStartPtr += (size) * (size - 1) * nChannels;
					neighborEdgeWalk = nChannels;
					neighborEdgePerpWalk = -(nChannels * size);
					break;
			}
		}
		
		
		//Perform edge walk, to average across the 12 edges and smoothly propagate change to 
		//nearby neighborhood
		
		//step ahead one texel on edge
		edgeStartPtr += edgeWalk;
		neighborEdgeStartPtr += neighborEdgeWalk;
		
		// note that this loop does not process the corner texels, since they have already been
		//  averaged across faces across earlier
		for(j=1; j<(size - 1); j++)       
		{             
			//for each set of taps along edge, average them
			// and rewrite the results into the edges
			for(k = 0; k<nChannels; k++)
			{             
				CP_ITYPE edgeTap, neighborEdgeTap, avgTap;  //edge tap, neighborEdgeTap and the average of the two
				CP_ITYPE edgeTapDev, neighborEdgeTapDev;
				
				edgeTap = *(edgeStartPtr + k);
				neighborEdgeTap = *(neighborEdgeStartPtr + k);
				
				//compute average of tap intensity values
				avgTap = 0.5f * (edgeTap + neighborEdgeTap);
				
				//propagate average of taps to edge taps
				(*(edgeStartPtr + k)) = avgTap;
				(*(neighborEdgeStartPtr + k)) = avgTap;
				
				edgeTapDev = edgeTap - avgTap;
				neighborEdgeTapDev = neighborEdgeTap - avgTap;
				
				//iterate over taps in direction perpendicular to edge, and 
				//  adjust intensity values gradualy to obscure change in intensity values of 
				//  edge averaging.
				for(iFixup = 1; iFixup < fixupDist; iFixup++)
				{
					//fractional amount to apply change in tap intensity along edge to taps 
					//  in a perpendicular direction to edge 
					CP_ITYPE fixupFrac = (CP_ITYPE)(fixupDist - iFixup) / (CP_ITYPE)(fixupDist); 
					CP_ITYPE fixupWeight;
					
					switch(a_FixupType )
					{
						case CP_FIXUP_PULL_LINEAR:
						{
							fixupWeight = fixupFrac;
						}
							break;
						case CP_FIXUP_PULL_HERMITE:
						{
							//hermite spline interpolation between 1 and 0 with both pts derivatives = 0 
							// e.g. smooth step
							// the full formula for hermite interpolation is:
							//              
							//                  [  2  -2   1   1 ][ p0 ] 
							// [t^3  t^2  t  1 ][ -3   3  -2  -1 ][ p1 ]
							//                  [  0   0   1   0 ][ d0 ]
							//                  [  1   0   0   0 ][ d1 ]
							// 
							// Where p0 and p1 are the point locations and d0, and d1 are their respective derivatives
							// t is the parameteric coordinate used to specify an interpoltion point on the spline
							// and ranges from 0 to 1.
							//  if p0 = 0 and p1 = 1, and d0 and d1 = 0, the interpolation reduces to
							//
							//  p(t) =  - 2t^3 + 3t^2
							fixupWeight = ((-2.0 * fixupFrac + 3.0) * fixupFrac * fixupFrac);
						}
							break;
						case CP_FIXUP_AVERAGE_LINEAR:
						{
							fixupWeight = fixupFrac;
							
							//perform weighted average of edge tap value and current tap
							// fade off weight linearly as a function of distance from edge
							edgeTapDev = 
							(*(edgeStartPtr + (iFixup * edgePerpWalk) + k)) - avgTap;
							neighborEdgeTapDev = 
							(*(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k)) - avgTap;
						}
							break;
						case CP_FIXUP_AVERAGE_HERMITE:
						{
							fixupWeight = ((-2.0 * fixupFrac + 3.0) * fixupFrac * fixupFrac);
							
							//perform weighted average of edge tap value and current tap
							// fade off weight using hermite spline with distance from edge
							//  as parametric coordinate
							edgeTapDev = 
							(*(edgeStartPtr + (iFixup * edgePerpWalk) + k)) - avgTap;
							neighborEdgeTapDev = 
							(*(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k)) - avgTap;
						}
							break;
					}
					
					// vary intensity of taps within fixup region toward edge values to hide changes made to edge taps
					*(edgeStartPtr + (iFixup * edgePerpWalk) + k) -= (fixupWeight * edgeTapDev);
					*(neighborEdgeStartPtr + (iFixup * neighborEdgePerpWalk) + k) -= (fixupWeight * neighborEdgeTapDev);
				}
				
			}
			
			edgeStartPtr += edgeWalk;
			neighborEdgeStartPtr += neighborEdgeWalk;
		}        
	}
}
