#include "UnityPrefix.h"

extern "C" void RegisterModule_Navigation();
extern "C" void RegisterModule_Physics2D();
extern "C" void RegisterModule_Physics();
extern "C" void RegisterModule_PhysicsEditor();
extern "C" void RegisterModule_Terrain();
extern "C" void RegisterModule_Audio();
extern "C" void RegisterModule_Animation();
extern "C" void RegisterModule_ClusterRenderer();

//we diverge from our usual convention that all defines have to be set to either 0 or 1.
//Jam will set the *_IS_DYNAMICALLY_LINKED defines on this single cpp file only, based on if
//jam is planning to link a certain module statically or dynamically.  by doing the define check
//with ifndef like this, it avoids having to add all these defines to all targets that do not yet
//support modularization.  this method also creates support for having some modules statically linked
//and some modules dynamically, which is incredibly helpful during development of the modularization

void RegisterStaticallyLinkedModules()
{
#ifndef NAVMESH_IS_DYNAMICALLY_LINKED
	#if !UNITY_WEBGL
	RegisterModule_Navigation();
	#endif
#endif

#ifndef ANIMATION_IS_DYNAMICALLY_LINKED	
	RegisterModule_Animation();
#endif

#ifndef PHYSICS_IS_DYNAMICALLY_LINKED
	#if ENABLE_PHYSICS
	RegisterModule_Physics();
	#endif
#endif

#ifndef TERRAIN_IS_DYNAMICALLY_LINKED
	#if ENABLE_TERRAIN
	RegisterModule_Terrain();
	#endif
#endif

#ifndef DYNAMICS2D_IS_DYNAMICALLY_LINKED
	#if ENABLE_2D_PHYSICS
	RegisterModule_Physics2D();
	#endif
#endif

#ifndef AUDIO_IS_DYNAMICALLY_LINKED	
	#if ENABLE_AUDIO
	RegisterModule_Audio();
	#endif
#endif

#ifndef CLUSTER_SYNC_IS_DYNAMICALLY_LINKED
	#if ENABLE_CLUSTER_SYNC
	RegisterModule_ClusterRenderer();
	#endif
#endif

#if UNITY_EDITOR
#ifndef PHYSICSEDITOR_IS_DYNAMICALLY_LINKED
	RegisterModule_PhysicsEditor();
#endif
#endif

#if ENABLE_CLUSTER_SYNC
#ifndef CLUSTERRENDERER_IS_DYNAMICALLY_LINKED
	RegisterModule_ClusterRenderer();
#endif
#endif

}