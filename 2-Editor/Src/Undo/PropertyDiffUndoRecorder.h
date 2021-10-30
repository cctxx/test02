#pragma once
#include "PropertyDiffUndoUtilities.h"
#include "UndoPropertyModification.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Allocator/STLAllocator.h"
#include <list>

class PropertyDiffUndoRecorder
{
public:
	PropertyDiffUndoRecorder();

	void RecordObject(Object* target, const UnityStr& actionName);
	void RecordObjects(Object** object, int object_count, const UnityStr& actionName);
	void Flush();

	static void StaticInitialize();
	static void StaticDestroy();

	// Postprocessing callback (The animation window extracts PropertyModifications from here
	// and routes it through animation mode instead)
	typedef void PostprocessCallback (UndoPropertyModifications& modifications);
	void RegisterPostprocessCallback (PostprocessCallback* callback) { m_PostprocessCallback = callback; }

private:

	std::list<RecordedObject>					m_CurrentlyRecording;
	UnityStr									m_CurrentRecordingName;
	PostprocessCallback*						m_PostprocessCallback;
};

PropertyDiffUndoRecorder& GetPropertyDiffUndoRecorder();


void RecordUndoDiff (Object** o, int size, const std::string& actionName);
void RecordUndoDiff (Object* o, const std::string& actionName);