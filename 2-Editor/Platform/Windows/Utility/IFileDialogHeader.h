#pragma once

#include <ShObjIdl.h>

// Pretty much copied from <ShObjIdl.h>. That header only includes it if whole Vista API is included, whereas we only
// want this bit.

SHSTDAPI SHParseDisplayName(PCWSTR pszName, __in_opt IBindCtx *pbc, __out PIDLIST_ABSOLUTE *ppidl, SFGAOF sfgaoIn, __out_opt SFGAOF *psfgaoOut);
SHSTDAPI SHCreateShellItem(__in_opt PCIDLIST_ABSOLUTE pidlParent, __in_opt IShellFolder *psfParent, __in PCUITEMID_CHILD pidl, __out IShellItem **ppsi);

extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0144_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0144_v0_0_s_ifspec;

EXTERN_C const IID IID_IShellItemFilter;

MIDL_INTERFACE("2659B475-EEB8-48b7-8F07-B378810F48CF")
IShellItemFilter : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE IncludeItem( 
        /* [in] */ __RPC__in_opt IShellItem *psi) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE GetEnumFlagsForItem( 
        /* [in] */ __RPC__in_opt IShellItem *psi,
        /* [out] */ __RPC__out SHCONTF *pgrfFlags) = 0;
    
};
    

typedef enum tagFDE_OVERWRITE_RESPONSE
{	FDEOR_DEFAULT	= 0,
	FDEOR_ACCEPT	= 0x1,
	FDEOR_REFUSE	= 0x2
} 	FDE_OVERWRITE_RESPONSE;

typedef /* [v1_enum] */ 
enum tagFDE_SHAREVIOLATION_RESPONSE
{	FDESVR_DEFAULT	= 0,
	FDESVR_ACCEPT	= 0x1,
	FDESVR_REFUSE	= 0x2
} 	FDE_SHAREVIOLATION_RESPONSE;

typedef /* [v1_enum] */ 
enum tagFDAP
{	FDAP_BOTTOM	= 0,
	FDAP_TOP	= 0x1
} 	FDAP;

typedef struct _COMDLG_FILTERSPEC
{
	LPCWSTR pszName;
	LPCWSTR pszSpec;
} 	COMDLG_FILTERSPEC;

typedef struct tagMACHINE_ID
{
	char szName[ 16 ];
} 	MACHINE_ID;

typedef struct tagDOMAIN_RELATIVE_OBJECTID
{
	GUID guidVolume;
	GUID guidObject;
} 	DOMAIN_RELATIVE_OBJECTID;

typedef GUID KNOWNFOLDERID;


EXTERN_C const IID IID_IFileDialogEvents;


MIDL_INTERFACE("973510db-7d7f-452b-8975-74a85828d354")
IFileDialogEvents : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE OnFileOk( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnFolderChanging( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psiFolder) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnFolderChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnSelectionChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnShareViolation( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [out] */ __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnTypeChange( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnOverwrite( 
		/* [in] */ __RPC__in_opt IFileDialog *pfd,
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [out] */ __RPC__out FDE_OVERWRITE_RESPONSE *pResponse) = 0;

};

extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0119_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0119_v0_0_s_ifspec;

enum tagFILEOPENDIALOGOPTIONS
{	FOS_OVERWRITEPROMPT	= 0x2,
	FOS_STRICTFILETYPES	= 0x4,
	FOS_NOCHANGEDIR	= 0x8,
	FOS_PICKFOLDERS	= 0x20,
	FOS_FORCEFILESYSTEM	= 0x40,
	FOS_ALLNONSTORAGEITEMS	= 0x80,
	FOS_NOVALIDATE	= 0x100,
	FOS_ALLOWMULTISELECT	= 0x200,
	FOS_PATHMUSTEXIST	= 0x800,
	FOS_FILEMUSTEXIST	= 0x1000,
	FOS_CREATEPROMPT	= 0x2000,
	FOS_SHAREAWARE	= 0x4000,
	FOS_NOREADONLYRETURN	= 0x8000,
	FOS_NOTESTFILECREATE	= 0x10000,
	FOS_HIDEMRUPLACES	= 0x20000,
	FOS_HIDEPINNEDPLACES	= 0x40000,
	FOS_NODEREFERENCELINKS	= 0x100000,
	FOS_DONTADDTORECENT	= 0x2000000,
	FOS_FORCESHOWHIDDEN	= 0x10000000,
	FOS_DEFAULTNOMINIMODE	= 0x20000000,
	FOS_FORCEPREVIEWPANEON	= 0x40000000
} ;

EXTERN_C const IID IID_IFileDialog;

MIDL_INTERFACE("42f85136-db7e-439c-85f1-e4075d135fc8")
IFileDialog : public IModalWindow
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetFileTypes( 
		/* [in] */ UINT cFileTypes,
		/* [size_is][in] */ __RPC__in_ecount_full(cFileTypes) const COMDLG_FILTERSPEC *rgFilterSpec) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileTypeIndex( 
		/* [in] */ UINT iFileType) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFileTypeIndex( 
		/* [out] */ __RPC__out UINT *piFileType) = 0;

	virtual HRESULT STDMETHODCALLTYPE Advise( 
		/* [in] */ __RPC__in_opt IFileDialogEvents *pfde,
		/* [out] */ __RPC__out DWORD *pdwCookie) = 0;

	virtual HRESULT STDMETHODCALLTYPE Unadvise( 
		/* [in] */ DWORD dwCookie) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetOptions( 
		/* [in] */ DWORD fos) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetOptions( 
		/* [out] */ __RPC__out DWORD *pfos) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetDefaultFolder( 
		/* [in] */ __RPC__in_opt IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFolder( 
		/* [in] */ __RPC__in_opt IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolder( 
		/* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentSelection( 
		/* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileName( 
		/* [string][in] */ __RPC__in LPCWSTR pszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFileName( 
		/* [string][out] */ __RPC__deref_out_opt_string LPWSTR *pszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetTitle( 
		/* [string][in] */ __RPC__in LPCWSTR pszTitle) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetOkButtonLabel( 
		/* [string][in] */ __RPC__in LPCWSTR pszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFileNameLabel( 
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetResult( 
		/* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddPlace( 
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [in] */ FDAP fdap) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetDefaultExtension( 
		/* [string][in] */ __RPC__in LPCWSTR pszDefaultExtension) = 0;

	virtual HRESULT STDMETHODCALLTYPE Close( 
		/* [in] */ HRESULT hr) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetClientGuid( 
		/* [in] */ __RPC__in REFGUID guid) = 0;

	virtual HRESULT STDMETHODCALLTYPE ClearClientData( void) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetFilter( 
		/* [in] */ __RPC__in_opt IShellItemFilter *pFilter) = 0;

};

EXTERN_C const IID IID_IFileSaveDialog;

MIDL_INTERFACE("84bccd23-5fde-4cdb-aea4-af64b83d78ab")
IFileSaveDialog : public IFileDialog
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetSaveAsItem( 
		/* [in] */ __RPC__in_opt IShellItem *psi) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetProperties( 
		/* [in] */ __RPC__in_opt IPropertyStore *pStore) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetCollectedProperties( 
		/* [in] */ __RPC__in_opt IPropertyDescriptionList *pList,
		/* [in] */ BOOL fAppendDefault) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetProperties( 
		/* [out] */ __RPC__deref_out_opt IPropertyStore **ppStore) = 0;

	virtual HRESULT STDMETHODCALLTYPE ApplyProperties( 
		/* [in] */ __RPC__in_opt IShellItem *psi,
		/* [in] */ __RPC__in_opt IPropertyStore *pStore,
		/* [unique][in] */ __RPC__in_opt HWND hwnd,
		/* [unique][in] */ __RPC__in_opt IFileOperationProgressSink *pSink) = 0;

};

EXTERN_C const IID IID_IFileOpenDialog;

MIDL_INTERFACE("d57c7288-d4ad-4768-be02-9d969532d960")
IFileOpenDialog : public IFileDialog
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetResults( 
		/* [out] */ __RPC__deref_out_opt IShellItemArray **ppenum) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetSelectedItems( 
		/* [out] */ __RPC__deref_out_opt IShellItemArray **ppsai) = 0;

};

typedef /* [v1_enum] */ 
enum tagCDCONTROLSTATE
{	CDCS_INACTIVE	= 0,
	CDCS_ENABLED	= 0x1,
	CDCS_VISIBLE	= 0x2
} 	CDCONTROLSTATE;

typedef DWORD CDCONTROLSTATEF;

extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0122_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_shobjidl_0000_0122_v0_0_s_ifspec;


EXTERN_C const IID IID_IFileDialogCustomize;

MIDL_INTERFACE("e6fdd21a-163f-4975-9c8c-a69f1ba37034")
IFileDialogCustomize : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE EnableOpenDropDown( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddMenu( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddPushButton( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddComboBox( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddRadioButtonList( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddCheckButton( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel,
		/* [in] */ BOOL bChecked) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddEditBox( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddSeparator( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddText( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetControlLabel( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetControlState( 
		/* [in] */ DWORD dwIDCtl,
		/* [out] */ __RPC__out CDCONTROLSTATEF *pdwState) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetControlState( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ CDCONTROLSTATEF dwState) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetEditBoxText( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][out] */ __RPC__deref_out_opt_string WCHAR **ppszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetEditBoxText( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszText) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCheckButtonState( 
		/* [in] */ DWORD dwIDCtl,
		/* [out] */ __RPC__out BOOL *pbChecked) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetCheckButtonState( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ BOOL bChecked) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddControlItem( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem,
		/* [in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE RemoveControlItem( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem) = 0;

	virtual HRESULT STDMETHODCALLTYPE RemoveAllControlItems( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetControlItemState( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem,
		/* [out] */ __RPC__out CDCONTROLSTATEF *pdwState) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetControlItemState( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem,
		/* [in] */ CDCONTROLSTATEF dwState) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetSelectedControlItem( 
		/* [in] */ DWORD dwIDCtl,
		/* [out] */ __RPC__out DWORD *pdwIDItem) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetSelectedControlItem( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem) = 0;

	virtual HRESULT STDMETHODCALLTYPE StartVisualGroup( 
		/* [in] */ DWORD dwIDCtl,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

	virtual HRESULT STDMETHODCALLTYPE EndVisualGroup( void) = 0;

	virtual HRESULT STDMETHODCALLTYPE MakeProminent( 
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetControlItemText( 
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;

};



EXTERN_C const IID IID_IFileDialogControlEvents;

MIDL_INTERFACE("36116642-D713-4b97-9B83-7484A9D00433")
IFileDialogControlEvents : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE OnItemSelected( 
		/* [in] */ __RPC__in_opt IFileDialogCustomize *pfdc,
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ DWORD dwIDItem) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnButtonClicked( 
		/* [in] */ __RPC__in_opt IFileDialogCustomize *pfdc,
		/* [in] */ DWORD dwIDCtl) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnCheckButtonToggled( 
		/* [in] */ __RPC__in_opt IFileDialogCustomize *pfdc,
		/* [in] */ DWORD dwIDCtl,
		/* [in] */ BOOL bChecked) = 0;

	virtual HRESULT STDMETHODCALLTYPE OnControlActivating( 
		/* [in] */ __RPC__in_opt IFileDialogCustomize *pfdc,
		/* [in] */ DWORD dwIDCtl) = 0;

};

