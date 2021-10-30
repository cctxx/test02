#pragma once

#include <ShObjIdl.h>


#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__

typedef
enum TBPFLAG
{	TBPF_NOPROGRESS	= 0,
	TBPF_INDETERMINATE	= 0x1,
	TBPF_NORMAL	= 0x2,
	TBPF_ERROR	= 0x4,
	TBPF_PAUSED	= 0x8
} 	TBPFLAG;

const GUID IID_ITaskbarList3 = {0xea1afb91, 0x9e28, 0x4b86, {0x90, 0xe9,  0x9e, 0x9f, 0x8a, 0x5e, 0xef, 0xaf}};


MIDL_INTERFACE("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf")
ITaskbarList3 : public ITaskbarList2
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetProgressValue( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ ULONGLONG ullCompleted,
		/* [in] */ ULONGLONG ullTotal) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetProgressState( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ TBPFLAG tbpFlags) = 0;

	virtual HRESULT STDMETHODCALLTYPE RegisterTab( 
		/* [in] */ __RPC__in HWND hwndTab,
		/* [in] */ __RPC__in HWND hwndMDI) = 0;

	virtual HRESULT STDMETHODCALLTYPE UnregisterTab( 
		/* [in] */ __RPC__in HWND hwndTab) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetTabOrder( 
		/* [in] */ __RPC__in HWND hwndTab,
		/* [in] */ __RPC__in HWND hwndInsertBefore) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetTabActive( 
		/* [in] */ __RPC__in HWND hwndTab,
		/* [in] */ __RPC__in HWND hwndMDI,
		/* [in] */ DWORD dwReserved) = 0;

	virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ UINT cButtons,
		/* [size_is][in] */ __RPC__in_ecount_full(cButtons) LPVOID pButton) = 0;

	virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ UINT cButtons,
		/* [size_is][in] */ __RPC__in_ecount_full(cButtons) LPVOID pButton) = 0;

	virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ __RPC__in_opt HIMAGELIST himl) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ __RPC__in HICON hIcon,
		/* [string][unique][in] */ __RPC__in_opt_string LPCWSTR pszDescription) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [string][unique][in] */ __RPC__in_opt_string LPCWSTR pszTip) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip( 
		/* [in] */ __RPC__in HWND hwnd,
		/* [in] */ __RPC__in RECT *prcClip) = 0;

};

#endif 	/* __ITaskbarList3_INTERFACE_DEFINED__ */
