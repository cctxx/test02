#include "UnityPrefix.h"
#include "GizmoUtil.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Filters/Particles/ParticleRenderer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"
#include "Runtime/Filters/Misc/TrailRenderer.h"
#include "Runtime/Dynamics/ClothRenderer.h"
#include "Editor/Src/Selection.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Camera.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Geometry/Ray.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/AABBUtility.h"
#include "BezierCurve.h"
#include "Runtime/Filters/Misc/LineBuilder.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"
#include "GizmoRenderer.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Editor/Src/PrefKeys.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Dynamics/Collider.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"

PREFFLOAT (kSceneViewDepthTransp, .5f);
static SHADERPROP (Tex);

using namespace std;
const int kGizmoSegments = 32;
const float kHandleSize = 80.0f;
const float kBackfaceAlphaMultiplier = 0.2f;
static const char* kAssetsFolder = "Assets";

static Matrix4x4f gGizmoMatrix;
void SetGizmoMatrix (const Matrix4x4f &mat) {
	gGizmoMatrix = mat;
}
void ClearGizmoMatrix () {
	gGizmoMatrix = Matrix4x4f::identity;
}
const Matrix4x4f &GetGizmoMatrix () {
	return gGizmoMatrix;
}

static void LoadGizmoMatrix()
{
	GfxDevice& device = GetGfxDevice();
	device.SetViewMatrix (GetCurrentCamera().GetWorldToCameraMatrix().GetPtr());
	device.SetWorldMatrix (gGizmoMatrix.GetPtr());
}

static void InsertChildrenRecurse (Transform& transform, set<Transform*>& transforms);
static void InsertChildrenRecurse (Transform& transform, set<Transform*>& transforms)
{
	if (transforms.insert (&transform).second)
	{
		for (Transform::iterator i=transform.begin ();i != transform.end ();i++)
			InsertChildrenRecurse (**i, transforms);
	}
}

set<Transform*> SelectionToDeepHierarchy (const set<Object*>& selection)
{
	set<Transform*> transforms;
	for (set<Object*>::const_iterator i = selection.begin ();i != selection.end ();i++)
	{
		// Filter out any NON-go's and GOs without transform
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go == NULL || go->QueryComponent (Transform) == NULL)
			continue;
		
		InsertChildrenRecurse (go->GetComponent (Transform), transforms);
	}
	return transforms;
}

set<Transform*> SelectionToDeepHierarchy (const set<Transform*>& selection)
{
	set<Transform*> transforms;
	for (set<Transform*>::const_iterator i = selection.begin ();i != selection.end ();i++)
	{
		// Filter out any NON-go's and GOs without transform
		Transform* transform = *i;
		if (transform == NULL)
			continue;
		
		InsertChildrenRecurse (*transform, transforms);
	}
	return transforms;
}

static AABB SanitizeBounds (const MinMaxAABB& bounds)
{
	if (!IsFinite(bounds.GetMin()) || !IsFinite(bounds.GetMax()))
		return AABB(Vector3f::infinityVec, Vector3f::infinityVec);
	AABB aabb = bounds;
	
	if (aabb.GetExtent().x < -0.0001F || aabb.GetExtent().y < -0.0001F || aabb.GetExtent().z < -0.0001F)
		return AABB(Vector3f::infinityVec, Vector3f::infinityVec);

	const float kMaxCenterPosition = 100000.0F;
	const float kMaxSize = 10000.0F;
	
	if (Abs(aabb.GetCenter().x) > kMaxCenterPosition || Abs(aabb.GetCenter().y) > kMaxCenterPosition || Abs(aabb.GetCenter().y) > kMaxCenterPosition)
		return AABB(Vector3f::infinityVec, Vector3f::infinityVec);

	if (Abs(aabb.GetExtent().x) > kMaxSize || aabb.GetExtent().y > kMaxSize || Abs(aabb.GetExtent().y) > kMaxSize)
		return AABB(aabb.GetCenter(), Vector3f (min(aabb.GetExtent().x, kMaxSize), min(aabb.GetExtent().y, kMaxSize), min(aabb.GetExtent().z, kMaxSize)));

	return bounds;
}

MinMaxAABB CalculateObjectBounds (GameObject *go, bool onlyUsePivotForParticles, bool *any)
{
	MinMaxAABB minmax;
	Transform *transform = &go->GetComponent (Transform);
	
	//Always check for a collision component
	Collider* c = go->QueryComponent (Collider);
	if( c )
	{
		AABB colliderAABB = c->GetBounds( );
		minmax.Encapsulate(colliderAABB);
		*any = true;
	}
	
	Renderer* r = transform->QueryComponent(Renderer);
	if (r)
	{
		// Special particle renderer case
		
		// Otherwise use world bounds from renderer
		if (r->IsActive () && r->GetVisible ())
		{
			Renderer* r2 = transform->QueryComponent(ParticleRenderer);
			if (!r2)
				r2 = transform->QueryComponent(ParticleSystemRenderer);
			if (!r2)
				r2 = transform->QueryComponent(TrailRenderer);
			if (!r2)
				r2 = transform->QueryComponent(ClothRenderer);

			if (r2)
			{
				AABB aabb;
				r2->GetWorldAABB(aabb);
				bool pivotOnly = onlyUsePivotForParticles;
				if (CompareApproximately(aabb.GetExtent(), Vector3f(0.0F, 0.0F, 0.0F))) 
					pivotOnly = true;
				
				if (!pivotOnly)
				{
					minmax.Encapsulate(aabb);
					*any = true;
				}
				return minmax;
			}
			else
			{	
				*any = true;
				AABB rendererAABB;
				
				r->GetWorldAABB ( rendererAABB );
				minmax.Encapsulate(rendererAABB);
				return minmax;
			}
		}
		
		// Fall back to mesh filter
		MeshFilter* mf = transform->QueryComponent(MeshFilter);
		if (mf)
		{
			Mesh* me = mf->GetSharedMesh();
			if (me)
			{
				AABB bounds = me->GetBounds();
				
				Vector3f min = bounds.GetCenter() + bounds.GetExtent();
				Vector3f max = bounds.GetCenter() - bounds.GetExtent();
				minmax.Encapsulate(transform->TransformPoint(Vector3f (min.x, min.y, min.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (min.x, max.y, min.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (max.x, min.y, min.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (max.x, max.y, min.z)));
				
				minmax.Encapsulate(transform->TransformPoint(Vector3f (min.x, min.y, max.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (min.x, max.y, max.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (max.x, min.y, max.z)));
				minmax.Encapsulate(transform->TransformPoint(Vector3f (max.x, max.y, max.z)));
				*any = true;
			}
			return minmax;
		}
	}
	return minmax;
}



AABB CalculateSelectionBounds (bool onlyUsePivotForParticles)
{
	bool any = false;
	MinMaxAABB minmax;
	
	set<Transform*> objs = GetTransformSelection (kDeepSelection | kExcludePrefabSelection | kOnlyUserModifyableSelection);
	for (set<Transform*>::iterator i=objs.begin();i != objs.end();i++)
	{
		Transform* transform = *i;
		minmax.Encapsulate (CalculateObjectBounds (&transform->GetGameObject(), onlyUsePivotForParticles, &any));
	}
	
	AABB finalBounds = SanitizeBounds(minmax);
	
	if (IsFinite(finalBounds.GetCenter()) && IsFinite(finalBounds.GetExtent()) && any)
	{
		return finalBounds;
	}
	else
	{
		set<Transform*> rootSelection = GetTransformSelection (kExcludePrefabSelection | kOnlyUserModifyableSelection);
		
		// ok, we have no renderers. Do we have any transforms?
		if (rootSelection.empty())
			return AABB(Vector3f::infinityVec, Vector3f::infinityVec);
		else
		{

			minmax = MinMaxAABB();
			
			for (set<Transform*>::iterator i=rootSelection.begin();i != rootSelection.end();i++)
			{
				Transform* transform = *i;
				minmax.Encapsulate(transform->GetPosition());
			}
			return SanitizeBounds(minmax);
		}
	}
}

float CalculateRaySnapOffset (GameObject *go, const Vector3f &normal) 
{	
	float offset = std::numeric_limits<float>::infinity ();
	MinMaxAABB minmax;
	Transform &t = go->GetComponent(Transform);
	Matrix4x4f mat = t.GetLocalToWorldMatrix ();

	if (go->QueryComponent(Renderer))
	{
		// Fall back to mesh filter
		if (MeshFilter* mf = go->QueryComponent(MeshFilter))
		{
			if (Mesh* me = mf->GetSharedMesh())
			{
				for (StrideIterator<Vector3f> vert = me->GetVertexBegin(), end = me->GetVertexEnd (); vert != end; ++vert)
				{
					float off = Dot( mat.MultiplyPoint3 (*vert), normal);
					if (off < offset)
						offset = off;
				}
			}
		}
	}
	return offset;
}

set<Transform*> GetTransformSelection (int mask)
{
	TempSelectionSet selection;
	Selection::GetSelection(selection);
	
	set<Transform*> transforms;
	for (TempSelectionSet::iterator i=selection.begin ();i != selection.end ();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go == NULL || go->QueryComponent (Transform) == NULL)
			continue;
		
		if ((mask & kExcludePrefabSelection) && go->IsPrefabParent ())
			continue;

		if ((mask & kOnlyUserModifyableSelection) && !IsUserModifiable (*go))
			continue;

		transforms.insert (go->QueryComponent (Transform));
	}
	
	// Remove non top level transforms	
	if (mask & kTopLevelSelection)
	{
		set<Transform*>::iterator next;
		for (set<Transform*>::iterator i = transforms.begin ();i != transforms.end ();i=next)
		{
			next = i; next++;
			if (HasParentInSelection (**i, transforms))
				transforms.erase (i);
		}
	}
	
	if (mask & kDeepSelection)
		transforms = SelectionToDeepHierarchy (transforms);
	
	return transforms;
}

std::set<GameObject*> GetGameObjectSelection(int mask) {
	AssertIf (mask & kDeepSelection);
	AssertIf (mask & kTopLevelSelection);
	std::set<GameObject*> retval;
	TempSelectionSet selection;
	Selection::GetSelection(selection);

	for (TempSelectionSet::iterator i = selection.begin ();i != selection.end ();i++) {
		// Filter out any NON-go's
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go == NULL)
			continue;
		
		if ((mask & kExcludePrefabSelection) && go->IsPrefabParent ())
			continue;
	
		if ((mask & kOnlyUserModifyableSelection) && !IsUserModifiable (*go))
			continue;
		
		retval.insert (go);
	}
	return retval;
}

std::set<PPtr<Object> > GetObjectSelectionPPtr ()
{		
	std::set<int> objects = GetSceneTracker().GetSelectionID();;
	std::set<PPtr<Object> > outObjects;
	for (std::set<int>::iterator i=objects.begin();i != objects.end();i++)
		outObjects.insert(PPtr<Object> (*i));
	return outObjects;
}

// The folder tree view in the ProjectBrowser have its own selection
// to prevent stealing selection while browsing assets.
set <PPtr<Object> > GetFolderSelectionPPtr ()
{
	set <PPtr<Object> > result;
	MonoArray* folderInstanceIDs = (MonoArray*) CallStaticMonoMethod ("ProjectBrowser", "GetTreeViewFolderSelection");
	if (folderInstanceIDs)
	{	
		int folderCount = mono_array_length (folderInstanceIDs);
		for(int i = 0; i < folderCount ; i++ )
		{
			int instanceID = GetMonoArrayElement<int> (folderInstanceIDs, i);
			result.insert (PPtr<Object> (instanceID));
		}
	}
	return result;
}

set<UnityGUID> GetSelectedAssets (int mask)
{
	set<PPtr<Object> > objects;
	objects = GetFolderSelectionPPtr ();
	if (objects.empty ())
		objects = GetObjectSelectionPPtr ();

	set<UnityGUID> guids;
	AssetDatabase& database = AssetDatabase::Get ();

	for (set<PPtr<Object> >::iterator i= objects.begin ();i != objects.end ();i++)
	{
		UnityGUID guid = ObjectToGUID (*i);
		if (guid != UnityGUID ())
		{
			guids.insert (guid);
			if (mask & kDeepAssetsSelection)
				database.CollectAllChildren (guid, &guids);
		}
	}
	return guids;
}




UnityGUID GetSelectedAsset ()
{
	set<PPtr<Object> > objects = GetFolderSelectionPPtr ();
	if (!objects.empty ())
	{
		UnityGUID guid = ObjectToGUID (*objects.begin ());
		if (guid != UnityGUID ())
			return guid;
	}	

	Object* o = GetActiveObject ();
	if (o)
	{
		UnityGUID guid = ObjectToGUID (o);
		
		if (BeginsWith (ToLower(GetAssetPathFromGUID(guid)), ToLower(UnityStr(kAssetsFolder))))
		{
		if (guid != UnityGUID ())
			return guid;
	}
	}
	
	set<UnityGUID> guids = GetSelectedAssets ();
	if (!guids.empty ())
	{
		UnityGUID guid = *guids.begin ();
		
		if (BeginsWith (ToLower(GetAssetPathFromGUID(guid)), ToLower(UnityStr(kAssetsFolder))))
		{
			if (guid != UnityGUID ())
				return guid;
		}
	}
		return kAssetFolderGUID;
}


void GetObjectSelection (int mask, TempSelectionSet& selection)
{
	selection.clear();
	if (mask == 0)
		return GetSceneTracker().GetSelection(selection);
	else
	{
		TempSelectionSet objects;
		
		if (mask & (kDeepAssetsSelection | kAssetsSelection))
			GuidsToObjects(GetSelectedAssets(mask),objects);
		else
			GetSceneTracker().GetSelection(objects);
		
		for (TempSelectionSet::iterator i=objects.begin ();i != objects.end ();i++)
		{
			Object* o = *i;
		
			EditorExtension* extended = dynamic_pptr_cast<EditorExtension*>(o);
			if ((mask & kExcludePrefabSelection) && extended && extended->IsPrefabParent ())
				continue;
	
			if ((mask & kOnlyUserModifyableSelection) && !IsUserModifiable (*o))
				continue;
				
			selection.insert(o);	
		}
	}
}

GameObject *GetActiveGO ()
{
 	return  GetSceneTracker().GetActiveGO();;
}

Object* GetActiveObject ()
{
	return Selection::GetActive();
}

void SetActiveObject (Object* active)
{
	Selection::SetActive(active);
}

void SetObjectSelection(const TempSelectionSet& selection)
{
	Selection::SetSelection(selection);
}

void SetObjectSelection(const set<PPtr<Object> >& selection)
{
	Selection::SetSelectionPPtr(selection);
}


Transform* GetActiveTransform ()
{
	GameObject* go = GetActiveGO ();
	if (go)
	{
		if (go->IsPrefabParent ())
			return NULL;
		if (!IsUserModifiable (*go))
			return NULL;
		return go->QueryComponent (Transform);
	}
	else
		return NULL;
}

bool HasParentInSelection (Transform& transform, const set<Transform*>& selection)
{
	Transform* father = transform.GetParent ();
	while (father)
	{
		if (selection.count (father) == 1)
			break;
		father = father->GetParent ();
	}

	if (father == NULL)
		return false;
	else
		return true;
}

bool HasParentInSelection (GameObject& go, const set<Object*>& selection)
{
	Transform* object = go.QueryComponent (Transform);
	if (object == NULL)
		return false;
		
	Transform* father = object->GetParent ();
	while (father)
	{
		if (selection.count (father->GetGameObjectPtr ()) == 1)
			break;
		father = father->GetParent ();
	}

	if (father == NULL)
		return false;
	else
		return true;
}

bool HasParentInSelection (GameObject& go)
{
	if (go.QueryComponent (Transform))
		return HasParentInSelection (go.GetComponent (Transform), GetTransformSelection (0));
	else
		return false;
}

/// Calculates the extent points of all transforms uses the aabb of the renderer and the transform position
/// Does not 
static void CalculateExtentPoints (const set<Transform*>& transforms, vector<Vector3f>& points, bool addTransforms, bool forceParticleSystemCenter)
{
	points.reserve (points.size() + 8 * transforms.size ());
	for (set<Transform*>::const_iterator i = transforms.begin ();i != transforms.end ();i++)
	{
		Transform& transform = **i;
		AABB localAABB;
		const bool isParticleRenderer = (transform.QueryComponent(ParticleRenderer) || transform.QueryComponent(ParticleSystemRenderer));
		if (forceParticleSystemCenter && isParticleRenderer)
		{
			points.push_back (transform.GetPosition ());
		}
		else
		{
			Vector3f verts[8];
			if (CalculateAABBCornerVertices (transform.GetGameObject (), verts))
			{
				for (int v=0;v<8;v++)
					points.push_back (verts[v]);
			}
			else if (addTransforms)
				points.push_back (transform.GetPosition ());
		}
	}
}

AABB GetSelectionAABB (const set<Transform*>& transforms)
{
	vector<Vector3f> points;
	CalculateExtentPoints(transforms, points, true, false);
	if (!points.empty ())
	{
		MinMaxAABB aabb;
		aabb.Init ();
		for (int i = 0; i < points.size (); i++)
			aabb.Encapsulate (points[i]);

		return AABB (aabb);
	}
	else
	{
		return AABB(Vector3f::infinityVec, Vector3f::infinityVec);
	}
}


Vector3f GetSelectionCenter (const set<Transform*>& transforms) {
	AABB aabb = GetSelectionAABB (transforms);
	if (aabb.GetCenter () == Vector3f::infinityVec) 
		return Vector3f::zero;
	else
		return aabb.GetCenter ();
}



// Primitives drawing
//-----------------------------------------

static void RotateGL (const Vector3f &newZ) {
	Matrix3x3f temp;
	Vector3f upDir (0,1,0);
	Vector3f forw (NormalizeSafe (newZ));
	if (SqrMagnitude (Cross (upDir, forw)) < Vector3f::epsilon) {
		upDir = Vector3f (1,0,0);
	}
	LookRotationToMatrix (-newZ, upDir, &temp);
	Matrix4x4f temp2 (temp);
	GetGfxDevice().SetWorldMatrix( temp2.GetPtr() );
}

void DrawCap (CapStyle style, const Vector3f &center, const Vector3f  &dir, float size) {
	switch (style) {
	case kCapBox:
		DrawCube (center, Vector3f (size * .5f, size * .5f, size * .5f), dir);
		break;		
	case kCapCone:
		DrawCone (center - dir * size * .5f, center + dir * size * .5f, size * .5);
		break;
	case kCapDisk:
		DrawWireDisk (center, dir, size * .5);
		break;
	case kCapRect:
		DrawWireRect (center, size);
		break;
	case kCapCircle:
		DrawWireCircle (center, size);
		break;
	case kCapNone:
		break;
	}	
}

void DrawWireRect (const Vector3f &center, float radius, bool depthTest) {	
	float size = CalcHandleSize (center);
	radius = radius * size / kHandleSize;
	
	const Transform& tr = GetCurrentCamera().GetComponent(Transform);
	Vector3f ax = Normalize(tr.TransformDirection(Vector3f(1,0,0))) * radius;
	Vector3f ay = Normalize(tr.TransformDirection(Vector3f(0,1,0))) * radius;
	
	Vector3f verts[5];
	verts[0] = center - ax - ay;
	verts[1] = center + ax - ay;
	verts[2] = center + ax + ay;
	verts[3] = center - ax + ay;
	verts[4] = center - ax - ay;
	gizmos::AddLinePrimitives( kPrimitiveLineStrip, 5, verts, depthTest );
}

void DrawWireCircle (const Vector3f &center, float radius) {
	const Transform& tr = GetCurrentCamera().GetComponent(Transform);
	float size = CalcHandleSize (center);
	radius = radius * size / kHandleSize;
	DrawWireArc (center, tr.TransformDirection(Vector3f(0,0,1)), tr.TransformDirection(Vector3f(1,0,0)), 360.0f, radius);
}

void DrawCircleFrustum (const Vector3f &center, float fov, float maxRange) {
	fov = Deg2Rad ((fov) * .5f);
	Vector3f farEnd (0,0,maxRange);
	Vector3f endSizeX (maxRange * tanf (fov), 0, 0);
	Vector3f endSizeY (0, maxRange * tanf (fov),0);

	Vector3f e1 = farEnd + endSizeX;
	Vector3f e2 = farEnd - endSizeX;
	Vector3f e3 = farEnd - endSizeY;
	Vector3f e4 = farEnd + endSizeY;
	DrawLine (center, e1);
	DrawLine (center, e2);
	DrawLine (center, e3);
	DrawLine (center, e4);
	
	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	DrawWireDisk (farEnd, Vector3f (0,0,1), maxRange * tanf (fov));
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

void DrawFrustum (const Vector3f &center, float fov, float maxRange, float minRange, float aspect) {
	fov = Deg2Rad (fov * .5f);
	float tanfov = tanf (fov);
	Vector3f farEnd (0,0,maxRange);
	Vector3f endSizeX (maxRange * tanfov * aspect, 0, 0);
	Vector3f endSizeY (0, maxRange * tanfov / aspect,0);

	Vector3f s1, s2, s3, s4;
	Vector3f e1 = farEnd + endSizeX + endSizeY;
	Vector3f e2 = farEnd - endSizeX + endSizeY;
	Vector3f e3 = farEnd - endSizeX - endSizeY;
	Vector3f e4 = farEnd + endSizeX - endSizeY;
	if (minRange <= 0.0f) {
		s1 = s2 = s3 = s4 = center;
	} else {
		Vector3f startSizeX (minRange * tanfov * aspect, 0, 0);
		Vector3f startSizeY (0, minRange * tanfov / aspect, 0);
		Vector3f startPoint = Vector3f (0,0,minRange);
		s1 = 	startPoint + startSizeX + startSizeY;
		s2 = 	startPoint - startSizeX + startSizeY;
		s3 = 	startPoint - startSizeX - startSizeY;
		s4 = 	startPoint + startSizeX - startSizeY;
		DrawLine (s1, s2);
		DrawLine (s2, s3);
		DrawLine (s3, s4);
		DrawLine (s4, s1);
	}

	DrawLine (e1, e2);
	DrawLine (e2, e3);
	DrawLine (e3, e4);
	DrawLine (e4, e1);

	DrawLine (s1, e1);
	DrawLine (s2, e2);
	DrawLine (s3, e3);
	DrawLine (s4, e4);
}

void DrawCube (const Vector3f &center, const Vector3f &_size)  {
	DrawCube (center, _size, Vector3f (0,0,1));
}

void DrawCube (const Vector3f &center, const Vector3f& size, const Vector3f &forwardDir)
{
	static Vector3f s_CornerPoints[6][4] = {
		Vector3f (-1,1, 1), Vector3f (1,1, 1), Vector3f (1,-1, 1), Vector3f (-1,-1, 1),
		Vector3f (1,1, -1), Vector3f (-1,1, -1), Vector3f (-1,-1, -1), Vector3f (1,-1, -1),
		Vector3f (1, 1,1), Vector3f (1, 1,-1), Vector3f (1, -1,-1), Vector3f (1, -1,1),
		Vector3f (-1, 1,-1), Vector3f (-1, 1,1), Vector3f (-1, -1,1), Vector3f (-1, -1,-1),
		Vector3f (-1, 1,-1), Vector3f (1, 1,-1), Vector3f (1, 1,1), Vector3f (-1, 1,1), 
		Vector3f (-1, -1,1), Vector3f (1, -1,1), Vector3f (1, -1,-1), Vector3f (-1, -1,-1)
	};
	static Vector3f s_Normals[6] = {
		Vector3f (0,0,1),
		Vector3f (0,0,-1),
		Vector3f (1,0,0),
		Vector3f (-1,0,0),
		Vector3f (0,1,0),
		Vector3f (0,-1,0)
	};
	
	static gizmos::LitVertex s_Vertices[24];
	Vector3f halfSize = size * 0.5f;
	for( int i = 0; i < 6; ++i ) {
		Vector3f n = NormalizeSafe(gGizmoMatrix.MultiplyVector3(s_Normals[i]));
		s_Vertices[i*4+0].vertex = gGizmoMatrix.MultiplyPoint3( center + Scale(halfSize, s_CornerPoints[i][3]) );
		s_Vertices[i*4+0].normal = n;
		s_Vertices[i*4+1].vertex = gGizmoMatrix.MultiplyPoint3( center + Scale(halfSize, s_CornerPoints[i][2]) );
		s_Vertices[i*4+1].normal = n;
		s_Vertices[i*4+2].vertex = gGizmoMatrix.MultiplyPoint3( center + Scale(halfSize, s_CornerPoints[i][1]) );
		s_Vertices[i*4+2].normal = n;
		s_Vertices[i*4+3].vertex = gGizmoMatrix.MultiplyPoint3( center + Scale(halfSize, s_CornerPoints[i][0]) );
		s_Vertices[i*4+3].normal = n;
	}
	gizmos::AddLitPrimitives ( kPrimitiveQuads, 24, s_Vertices );
}

void DrawWireCube (const Vector3f& center, const Vector3f& siz, bool depthTest)
{
	Vector3f halfsize = siz * 0.5f;
	Vector3f p000 = gGizmoMatrix.MultiplyPoint3( center + Vector3f(-halfsize.x, -halfsize.y, -halfsize.z) );
	Vector3f p001 = gGizmoMatrix.MultiplyPoint3( center + Vector3f(-halfsize.x, -halfsize.y,  halfsize.z) );
	Vector3f p010 = gGizmoMatrix.MultiplyPoint3( center + Vector3f(-halfsize.x,  halfsize.y, -halfsize.z) );
	Vector3f p011 = gGizmoMatrix.MultiplyPoint3( center + Vector3f(-halfsize.x,  halfsize.y,  halfsize.z) );
	Vector3f p100 = gGizmoMatrix.MultiplyPoint3( center + Vector3f( halfsize.x, -halfsize.y, -halfsize.z) );
	Vector3f p101 = gGizmoMatrix.MultiplyPoint3( center + Vector3f( halfsize.x, -halfsize.y,  halfsize.z) );
	Vector3f p110 = gGizmoMatrix.MultiplyPoint3( center + Vector3f( halfsize.x,  halfsize.y, -halfsize.z) );
	Vector3f p111 = gGizmoMatrix.MultiplyPoint3( center + Vector3f( halfsize.x,  halfsize.y,  halfsize.z) );

	static Vector3f s_Vertices[24];
	int index = 0;
	
	#define ADD_CUBE_LINE(aaa,bbb) s_Vertices[index++] = aaa; s_Vertices[index++] = bbb
	ADD_CUBE_LINE(p000, p001);
	ADD_CUBE_LINE(p001, p011);
	ADD_CUBE_LINE(p011, p010);
	ADD_CUBE_LINE(p010, p000);
	
	ADD_CUBE_LINE(p100, p101);
	ADD_CUBE_LINE(p101, p111);
	ADD_CUBE_LINE(p111, p110);
	ADD_CUBE_LINE(p110, p100);
	
	ADD_CUBE_LINE (p000, p100);
	ADD_CUBE_LINE (p001, p101);
	ADD_CUBE_LINE (p010, p110);
	ADD_CUBE_LINE (p011, p111);
	#undef ADD_CUBE_LINE

	gizmos::AddLinePrimitives( kPrimitiveLines, 24, s_Vertices, depthTest );
}

static void MakeBezierPoints (Vector3f *inVertices, size_t subdivision, const Vector3f& startPosition, const Vector3f& endPosition, const Vector3f& startTangent, const Vector3f& endTangent) 
{
	BezierCurve bezierCurve;
	vector<BezierCurve::Knot>& knots = bezierCurve.Knots();
	BezierCurve::Knot firstKnot, secondKnot;
	firstKnot.m_Val[0] = Vector3f(0.0, 0.0, 0.0);
	firstKnot.m_Val[1] = startPosition;
	firstKnot.m_Val[2] = startTangent;
	secondKnot.m_Val[0] = endTangent;
	secondKnot.m_Val[1] = endPosition;
	secondKnot.m_Val[2] = Vector3f(0.0, 0.0, 0.0);
	knots.push_back(firstKnot);
	knots.push_back(secondKnot);
	bezierCurve.Touch();
	
	for(int t = 0; t < subdivision-1; t++) {
		inVertices[t] = bezierCurve.NormalPoint( (float)t / (float)subdivision );
	}
	inVertices[subdivision-1] = endPosition;
}	


PROFILER_INFORMATION(gSubmitVBOProfileGizmoUtil, "Mesh.SubmitVBO", kProfilerRender);

float DistancePointBezier (const Vector3f &point, const Vector3f& startPosition, const Vector3f& endPosition, const Vector3f& startTangent, const Vector3f& endTangent) 
{
	const int kSubdivision = 40;
	Vector3f inVertices[kSubdivision];
	MakeBezierPoints (&inVertices[0], kSubdivision, startPosition, endPosition, startTangent, endTangent);

	return DistancePointPolyLine (point, kSubdivision, &inVertices[0]); 
}
					   

void DrawBezier (const Vector3f& startPosition, const Vector3f& endPosition, const Vector3f& startTangent, const Vector3f& endTangent, const ColorRGBAf& color, Texture2D *texture, float width)
{
	const int kSubdivision = 40;
	Vector3f inVertices[kSubdivision];
	MakeBezierPoints (&inVertices[0], kSubdivision, startPosition, endPosition, startTangent, endTangent);

	DrawAAPolyLine (kSubdivision, &inVertices[0], NULL, color, texture, width);
}


template<int Sides> void DrawConeGeneric(const Vector3f &basePoint, const Vector3f &endPoint, float radius)
{
	Vector3f direction = Normalize(endPoint-basePoint);
	Vector3f tangent = Cross (direction, Vector3f(0,1,0));
	if (SqrMagnitude(tangent) < 0.1f)
		tangent = Cross (direction, Vector3f(1,0,0));
	Vector3f bitangent = Cross (direction, tangent);
	
	const int kConeSides = Sides;
	static gizmos::LitVertex s_Vertices[kConeSides*6];
	const float kDelta = kPI * 2.0f / kConeSides;
	float phi = 0.0f;
	for( int i = 0; i < kConeSides; ++i ) {
		float cs1 = cosf(phi);
		float ss1 = sinf(phi);
		float cs2 = cosf(phi+kDelta);
		float ss2 = sinf(phi+kDelta);
		Vector3f p1 = basePoint + tangent*(cs1*radius) + bitangent*(ss1*radius);
		Vector3f p2 = basePoint + tangent*(cs2*radius) + bitangent*(ss2*radius);
		Vector3f n = RobustNormalFromTriangle (endPoint, p1, p2);
		// triangle of the cone
		s_Vertices[i*6+0].vertex = endPoint;
		s_Vertices[i*6+0].normal = n;
		s_Vertices[i*6+1].vertex = p1;
		s_Vertices[i*6+1].normal = n;
		s_Vertices[i*6+2].vertex = p2;
		s_Vertices[i*6+2].normal = n;
		// triangle of the base point disk
		n = -direction;
		s_Vertices[i*6+3].vertex = basePoint;
		s_Vertices[i*6+3].normal = n;
		s_Vertices[i*6+4].vertex = p2;
		s_Vertices[i*6+4].normal = n;
		s_Vertices[i*6+5].vertex = p1;
		s_Vertices[i*6+5].normal = n;
		phi += kDelta;
	}
	gizmos::AddLitPrimitives( kPrimitiveTriangles, kConeSides*6, s_Vertices );
}

void DrawCone (const Vector3f &basePoint, const Vector3f &endPoint, float radius)
{
	DrawConeGeneric<8>(basePoint, endPoint, radius);
}

// Recursively subdivides a triangular area (vertices pta,ptb,ptc) into
// smaller triangles, and then draws the triangles. All triangle vertices are
// normalized to a distance of 1.0 from the origin (pta,ptb,ptc are assumed
// to be already normalized).
static gizmos::LitVertex* SubdivideTriPatch(
	const Vector3f& center, const Vector3f& pta, const Vector3f& ptb, const Vector3f& ptc,
	float radius, gizmos::LitVertex* vb, int level )
{
	if( level > 0 ) {
		// sub-vertices
		Vector3f q1 = Normalize( (pta+ptb) * 0.5f );
		Vector3f q2 = Normalize( (ptb+ptc) * 0.5f );
		Vector3f q3 = Normalize( (ptc+pta) * 0.5f );
		// recurse
		--level;
		vb = SubdivideTriPatch( center, pta,  q1,  q3, radius, vb, level );
		vb = SubdivideTriPatch( center,  q1, ptb,  q2, radius, vb, level );
		vb = SubdivideTriPatch( center,  q1,  q2,  q3, radius, vb, level );
		vb = SubdivideTriPatch( center,  q3,  q2, ptc, radius, vb, level );
	} else {
		// do triangle
		vb->vertex = center + pta*radius; vb->normal = pta; ++vb;
		vb->vertex = center + ptb*radius; vb->normal = ptb; ++vb;
		vb->vertex = center + ptc*radius; vb->normal = ptc; ++vb;
	}
	return vb;
}



void DrawSphere (const Vector3f &center, float radius)
{
	// draws tessellated icosahedron
	const int kTessLevel = 1;
	const int kVertexCount = 20 * 4 * 3; // depends on kTessLevel!
	static gizmos::LitVertex vertices[kVertexCount];

	const float ICX = 0.525731112119133606f;
	const float ICZ = 0.850650808352039932f;
	static Vector3f ICO_DATA[12] = {
		Vector3f(-ICX,    0,  ICZ),
		Vector3f( ICX,    0,  ICZ),
		Vector3f(-ICX,    0, -ICZ),
		Vector3f( ICX,    0, -ICZ),
		Vector3f(   0,  ICZ,  ICX),
		Vector3f(   0,  ICZ, -ICX),
		Vector3f(   0, -ICZ,  ICX),
		Vector3f(   0, -ICZ, -ICX),
		Vector3f( ICZ,  ICX,    0),
		Vector3f(-ICZ,  ICX,    0),
		Vector3f( ICZ, -ICX,    0),
		Vector3f(-ICZ, -ICX,    0)
	};
	static int ICO_INDEX[20][3] = {
		{0, 4, 1},	{0, 9, 4},
		{9, 5, 4},	{4, 5, 8},
		{4, 8, 1},	{8, 10, 1},
		{8, 3, 10},	{5, 3, 8},
		{5, 2, 3},	{2, 7, 3},
		{7, 10, 3},	{7, 6, 10},
		{7, 11, 6},	{11, 0, 6},
		{0, 1, 6},	{6, 1, 10},
		{9, 0, 11},	{9, 11, 2},
		{9, 2, 5},	{7, 2, 11},
	};
	gizmos::LitVertex* vb = vertices;
    for( int i = 0; i < 20; ++i ) {
		// draw patch
		vb = SubdivideTriPatch( center,
			ICO_DATA[ICO_INDEX[i][2]],
			ICO_DATA[ICO_INDEX[i][1]],
			ICO_DATA[ICO_INDEX[i][0]],
			radius, vb, kTessLevel );
	}
	
	// transform by matrix
	for( int i = 0; i < kVertexCount; ++i ) {
		vertices[i].vertex = gGizmoMatrix.MultiplyPoint3( vertices[i].vertex );
		vertices[i].normal = gGizmoMatrix.MultiplyVector3( vertices[i].normal );
	}

	gizmos::AddLitPrimitives( kPrimitiveTriangles, kVertexCount, vertices );
}


static void SetDiscSectionPoints( Vector3f dest[], int count, const Vector3f& center, const Vector3f& normal, const Vector3f& from, float angle, float radius) {
	Vector3f fromn = Normalize(from);
	Quaternionf r = AxisAngleToQuaternionSafe( normal, Deg2Rad(angle / (count - 1)) );
	Vector3f tangent = fromn * radius;
	for( int i = 0; i < count; i++) {
		dest[i] = center + tangent;
		tangent = RotateVectorByQuat (r, tangent);
	}
}

static float Intersect2DLines (const Vector2f& a0, const Vector2f& a1, const Vector2f& b0, const Vector2f& b1, Vector2f& isect)
{
	Vector2f diff = b0 - a0;
	Vector2f dirA = a1-a0;
	Vector2f dirB = b1-b0;
	Vector2f perpDirB = Vector2f(dirB.y, -dirB.x);
	float perpDotAB = Dot(dirA, perpDirB);
	float absDot = Abs(perpDotAB);
	if (absDot > 1.0e-3f)
	{
		float perpDotDiffB = Dot(diff, perpDirB);
		float s = perpDotDiffB / perpDotAB;
		isect = a0 + s * dirA;
		return absDot;
	}
	return 0.0f;
}

float CalcHandleSize (const Vector3f &pos, const Camera &cam)
{
	const Transform& tr = cam.GetComponent(Transform);
	Vector3f camPos = tr.GetPosition();
	float distance = Dot( pos - tr.GetPosition(), tr.TransformDirection(Vector3f(0,0,1)) );
	Vector3f screenPos = cam.WorldToScreenPoint (camPos + tr.TransformDirection(Vector3f (0,0,distance)));
	Vector3f screenPos2 = cam.WorldToScreenPoint (camPos + tr.TransformDirection(Vector3f (1,0,distance)));
	
	float screenDist (Magnitude (screenPos - screenPos2));
	return kHandleSize / std::max( screenDist, 0.0001f );
}

float CalcHandleSize (const Vector3f &pos)
{
	return CalcHandleSize (pos, GetCurrentCamera ());
}

float DistancePointPolyLine (const Vector3f& point, size_t count, const Vector3f* inVertices)
{
	float dist = numeric_limits<float>::infinity ();
	for (int i = 1; i < count; i++) 
		dist = std::min (dist, DistancePointLine(point, inVertices[i-1], inVertices[i]));
						 return dist;
	
}

void DrawAAPolyLine (size_t count, const Vector3f* points3, const ColorRGBAf* colors, ColorRGBAf defaultColor, Texture2D *texture, float width)
{
	static Texture2D* s_AATexture = NULL;

	if (!texture)
	{
		if (!s_AATexture)
		{
			s_AATexture = NEW_OBJECT_MAIN_THREAD (Texture2D);
			s_AATexture->Reset ();
			s_AATexture->AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);
			s_AATexture->SetHideFlags (Object::kHideAndDontSave);
			s_AATexture->InitTexture (1, 4, kTexFormatARGB32, Texture2D::kNoMipmap, 1);
			UInt8* pix = s_AATexture->GetRawImageData ();
			pix[0] = 0; pix[1] = 255; pix[2] = 255; pix[3] = 255;
			pix[4] = 0; pix[5] = 255; pix[6] = 255; pix[7] = 255;
			pix[8] = 255; pix[9] = 255; pix[10] = 255; pix[11] = 255;
			pix[12] = 0; pix[13] = 255; pix[14] = 255; pix[15] = 255;
			s_AATexture->UpdateImageDataDontTouchMipmap ();
		}
		texture = s_AATexture;
	}

	if (count < 2)
		return;

	GfxDevice& device = GetGfxDevice();
	const Camera* cam = GetCurrentCameraPtr ();
	
	// if GetCurrentCamera is NULL, we assume 2D stuff
	if(cam) 
	{
		Matrix4x4f g_matProjDevice;
		CopyMatrix(device.GetProjectionMatrix(), g_matProjDevice.GetPtr());

		// We check if current camera projection matrix is same as the device projection matrix
		// This detects the case when we are inside Handles.BeginGUI() / Handles.EndGUI() block and are doing 2D stuff
		if(!CompareApproximately(cam->GetProjectionMatrix(), g_matProjDevice))
			cam = NULL;
	}

	Vector3f* screenSpace;
	ALLOC_TEMP(screenSpace,Vector3f, count);
	
	for (size_t i = 0; i < count; i++)
		screenSpace[i] = cam ? cam->WorldToScreenPoint(points3[i]) : points3[i];

	device.SetTexture (kShaderFragment, 0, 0, texture->GetTextureID(), kTexDim2D, 0);
	device.ImmediateBegin (kPrimitiveTriangleStripDeprecated);

	float offX = 0.0f, offY = 0.0f;
//	if (!device.UsesHalfTexelOffset())
//	{
		offX = texture->GetTexelSizeX() * 0.5f;
		offY = texture->GetTexelSizeY() * 0.5f;
//	}

	// start
	{
		Vector2f curr(screenSpace[0].x, screenSpace[0].y);
		Vector2f next(screenSpace[1].x, screenSpace[1].y);
		Vector2f dif = NormalizeFast(next-curr) * width;
		
		Vector3f p1(curr.x + dif.y, curr.y - dif.x, screenSpace[0].z);
		Vector3f p2(curr.x - dif.y, curr.y + dif.x, screenSpace[0].z);

		Vector3f p1inv = cam ? cam->ScreenToWorldPoint(p1) : p1;
		Vector3f p2inv = cam ? cam->ScreenToWorldPoint(p2) : p2;

		if (colors == NULL)
			device.ImmediateColor (defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a);
		else 
			device.ImmediateColor (colors[0].r, colors[0].g, colors[0].b, colors[0].a);

		device.ImmediateTexCoord (0, offX, offY, 0);
		device.ImmediateVertex (p1inv.x, p1inv.y, p1inv.z);
		device.ImmediateTexCoord (0, offX, 1 + offY, 0);
		device.ImmediateVertex (p2inv.x, p2inv.y, p2inv.z);
	}

	for (size_t i = 1; i < count-1; i++)
	{
		Vector2f curr(screenSpace[i].x, screenSpace[i].y);
		Vector2f prev(screenSpace[i-1].x, screenSpace[i-1].y);
		Vector2f next(screenSpace[i+1].x, screenSpace[i+1].y);
		Vector2f difa = NormalizeFast(curr - prev) * width;
		Vector2f difb = NormalizeFast(next - curr) * width;
		if (colors != NULL)
			device.ImmediateColor (colors[i].r, colors[i].g, colors[i].b, colors[i].a);
		// two segments on one side
		{
			Vector2f pt1 = Vector2f(prev.x + difa.y, prev.y - difa.x);
			Vector2f pt2 = Vector2f(curr.x + difa.y, curr.y - difa.x);
			Vector2f pt3 = Vector2f(curr.x + difb.y, curr.y - difb.x);
			Vector2f pt4 = Vector2f(next.x + difb.y, next.y - difb.x);
			Vector2f isect;
			float factor = Intersect2DLines(pt1, pt2, pt3, pt4, isect);
			if (factor == 0.0f)
				isect = (pt2+pt3) * 0.5f;
			else
				isect = curr + NormalizeFast(isect - curr) * width;

			Vector3f iSect3 = Vector3f(isect.x, isect.y, screenSpace[i].z);
			Vector3f iSect3Inv = cam ? cam->ScreenToWorldPoint(iSect3) : iSect3;

			device.ImmediateTexCoord (0, offX, offY, 0);
			device.ImmediateVertex (iSect3Inv.x, iSect3Inv.y, iSect3Inv.z);
		}
		
		{
			Vector2f pt1 = Vector2f(prev.x - difa.y, prev.y + difa.x);
			Vector2f pt2 = Vector2f(curr.x - difa.y, curr.y + difa.x);
			Vector2f pt3 = Vector2f(curr.x - difb.y, curr.y + difb.x);
			Vector2f pt4 = Vector2f(next.x - difb.y, next.y + difb.x);
			Vector2f isect;
			float factor = Intersect2DLines(pt1, pt2, pt3, pt4, isect);
			if (factor == 0.0f)
				isect = (pt2+pt3) * 0.5f;
			else
				isect = curr + NormalizeFast(isect - curr) * width;

			Vector3f iSect3 = Vector3f(isect.x, isect.y, screenSpace[i].z);
			Vector3f iSect3Inv = cam ? cam->ScreenToWorldPoint(iSect3) : iSect3;

			device.ImmediateTexCoord (0, offX, 1 + offY, 0);
			device.ImmediateVertex (iSect3Inv.x, iSect3Inv.y, iSect3Inv.z);
		}
	}

	// end
	{
		Vector2f curr(screenSpace[count-2].x, screenSpace[count-2].y);
		Vector2f next(screenSpace[count-1].x, screenSpace[count-1].y);
		Vector2f dif = NormalizeFast(next-curr) * width;

		Vector3f p1(next.x + dif.y, next.y - dif.x, screenSpace[count-1].z);
		Vector3f p2(next.x - dif.y, next.y + dif.x, screenSpace[count-1].z);

		Vector3f p1inv = cam ? cam->ScreenToWorldPoint(p1) : p1;
		Vector3f p2inv = cam ? cam->ScreenToWorldPoint(p2) : p2;

		if (colors != NULL)
			device.ImmediateColor (colors[count-1].r, colors[count-1].g, colors[count-1].b, colors[count-1].a);

		device.ImmediateTexCoord (0, offX, offY, 0);
		device.ImmediateVertex (p1inv.x, p1inv.y, p1inv.z);
		device.ImmediateTexCoord (0, offX, 1 + offY, 0);
		device.ImmediateVertex (p2inv.x, p2inv.y, p2inv.z);
	}
	device.ImmediateEnd ();

}


void DrawWireArc( const Vector3f& center, const Vector3f& normal, const Vector3f& from, float angle, float radius, bool depthTest )
{
	const int kArcPoints = 60;
	static Vector3f s_Points[kArcPoints];
	SetDiscSectionPoints (s_Points, kArcPoints, center, normal, from, angle, radius);

	static Vector3f s_Vertices[kArcPoints];
	for( int i = 0; i < kArcPoints; ++i )
		s_Vertices[i] = gGizmoMatrix.MultiplyPoint3( s_Points[i] );

	gizmos::AddLinePrimitives( kPrimitiveLineStrip, kArcPoints, s_Vertices, depthTest );
}

void DrawWireSphere(const Vector3f &center, float radius)
{
	DrawWireArc (center, Vector3f(0,1,0), Vector3f(1,0,0), 360.0f, radius);
	DrawWireArc (center, Vector3f(0,0,1), Vector3f(1,0,0), 360.0f, radius);
	DrawWireArc (center, Vector3f(1,0,0), Vector3f(0,1,0), 360.0f, radius);
}

void DrawWireSphereTwoShaded(const Vector3f &center, float radius, const Quaternionf &rotation)
{
	const Camera& cam = GetCurrentCamera ();
	const Transform& tr = cam.GetComponent(Transform);
	Vector3f camPos = tr.GetPosition();
	
	Vector3f planeNormal;
	
	Vector3f DIRS[3] = {
		RotateVectorByQuat(rotation, Vector3f( 1, 0, 0)),
		RotateVectorByQuat(rotation, Vector3f( 0, 1, 0)),
		RotateVectorByQuat(rotation, Vector3f( 0, 0, 1)),
	};
	if (cam.GetOrthographic())
	{
		planeNormal = tr.TransformDirection(Vector3f(0,0,1));
		DrawWireDisk(center, planeNormal, radius);
		for (int i=0; i<3; i++)
		{
			Vector3f from = NormalizeSafe(Cross(DIRS[i], planeNormal));
			
			// we may have view dir locked to one axis
			if( SqrMagnitude(from) > 1e-6f )
				DrawWireDiskTwoShaded(center, DIRS[i], from, 180, radius);
		}
	}
	else
	{
		planeNormal = center - camPos;
		float sqrDist = SqrMagnitude(planeNormal);
		float sqrRadius = radius * radius;
		float sqrOffset = sqrRadius * sqrRadius / sqrDist;
		float insideAmount = sqrOffset / sqrRadius;
		if (insideAmount < 1)
		{
			float drawnRadius = Sqrt(sqrRadius - sqrOffset);
			DrawWireDisk(center - sqrRadius * planeNormal / sqrDist, planeNormal, drawnRadius);
			for (int i=0; i<3; i++)
			{
				float q = acos(Dot(planeNormal / Sqrt(sqrDist), DIRS[i]));
				q = kPI * 0.5f - min(q, kPI - q);
				float f = tan(q);
				float g = Sqrt(sqrOffset + f * f * sqrOffset) / radius;
				if (g < 1)
				{
					float e = asin(g);
					Vector3f from = NormalizeSafe(Cross(DIRS[i], planeNormal));
					from = RotateVectorByQuat (AxisAngleToQuaternionSafe(DIRS[i], e), from);
					DrawWireDiskTwoShaded(center, DIRS[i], from, (90 - Rad2Deg(e)) * 2.0f, radius);
				}
				else
				{
					DrawWireDiskTwoShaded(center, DIRS[i], radius);
				}
			}
		}
		else
		{
			for (int i=0; i<3; i++)
			{
				DrawWireDiskTwoShaded(center, DIRS[i], radius);
			}
		}
		
	}
}

void DrawWireCapsule(const Vector3f &center, float radius, float height)
{
	float halfHeight = height * 0.5F;
	// lower cap
	DrawWireArc( Vector3f(0,halfHeight,0), Vector3f(0,1,0), Vector3f(1,0,0),  360.0f, radius );
	DrawWireArc( Vector3f(0,halfHeight,0), Vector3f(1,0,0), Vector3f(0,0,1), -180.0f, radius );
	DrawWireArc( Vector3f(0,halfHeight,0), Vector3f(0,0,1), Vector3f(1,0,0),  180.0f, radius );

	// upper cap
	DrawWireArc( Vector3f(0,-halfHeight,0), Vector3f(0,1,0), Vector3f(1,0,0),  360.0f, radius );
	DrawWireArc( Vector3f(0,-halfHeight,0), Vector3f(1,0,0), Vector3f(0,0,1),  180.0f, radius );
	DrawWireArc( Vector3f(0,-halfHeight,0), Vector3f(0,0,1), Vector3f(1,0,0), -180.0f, radius );

	// side lines
	DrawLine( Vector3f(+radius, halfHeight, 0.0f), Vector3f(+radius, -halfHeight, 0.0f) );
	DrawLine( Vector3f(-radius, halfHeight, 0.0f), Vector3f(-radius, -halfHeight, 0.0f) );
	DrawLine( Vector3f(0.0f, halfHeight, +radius), Vector3f(0.0f, -halfHeight, +radius) );
	DrawLine( Vector3f(0.0f, halfHeight, -radius), Vector3f(0.0f, -halfHeight, -radius) );
}

void DrawWireCylinder(const Vector3f &center, float radius, float height)
{
	float halfHeight = height * 0.5F;
	// lower cap
	DrawWireArc( Vector3f(0,halfHeight,0), Vector3f(0,1,0), Vector3f(1,0,0),  360.0f, radius );
	
	// upper cap
	DrawWireArc( Vector3f(0,-halfHeight,0), Vector3f(0,1,0), Vector3f(1,0,0),  360.0f, radius );
	
	// side lines
	DrawLine( Vector3f(+radius, halfHeight, 0.0f), Vector3f(+radius, -halfHeight, 0.0f) );
	DrawLine( Vector3f(-radius, halfHeight, 0.0f), Vector3f(-radius, -halfHeight, 0.0f) );
	DrawLine( Vector3f(0.0f, halfHeight, +radius), Vector3f(0.0f, -halfHeight, +radius) );
	DrawLine( Vector3f(0.0f, halfHeight, -radius), Vector3f(0.0f, -halfHeight, -radius) );
}

void DrawWireDisk (const Vector3f &center, const Vector3f &normal, float radius)
{
	Vector3f tangent = Cross (normal, Vector3f(0,1,0));
	if (SqrMagnitude(tangent) < 0.001f)
		tangent = Cross (normal, Vector3f(1,0,0));
	DrawWireArc (center, normal, tangent, 360.0f, radius);
}

void DrawWireDiskTwoShaded (const Vector3f &center, const Vector3f &normal, const Vector3f& from, float angle, float radius)
{
	DrawWireArc(center, normal, from, angle, radius);
	
	ColorRGBAf color = gizmos::g_GizmoColor;
	ColorRGBAf colorOrig = color;
	color.a *= kBackfaceAlphaMultiplier;
	gizmos::g_GizmoColor = color;
	DrawWireArc(center, normal, from, angle - 360.0f, radius);
	gizmos::g_GizmoColor = colorOrig;
}

void DrawWireDiskTwoShaded (const Vector3f &center, const Vector3f &normal, float radius)
{
	ColorRGBAf color = gizmos::g_GizmoColor;
	ColorRGBAf colorOrig = color;
	color.a *= kBackfaceAlphaMultiplier;
	gizmos::g_GizmoColor = color;
	DrawWireDisk(center, normal, radius);
	gizmos::g_GizmoColor = colorOrig;
}

void DrawRawMesh (const Vector3f* vertices, const UInt32* indices, int triCount, bool depthTest)
{
	Vector3f trilines[6];

	for( int i = 0; i < triCount; ++i ) {
		Vector3f p0 = gGizmoMatrix.MultiplyPoint3(vertices[indices[0]]);
		Vector3f p1 = gGizmoMatrix.MultiplyPoint3(vertices[indices[1]]);
		Vector3f p2 = gGizmoMatrix.MultiplyPoint3(vertices[indices[2]]);
		trilines[0] = p0; trilines[1] = p1;
		trilines[2] = p1; trilines[3] = p2;
		trilines[4] = p2; trilines[5] = p0;
		gizmos::AddLinePrimitives( kPrimitiveLines, 6, trilines, depthTest );
		indices += 3;
	}
}


void DrawLine (const Vector3f &p1, const Vector3f &p2, bool depthTest) {
	Vector3f vertices[2];
	vertices[0] = gGizmoMatrix.MultiplyPoint3 (p1);
	vertices[1] = gGizmoMatrix.MultiplyPoint3 (p2);
	gizmos::AddLinePrimitives( kPrimitiveLines, 2, vertices, depthTest );
}

void DrawLine (const Vector3f &p1, const ColorRGBAf &col1, const Vector3f &p2, const ColorRGBAf &col2) {
	gizmos::ColorVertex vertices[2];
	vertices[0] = gizmos::ColorVertex( gGizmoMatrix.MultiplyPoint3 (p1), col1 );
	vertices[1] = gizmos::ColorVertex( gGizmoMatrix.MultiplyPoint3 (p2), col2 );
	gizmos::AddColorPrimitives( kPrimitiveLines, 2, vertices );
}

void DrawIcon (const Vector3f &center, const string& name, bool allowScaling /*= true*/, ColorRGBA32 tint /*= ColorRGBA32(255,255,255,255)*/)
{
	Texture2D* tex = Texture2DNamed (name);
	gizmos::AddIcon( center, tex, NULL, allowScaling, tint);
}

static inline float TriArea2D (float x1, float y1, float x2, float y2, float x3, float y3)
{
	return (x1-x2) * (y2-y3) - (x2-x3) * (y1-y2);
}

// Taken from Real-Time Collision Detection
// TODO: make it faster? Do we really need that Cross()? But we can't use a normalized even if we have it (we do for tetrahedra)
Vector3f BarycentricCoordinates3DTriangle (const Vector3f tri[3], const Vector3f& p)
{
	// Unnormalized(!) triangle normal
	Vector3f normal = Cross (tri[1] - tri[0], tri[2] - tri[0]);
	// Nominators and one-over-denominator for u and v ratios
	float nu, nv, ood;
	// Absolute components for determining projection plane
	float x = Abs (normal.x), y = Abs (normal.y), z = Abs (normal.z);
	
	// Compute areas in plane of largest projection
	if (x >= y && x >= z) {
		// x is largest, project to the yz plane
		nu = TriArea2D (p.y, p.z, tri[1].y, tri[1].z, tri[2].y, tri[2].z); // Area of PBC in yz plane
		nv = TriArea2D (p.y, p.z, tri[2].y, tri[2].z, tri[0].y, tri[0].z); // Area of PCA in yz plane
		ood = 1.0f / normal.x; // 1/(2*area of ABC in yz plane)
	} else if (y >= x && y >= z) {
		// y is largest, project to the xz plane
		nu = TriArea2D (p.x, p.z, tri[1].x, tri[1].z, tri[2].x, tri[2].z);
		nv = TriArea2D (p.x, p.z, tri[2].x, tri[2].z, tri[0].x, tri[0].z);
		ood = 1.0f / -normal.y;
	} else {
		// z is largest, project to the xy plane
		nu = TriArea2D (p.x, p.y, tri[1].x, tri[1].y, tri[2].x, tri[2].y);
		nv = TriArea2D (p.x, p.y, tri[2].x, tri[2].y, tri[0].x, tri[0].y);
		ood = 1.0f / normal.z;
	}
	return Vector3f (nu * ood, nv * ood, 1.0f - nu * ood - nv * ood);
}
