#pragma once

// Implementation mostly based on OLE Drag and Drop tutorials from Catch22:
// http://catch22.net/tuts/dragdrop.asp


#include <ObjIdl.h>


void InitializeDragAndDrop();
void ShutdownDragAndDrop();

HRESULT CreateDataObject( FORMATETC *fmtetc, STGMEDIUM *stgmeds, UINT count, IDataObject **ppDataObject );
HRESULT CreateDropSource( IDropSource **ppDropSource );

struct IWindowsDropTarget {
	virtual bool UnderstandsData( IDataObject* data ) = 0;
	virtual DWORD DragUpdated( void* userData, DWORD keyState, const POINTL& pt ) = 0;
	virtual void DragPerformed( void* userData, DWORD keyState, const POINTL& pt ) = 0;
	virtual void DragExited( void* userData ) = 0;
};



void RegisterDropWindow( HWND hwnd, IWindowsDropTarget& dragInterface, void* userData, IDropTarget **ppDropTarget );
void UnregisterDropWindow( HWND hwnd, IDropTarget *pDropTarget );

UInt32 GetClipboardFormatPPtr();
UInt32 GetClipboardFormatPaths();
UInt32 GetClipboardFormatFiles();
