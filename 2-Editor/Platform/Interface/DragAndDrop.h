#pragma once

#include "Runtime/BaseClasses/BaseObject.h"

struct MonoObject;
#ifdef __OBJC__
@class NSArray;
@class NSPasteboard;
#endif
/*
class DragAndDropData
{
	public:
	
	virtual ~DragAndDropData ();
};

class MonoDragAndDropData : public DragAndDropData
{
	int m_MonoReference;

	public:
	
	MonoDragAndDropData (MonoObject* obj);
	
	MonoObject* GetMonoObject();
	virtual ~MonoDragAndDropData ();
};
*/

#if UNITY_WIN
class GUIView;
struct IDataObject;
struct DelayedDrag;
struct WindowsDropTargetImpl;
#endif


class DragAndDrop
{
public:
	
	DragAndDrop ();

	enum DragVisualMode
	{
		kDragOperationNone = 0,
		kDragOperationCopy = 1,
		kDragOperationLink = 2,
		kDragOperationGeneric = 4,
		kDragOperationMove = 16,
		kDragOperationRejected = 32
	};

	#ifdef __OBJC__
	void Setup (id sender);
	void Cleanup ();
	NSArray* GetAllSupportedTypes();
	#endif

	#if UNITY_WIN
	void* RegisterWindowForDrop( HWND window, GUIView* view );
	void UnregisterWindowForDrop( HWND window, void* dropData );

	void Setup();
	void Cleanup();
	bool FetchDataFromDrag( IDataObject* data );
	#endif
	
	DragVisualMode GetVisualMode () { return m_VisualMode; }
	void SetVisualMode (DragVisualMode mode) { m_VisualMode = mode; }
	void AcceptDrag () { m_AcceptedDrag = true; }
	bool GetDragAccepted () { return m_AcceptedDrag; }
		
	// Get / Set PPtrs from the drag&drop
	std::vector<PPtr<Object> > GetPPtrs();	
	void SetPPtrs(std::vector<PPtr<Object> >& objects);
	void SetPPtrs(std::vector<Object*>& objects) {std::vector<PPtr<Object> > pptr; pptr.assign(objects.begin(), objects.end()); SetPPtrs(pptr); }

	// Get / Set path names in the drag&drop
	std::vector<std::string> GetPaths();
	void SetPaths(const std::vector<std::string>& paths);
	
	MonoObject* GetMonoData (std::string type) { return NULL; }
	void SetMonoData (std::string type, MonoObject* obj) { }

	// Start a Drag&Drop operation
	void StartDrag (string title);
	void PrepareStartDrag ();

	// Cocoa/Windows starts an event loop directly upon initiating drag&drop.
	// This messes up the UnityGUI stack because it is not reentrant.
	// As a fix we start the drag after we exit the method.
	void ApplyQueuedStartDrag ();

	/// The control ID used during drag & drop. Used to specially highlight objects while dragging in the GUI framework.
	void SetActiveControlID (int con) { m_ActiveControlID = con; }
	int GetActiveControlID () { return m_ActiveControlID; }
	
private:
	#if UNITY_WIN
	void StartDelayedDrag( DelayedDrag& drag );
	#endif

private:
	DragVisualMode m_VisualMode;
	bool m_AcceptedDrag;
	int m_ActiveControlID;

	#if UNITY_OSX

		#ifdef __OBJC__
		NSPasteboard* m_PBoard;
		id           m_DelayedStartDrag;
		#else
		void* m_PBoard;
		void* m_DelayedStartDrag;
		#endif

	#elif UNITY_WIN

		DelayedDrag*	m_DelayedStartDrag;
		HGLOBAL			m_PPtrData;
		HGLOBAL			m_PathsData;
		HGLOBAL			m_FilesData;
		std::vector<std::string>	m_CurrentDragPaths;
		std::vector<PPtr<Object> >	m_CurrentDragPPtrs;

	#elif UNITY_LINUX

		std::vector<std::string>    m_CurrentDragPaths;
		std::vector<PPtr<Object> >  m_CurrentDragPPtrs;

	#else
		#error "Unknown platform"
	#endif
};

// Singleton accessor
DragAndDrop& GetDragAndDrop ();
