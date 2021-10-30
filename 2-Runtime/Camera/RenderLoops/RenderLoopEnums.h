#pragma once

enum RenderingPath {
	kRenderPathVertex = 0,
	kRenderPathForward,
	kRenderPathPrePass,
	kRenderPathCount
};

enum OcclusionQueryType {
	kOcclusionQueryTypeMostAccurate = 0,
	kOcclusionQueryTypeFastest,
	kOcclusionQueryTypeCount
};

enum
{
	kBackgroundRenderQueue	= 1000,
	kGeometryRenderQueue	= 2000,
	kAlphaTestRenderQueue	= 2450, // we want it to be in the end of geometry queue
	kTransparentRenderQueue	= 3000,
	kOverlayRenderQueue		= 4000,

	kQueueIndexMin = 0,
	kQueueIndexMax = 5000,

	kGeometryQueueIndexMin = kGeometryRenderQueue-500,
	kGeometryQueueIndexMax = kGeometryRenderQueue+500,
};
