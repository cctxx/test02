#ifndef PICKING_H
#define PICKING_H

#include <set>
#include <vector>
#include "Runtime/Math/Rect.h"

class Object;
namespace Unity { class GameObject; }
class Transform;
class Camera;
class Vector2f;
class Vector3f;
template<class T>
class PPtr;
namespace Unity { class Material; }

using namespace Unity;

struct SelectionEntry
{
	Object* object;
	int score;
	friend bool operator < (const SelectionEntry& lhs, const SelectionEntry& rhs) { return lhs.object < rhs.object; }
};
typedef std::set<SelectionEntry> SelectionList;

/// Finds all Renderer that are within a rect centered around screenPosition with size screenSizePickRect
/// All results are written to selection and the number of objects that hit the rect is returned
/// Culls Renderers that are marked !ShouldDisplayInEditor
int PickObjects (Camera& camera, UInt32 layers, const Vector2f& screenPosition, const Vector2f& screenSizePickRect, SelectionList* selection);


// Returns the closest GameObject at screen position pos
Object* PickClosestObject (Camera& cam, UInt32 layers, const Vector2f& screenPosition);
GameObject* PickClosestGO (Camera& cam, UInt32 layers, const Vector2f& pos);

Material* GetPickMaterial();
Material *GetAlphaPickMaterial(); 

// Returns GameObjects in screen rect
void PickRectObjects (Camera& cam, const Rectf& rect, std::set<GameObject*>* result, bool pickPrefabParents);

bool FindNearestVertex (std::vector<Transform*> *objectsToSearchIn, std::set<Transform*> *ignoreObjects, Vector2f screenPoint, Camera &camera, Vector3f *point);

#endif
