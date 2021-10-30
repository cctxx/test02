#include "UnityPrefix.h"
#include "UmbraBackwardsCompatibilityDefine.h"

#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA

#define UMBRA_DISABLE_SPU_QUERIES 1
#define UMBRA_COMP_NO_EXCEPTIONS 1
#define UMBRA_SUPPORT_LEGACY_DATA 1
#define NO_SSE2_NAMESPACE

#include "External/Umbra_3_0/source/source/runtime/umbraBSPTree_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraBitOps_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraConnectivity_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraInstrumentation_GPA_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraIntersect_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraLegacyTome_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraPVSCull_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraPortalCull_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraPortalCull2_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraPortalRaster_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraPortalRayTracer_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraQueryApi_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraQueryArgs_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraQueryContext_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraSIMD_SSE_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraNoSSE2_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraTome_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraTomeApi_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/umbraTransformer_3_0.cpp"
#include "External/Umbra_3_0/source/source/shared/umbraAABB_3_0.cpp"
#include "External/Umbra_3_0/source/source/shared/umbraMatrix_3_0.cpp"
#include "External/Umbra_3_0/source/source/shared/umbraPrivateDefs_3_0.cpp"
#include "External/Umbra_3_0/source/source/shared/umbraPrivateVersion_3_0.cpp"
#include "External/Umbra_3_0/source/source/shared/umbraRandom_3_0.cpp"
#include "External/Umbra_3_0/source/source/runtime/xbox360/umbraSIMD_XBOX360_3_0.cpp"

#endif