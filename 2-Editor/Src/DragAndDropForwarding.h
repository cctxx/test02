#pragma once
#include <vector>
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Platform/Interface/DragAndDrop.h"

using std::vector;
class Object;
class DragAndDrop;
struct UnityGUID;
struct MonoObject;

enum DropResult { kRejectedDrop = 0, kDropUpon = 1 << 0, kDropAbove = 1 << 1 };
enum { kHasToBePrefabParent = 1 << 2 };

class DragAndDropForwarding
{
	public:
	struct DragInfo;
	
	typedef int DragAndDropCallback (Object* draggedUponObject, Object* draggedObject, DragInfo* pos);

	enum { kDragIntoSceneWindow = 0, kDragIntoInspectorWindow = 1, kDragIntoProjectWindow = 2, kDragIntoTransformHierarchy = 3 };
	DragAndDropForwarding (int mode);
	
	struct DragInfo
	{
		Vector3f position;
		Quaternionf rotation;
		Vector2f viewportPos;
		int options;

		DragInfo () { viewportPos = Vector2f(0.5, 0.5); options = 0; position = Vector3f(0,0,0); rotation = Quaternionf::identity ();}
	};
	
	/// Iterates through the registered drag forwarders and performs the drag.
	/// draggedUponObject can be NULL, draggedObject has to be non-NULL
	/// Returns: DropResult
	int ForwardDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragInfo* dragInfo);

	/// Forwards a drag and drop with possibly multiple objects in the drag.
	/// returns if it was successful or not
	/// On Return drag result contains every object and its drag type
	int ForwardDragAndDrop (Object* draggedUponObject, DragInfo* dragInfo = NULL);

	/// Returns whether the drag and drop can possibly be done.
	int CanHandleDragAndDrop (Object* draggedUponObject, Object* draggedObject);
	int CanHandleDragAndDrop (Object* draggedUponObject);

	/// Registers a DragAndDropCallback
	/// draggedUponObjectClassID is the classID of the object is to receive the drag (-1) if it is NULL (dropped on the scene)
	void RegisterForward (int draggedUponObjectClassID, int draggedClassID, DragAndDropCallback* callback, int canHandleResult, DragAndDropCallback* canHandleCallback = NULL);
	
	private:
	struct DragCallback
	{
		int 						draggedClassID;
		int 						draggedUponClassID;
		int						flags;
		DragAndDropCallback*	callback;
		DragAndDropCallback*	canHandleCallback;
	};
	
	int						m_Mode;
	vector<DragCallback>	m_Callbacks;

	enum DispatchType { kDispatchForward, kDispatchCanHandle };

	int DispatchDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragInfo* dragInfo, DispatchType dispatchType);
	bool CheckAndConfirmDragOutOfPrefab (Object* draggedUponObject, vector<PPtr<Object> >& objects);
};

DragAndDrop::DragVisualMode ProjectWindowDrag (const UnityGUID& guidAsset, const LibraryRepresentation* representation, bool perform);