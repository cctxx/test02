#ifndef CUBEMAPPROCESSOR_H
#define CUBEMAPPROCESSOR_H

//--------------------------------------------------------------------------------------
// Based on CCubeMapProcessor from CubeMapGen v1.4
// http://developer.amd.com/archive/gpu/cubemapgen/Pages/default.aspx#download
//  Class for filtering and processing cubemaps
//
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------


#define CP_ITYPE float

struct CImageSurface
{
	int m_Width;          //image width
	int m_Height;         //image height
	int m_NumChannels;    //number of channels
	CP_ITYPE *m_ImgData;    //cubemap image data
};

// Edge fixup type (how to perform smoothing near edge region)
#define CP_FIXUP_NONE            0
#define CP_FIXUP_PULL_LINEAR     1
#define CP_FIXUP_PULL_HERMITE    2
#define CP_FIXUP_AVERAGE_LINEAR  3
#define CP_FIXUP_AVERAGE_HERMITE 4


void FixupCubeEdges (CImageSurface *a_CubeMap, int a_FixupType, int a_FixupWidth);

#endif
