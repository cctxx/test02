#include "UnityPrefix.h"
#include "Editor/Platform/Interface/DragAndDrop.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "DragAndDropImpl.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/Undo/UndoManager.h"
#include <shlobj.h>


using namespace std;

// ----------------------------------------------------------------------


struct WindowsDropTargetImpl : public IWindowsDropTarget {
	virtual bool UnderstandsData( IDataObject* data );
	virtual DWORD DragUpdated( void* userData, DWORD keyState, const POINTL& pt );
	virtual void DragPerformed( void* userData, DWORD keyState, const POINTL& pt );
	virtual void DragExited( void* userData );

	DWORD DragEvent( void* userData, DWORD keyState, const POINTL& pt, int eventType );
};

WindowsDropTargetImpl s_DropTargetImpl;

bool WindowsDropTargetImpl::UnderstandsData( IDataObject *dataObject )
{
	return GetDragAndDrop().FetchDataFromDrag( dataObject );
}


DWORD WindowsDropTargetImpl::DragEvent( void* userData, DWORD keyState, const POINTL& pt, int eventType )
{
/*
	DWORD dwEffect = 0;

	// 2. work out that the drop-effect should be based on grfKeyState
	if(grfKeyState & MK_CONTROL)
	{
		dwEffect = dwAllowed & DROPEFFECT_COPY;
	}
	else if(grfKeyState & MK_SHIFT)
	{
		dwEffect = dwAllowed & DROPEFFECT_MOVE;
	}

	// 3. no key-modifiers were specified (or drop effect not allowed), so
	//    base the effect on those allowed by the dropsource
	if(dwEffect == 0)
	{
		if(dwAllowed & DROPEFFECT_COPY) dwEffect = DROPEFFECT_COPY;
		if(dwAllowed & DROPEFFECT_MOVE) dwEffect = DROPEFFECT_MOVE;
	}

	return dwEffect;
	*/
	
	GUIView* view = reinterpret_cast<GUIView*>( userData );
	AssertIf( !view );

	GetUndoManager().RevertAllInCurrentGroup();

	// Drag and Drop
	DragAndDrop& dnd = GetDragAndDrop();
	int oldVisualMode = dnd.GetVisualMode ();
	dnd.Setup();
	view->DragEvent( pt.x, pt.y, keyState, eventType );
	
	if (eventType == InputEvent::kDragPerform)
		GetUndoManager().IncrementCurrentGroup();
	else if (eventType == InputEvent::kDragExited)
		GetUndoManager().RevertAllInCurrentGroup();


	if (oldVisualMode != dnd.GetVisualMode())
		view->RequestRepaint();

	// When DoDragDrop is called, a modal message loop is entered which monitors mouse and keyboard messages. 
	// To ensure we can animate e.g particle systems in this state we call TickTimer which handles updating and repainting.
	GetApplication().TickTimer();

	if (eventType == InputEvent::kDragExited)
		dnd.Cleanup();
	
	switch( dnd.GetVisualMode() )
	{
	case DragAndDrop::kDragOperationCopy: return DROPEFFECT_COPY;
	case DragAndDrop::kDragOperationLink: return DROPEFFECT_LINK;
	case DragAndDrop::kDragOperationGeneric: return DROPEFFECT_COPY;
	case DragAndDrop::kDragOperationMove: return DROPEFFECT_MOVE;
	default: return DROPEFFECT_NONE;
	}
}

DWORD WindowsDropTargetImpl::DragUpdated( void* userData, DWORD keyState, const POINTL& pt )
{
	return DragEvent(userData, keyState, pt, InputEvent::kDragUpdated);
}

void WindowsDropTargetImpl::DragExited( void* userData )
{
	POINTL pt = {0, 0};
	DragEvent(userData, 0, pt, InputEvent::kDragExited);
	GetDragAndDrop().SetActiveControlID(0);
}

void WindowsDropTargetImpl::DragPerformed( void* userData, DWORD keyState, const POINTL& pt )
{
	DragEvent(userData, keyState, pt, InputEvent::kDragPerform);

	//@TODO: On os x result = false will cause a sliding cursor.
	// Doesn't this exist on windows?
	bool result = GetDragAndDrop().GetDragAccepted();
	
	// after drag is finished, send drag exited message to match OS X behaviour
	DragExited(userData);

	// TODO: free data object
	/*

	FORMATETC fmtetc = { GetClipboardFormatPPtr(), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed;

	if( dataObject->QueryGetData(&fmtetc) == S_OK )
	{
		if( dataObject->GetData(&fmtetc, &stgmed) == S_OK )
		{
			void* data = GlobalLock( stgmed.hGlobal );

			//SetWindowText(hwnd, (char *)data);

			GlobalUnlock( stgmed.hGlobal );

			// release the data using the COM API
			ReleaseStgMedium( &stgmed );
		}
	}
	*/
}

// ----------------------------------------------------------------------



// Singleton
static DragAndDrop* gSingleton = NULL;
DragAndDrop& GetDragAndDrop ()
{
	if (gSingleton == NULL)
		gSingleton = new DragAndDrop();
	return *gSingleton;
}

void* DragAndDrop::RegisterWindowForDrop( HWND window, GUIView* view )
{
	IDropTarget* drop = NULL;
	RegisterDropWindow( window, s_DropTargetImpl, view, &drop );
	AssertIf( !drop );
	return drop;
}

void DragAndDrop::UnregisterWindowForDrop( HWND window, void* dropData )
{
	AssertIf( !dropData );
	IDropTarget* drop = reinterpret_cast<IDropTarget*>( dropData );
	UnregisterDropWindow( window, drop );
}


// ----------------------------------------------------------------------

struct DelayedDrag {
	std::string	title;
	HWND		window;
	POINT		location;
};


// ----------------------------------------------------------------------


DragAndDrop::DragAndDrop ()
:	m_DelayedStartDrag(NULL)
,	m_PPtrData(NULL)
,	m_PathsData(NULL)
,	m_FilesData(NULL)
{
}

bool DragAndDrop::FetchDataFromDrag( IDataObject* data )
{
	bool understands = false;

	// read PPtr data
	m_CurrentDragPPtrs.clear();
	FORMATETC fmtPPtr = { GetClipboardFormatPPtr(), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	if( data->QueryGetData( &fmtPPtr ) == S_OK )
	{
		STGMEDIUM stgmed;
		if( data->GetData( &fmtPPtr, &stgmed ) == S_OK )
		{
			understands |= true;

			const SInt32* ptr = reinterpret_cast<const SInt32*>( GlobalLock( stgmed.hGlobal ) );
			if( ptr ) {
				int count = *ptr++;
				m_CurrentDragPPtrs.reserve( count );
				for( int i = 0; i < count; ++i ) {
					int id = *ptr++;
					m_CurrentDragPPtrs.push_back( PPtr<Object>(id) );
				}
				GlobalUnlock( stgmed.hGlobal );
			}
			ReleaseStgMedium(&stgmed);
		}
	}

	// read paths data
	m_CurrentDragPaths.clear();
	FORMATETC fmtPaths = { GetClipboardFormatPaths(), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	if( data->QueryGetData( &fmtPaths ) == S_OK )
	{
		STGMEDIUM stgmed;
		if( data->GetData( &fmtPaths, &stgmed ) == S_OK )
		{
			understands |= true;

			const UInt8* ptr = reinterpret_cast<const UInt8*>( GlobalLock( stgmed.hGlobal ) );
			if( ptr ) {
				int count = *(const SInt32*)ptr; ptr += sizeof(SInt32);
				m_CurrentDragPaths.reserve( count );
				for( int i = 0; i < count; ++i ) {
					int length = *(const SInt32*)ptr; ptr += sizeof(SInt32);
					std::string path( (const char*)ptr, length );
					std::string relativePath = GetProjectRelativePath(path);
					if( !relativePath.empty() )
						path = relativePath;
					m_CurrentDragPaths.push_back( path );
					ptr += length;
				}
				GlobalUnlock( stgmed.hGlobal );
			}
			ReleaseStgMedium(&stgmed);
		}
	}

	// read file drag data
	if (!understands)
	{
		FORMATETC fmtFiles = { GetClipboardFormatFiles(), 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		if( data->QueryGetData( &fmtFiles ) == S_OK )
		{
			STGMEDIUM stgmed;
			if( data->GetData( &fmtFiles, &stgmed ) == S_OK )
			{
				understands |= true;

				const DROPFILES* ptr = reinterpret_cast<const DROPFILES*>( GlobalLock( stgmed.hGlobal ) );
				if( ptr )
				{
					if (ptr->fWide)
					{
						int offset = ptr->pFiles;

						while (((char*)ptr+offset)[0] != 0)
						{
							string path;
							wchar_t *widePath = (wchar_t*)((char*)ptr+offset);

							ConvertWindowsPathName(widePath, path);
							std::string relativePath = GetProjectRelativePath(path);
							if( !relativePath.empty() )
								path = relativePath;
							m_CurrentDragPaths.push_back( path );
							offset += wcslen(widePath)*sizeof(wchar_t) + 2;
						}
					}
					else
					{
						// ansi path names are not handled by unity.
						// this usually indicates some sort of drop source error (see bug 213936)
					}

					GlobalUnlock( stgmed.hGlobal );
				}
				ReleaseStgMedium(&stgmed);
			}
		}
	}

	return understands;
}

void DragAndDrop::Setup()
{
	m_VisualMode = kDragOperationNone;
	m_AcceptedDrag = false;
	SetActiveControlID(0);
}

void DragAndDrop::Cleanup()
{
	m_CurrentDragPaths.clear();
	m_CurrentDragPPtrs.clear();
}


// PPtr data format:
// 4 bytes: number of PPtrs
// 4 bytes * number: PPtr instance IDs

std::vector<PPtr<Object> > DragAndDrop::GetPPtrs()
{
	return m_CurrentDragPPtrs;
}

void DragAndDrop::SetPPtrs(std::vector<PPtr<Object> >& objects)
{
	if( m_PPtrData ) {
		GlobalFree( m_PPtrData );
		m_PPtrData = NULL;
	}

	m_PPtrData = GlobalAlloc( GHND, sizeof(SInt32) + objects.size()*sizeof(SInt32) );
	if( !m_PPtrData ) {
		ErrorString( "Failed to set PPtrs for drag and drop" );
		return;
	}

	SInt32* ptr = reinterpret_cast<SInt32*>( GlobalLock( m_PPtrData ) );
	if( !ptr ) {
		ErrorString( "Failed to lock PPtr memory for drag and drop" );
		return;
	}
	*ptr++ = objects.size();
	for( int i = 0; i < objects.size(); ++i ) {
		*ptr++ = objects[i].GetInstanceID();
	}
	GlobalUnlock( m_PPtrData );
}


// Paths data format:
// 4 bytes: number of paths
// For each path: 4 bytes length in bytes, and the string itself (without zero terminator)

std::vector<std::string> DragAndDrop::GetPaths()
{
	return m_CurrentDragPaths;
}

void DragAndDrop::SetPaths(const std::vector<std::string>& paths)
{
	if( m_PathsData ) {
		GlobalFree( m_PathsData );
		m_PathsData = NULL;
	}

	int i;

	// put absolute paths into drag and drop data
	std::vector<std::string> absolutePaths;
	int n = paths.size();
	absolutePaths.reserve(n);
	for( i = 0; i < n; ++i )
	{
		if (!paths[i].empty())
			absolutePaths.push_back(PathToAbsolutePath(paths[i]));
	}

	if (absolutePaths.size() == 0)
		return;

	// calculate size
	int size = sizeof(SInt32);
	for( i = 0; i < absolutePaths.size(); ++i )
	{
		size += sizeof(SInt32) + absolutePaths[i].size();
	}
	m_PathsData = GlobalAlloc( GHND, size );
	if( !m_PathsData )
	{
		ErrorString( "Failed to set paths for drag and drop" );
		return;
	}

	// put strings into memory
	UInt8* ptr = reinterpret_cast<UInt8*>( GlobalLock( m_PathsData ) );
	if( !ptr ) {
		ErrorString( "Failed to lock memory for paths drag and drop" );
		return;
	}
	*(SInt32*)ptr = n;
	ptr += sizeof(SInt32);
	for( i = 0; i < n; ++i ) {
		int length = absolutePaths[i].size();
		*(SInt32*)ptr = length;
		ptr += sizeof(SInt32);
		memcpy( ptr, absolutePaths[i].c_str(), length );
		ptr += length;
	}
	GlobalUnlock( m_PathsData );
}

void DragAndDrop::StartDelayedDrag( DelayedDrag& drag )
{
	#pragma message("! Generate drag image from title!")

	HRESULT hr;

	STGMEDIUM storage[] = {
		{ TYMED_HGLOBAL, { NULL }, NULL },
		{ TYMED_HGLOBAL, { NULL }, NULL },
		{ TYMED_HGLOBAL, { NULL }, NULL }, 
	};

	FORMATETC formats[3];
	int idx = 0;

	if (m_PPtrData)
	{
		storage[idx].hGlobal = m_PPtrData;
		FORMATETC f = {GetClipboardFormatPPtr(), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		formats[idx++] = f;
	}

	if (m_PathsData)
	{
		storage[idx].hGlobal = m_PathsData;
		FORMATETC f = { GetClipboardFormatPaths(), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		formats[idx++] = f;
	}

	if (m_FilesData)
	{
		storage[idx].hGlobal = m_FilesData; 
		FORMATETC f = { GetClipboardFormatFiles(), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		formats[idx++] = f;
	}

	// Create data and drop source COM objects
	IDropSource* pDropSource;
	hr = CreateDropSource( &pDropSource );
	AssertIf( FAILED(hr) );

	IDataObject* pDataObject;
	hr = CreateDataObject( formats, storage, idx, &pDataObject );
	AssertIf( FAILED(hr) );

	// Start drag & drop operation
	// Note: When DoDragDrop is called, a modal message loop is entered which monitors mouse and keyboard messages. 
	DWORD dwEffect;
	DWORD dwResult = DoDragDrop( pDataObject, pDropSource, DROPEFFECT_COPY|DROPEFFECT_MOVE|DROPEFFECT_LINK, &dwEffect );

	// success!
	if(dwResult == DRAGDROP_S_DROP)
	{
		if(dwEffect & DROPEFFECT_MOVE)
		{
			// remove selection from edit control
		}
	}
	// canceled
	else if(dwResult == DRAGDROP_S_CANCEL)
	{
	}

	pDataObject->Release();
	pDropSource->Release();

	//ReleaseCapture();
}


void DragAndDrop::ApplyQueuedStartDrag ()
{
	if( m_DelayedStartDrag )
	{
		// Make sure we don't start the same drag multiple times
		DelayedDrag* drag = m_DelayedStartDrag;
		m_DelayedStartDrag = NULL;

		// Start the actual drag
		GetApplication().ResetReloadAssemblies();
		StartDelayedDrag( *drag );
		delete drag;
	}
}
 
void DragAndDrop::StartDrag (string title)
{
	if( m_DelayedStartDrag != NULL )
	{
		ErrorString( "Starting multiple Drags. You can only start one drag at a time!" );
		return;
	}

	DelayedDrag* drag = new DelayedDrag();
	drag->title = title;
	drag->window = NULL;
	drag->location.x = 0;
	drag->location.y = 0;
	m_DelayedStartDrag = drag;
}

void DragAndDrop::PrepareStartDrag ()
{

}
