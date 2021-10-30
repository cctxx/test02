#pragma once

class BaseRenderer;

struct SceneNode
{
	SceneNode() :
	renderer(NULL), layer(0), pvsHandle(-1), lodGroup(0), lodIndexMask(0), 
	needsCullCallback(false), dirtyAABB(false), disable(false) {}
	
	BaseRenderer*	renderer;
	UInt32			layer;
	SInt32			pvsHandle;
	UInt32			lodGroup;
	UInt32			lodIndexMask;
	bool			needsCullCallback;
	bool			dirtyAABB;
	///@TODO: Maybe we can use Renderer* = NULL instead, we already set this to null for static objects...
	bool            disable;
};

typedef int SceneHandle;
const int kInvalidSceneHandle = -1;
