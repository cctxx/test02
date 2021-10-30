#ifndef GIZMOUTIL_H
#define GIZMOUTIL_H

class Transform;
class Camera;
class Texture2D;
class Vector3f;
class Quaternionf;
class Ray;
class Matrix4x4f;
class Object;
class Shader;
struct InputEvent;
#include <string>
#include <set>
#include <vector>
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Utilities/GUID.h"

/// Get the bounds of the entire selection including transform children.
/// Returns AABB (infinity, infinity) if the selection does not contain any transforms
AABB GetSelectionAABB ();

/// Get the center of selection. (Does not look at transform children recursively)
Vector3f GetSelectionCenter (const std::set<Transform*>& selection);

enum { kTopLevelSelection = 1 << 0, kDeepSelection = 1 << 1, kExcludePrefabSelection = 1 << 2, kOnlyUserModifyableSelection = 1 << 3, kAssetsSelection = 1 << 4, kDeepAssetsSelection = 1 << 5  };
/// Returns a transform selection.
std::set<Transform*> GetTransformSelection (int mask = kTopLevelSelection | kExcludePrefabSelection | kOnlyUserModifyableSelection);

/// Calculate the world-space bounds of an object, ignoring objects without any graphics on them
/// found is set to true if it actually found something. 
MinMaxAABB CalculateObjectBounds (GameObject *go, bool onlyUsePivotForParticles, bool *found);
AABB CalculateSelectionBounds (bool onlyUsePivotForParticles);


/// Calculate Ray snap offset for a given gameobject along normal
float CalculateRaySnapOffset (GameObject *go, const Vector3f &normal);

/// Returns the entire selection.
/// mask can be: kExcludePrefabSelection or kOnlyUserModifyableSelection
void GetObjectSelection (int mask, TempSelectionSet& selection);

std::set<UnityGUID> GetSelectedAssets (int mask = 0);
UnityGUID GetSelectedAsset ();

/// Returns the entire selection including objects that can not be loaded.
/// This can be useful for asset data handling, if assets are no longer available but menu commands should still work for them.
std::set<PPtr<Object> > GetObjectSelectionPPtr ();

/// Returns the active transform. If the active object is a prefab or not modifyable null will be returned.
Transform* GetActiveTransform ();

/// Returns a set containing all transforms and all transform children of selection
std::set<Transform*> SelectionToDeepHierarchy (const std::set<Object*>& selection);
std::set<Transform*> SelectionToDeepHierarchy (const std::set<Transform*>& selection);

/// Get/Set the active GameObject
GameObject* GetActiveGO ();
Object* GetActiveObject ();
void SetActiveObject (Object* active);
void SetObjectSelection(const TempSelectionSet& selection);
void SetObjectSelection(const std::set<PPtr<Object> >& selection);

/// Get the selected game objects
std::set<GameObject*> GetGameObjectSelection (int mask = 0);

// Is the game object a child of a selected transform?
bool HasParentInSelection (GameObject& go);
bool HasParentInSelection (Transform& transform, const std::set<Transform*>& selection);
bool HasParentInSelection (GameObject& go, const std::set<Object*>& selection);




// ----------------------------------------------------------------------------
// PRIMITIVE DRAWING:


enum CapStyle {
	kCapNone, kCapBox, kCapCone, kCapDisk, kCapCircle, kCapRect
};


void SetGizmoMatrix (const Matrix4x4f &mat);
const Matrix4x4f &GetGizmoMatrix ();
void ClearGizmoMatrix ();

void DrawCap (CapStyle style, const Vector3f &center, const Vector3f  &dir, float size);
void DrawCube (const Vector3f &center, const Vector3f &size);
void DrawCube (const Vector3f &center, const Vector3f &size, const Vector3f &forwardDir);
void DrawWireCube (const Vector3f &center, const Vector3f &size, bool depthTest=true);
void DrawBezier(const Vector3f& startPosition, const Vector3f& endPosition, const Vector3f& startTangent, const Vector3f& endTangent, const ColorRGBAf& startColor, Texture2D *texture, float width);
void DrawCone (const Vector3f &basePoint, const Vector3f &topPoint, float radius = 32);
void DrawSphere (const Vector3f &center, float radius);
void DrawWireSphere(const Vector3f &center, float radius);
void DrawWireSphereTwoShaded(const Vector3f &center, float radius, const Quaternionf &rotation);
void DrawWireCapsule(const Vector3f &center, float radius, float height);
void DrawWireCylinder(const Vector3f &center, float radius, float height);
void DrawWireDisk (const Vector3f &center, const Vector3f &normal, float radius);
void DrawWireDiskTwoShaded( const Vector3f& center, const Vector3f& normal, const Vector3f& from, float angle, float radius );
void DrawWireDiskTwoShaded( const Vector3f& center, const Vector3f& normal, float radius );
void DrawLine (const Vector3f &p11, const Vector3f &p2, bool depthTest=true);
void DrawLine (const Vector3f &p1, const ColorRGBAf &col1, const Vector3f &p2, const ColorRGBAf &col2);
void DrawWireRect (const Vector3f &center, float radius, bool depthTest=true);
void DrawWireCircle (const Vector3f &center, float radius);
void DrawFrustum (const Vector3f &center, float fov, float maxRange, float minRange, float aspect);
void DrawCircleFrustum (const Vector3f &center, float fov, float maxRange);
void DrawIcon (const Vector3f &center, const std::string& name, bool allowScaling = true, ColorRGBA32 tint = ColorRGBA32(255,255,255,255));
void DrawRawMesh (const Vector3f* vertices, const UInt32* indices, int triCount, bool depthTest=true);
void DrawWireArc( const Vector3f& center, const Vector3f& normal, const Vector3f& from, float angle, float radius, bool depthTest=true );
void DrawAAPolyLine (size_t count, const Vector3f* points3, const ColorRGBAf* colors, ColorRGBAf defaultColor, Texture2D *texture, float width);

// Hit test 
float HitCap (CapStyle style, const Vector3f &center, const Vector3f  &dir, float size, const InputEvent &event);
float DistanceToLine (const Vector3f &p1, const Vector3f &p2, const InputEvent &event);
float DistanceToCircle (const Vector3f &center, float radius, const InputEvent &event);
float DistancePointPolyLine (const Vector3f &point, size_t count, const Vector3f* vertices); 
float DistancePointBezier (const Vector3f &point, const Vector3f& startPosition, const Vector3f& endPosition, const Vector3f& startTangent, const Vector3f& endTangent);

float CalcHandleSize (const Vector3f &pos, const Camera &cam);
float CalcHandleSize (const Vector3f &pos);

Vector3f BarycentricCoordinates3DTriangle (const Vector3f tri[3], const Vector3f& p);

#endif
