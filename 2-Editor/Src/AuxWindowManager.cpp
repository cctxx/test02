#include "UnityPrefix.h"
#include "Editor/Src/AuxWindowManager.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Editor/Platform/Interface/ColorPicker.h"
#include "Runtime/Input/TimeManager.h"


/*
Test the following when making changes to the system:

To test do the following for all the numbers below:
-Open IconSelector (click the GameObject icon in the inspector of the MainCamera)
-Open ObjectSelctor from IconSelector (click 'Other...' in IconSelector)

1)
- Click in IconSelector (ObjectSelector should close and IconSelector should stay)

2)
- Click on window frame bar (Both views should close)

3) 
- Click on menubar, e.g 'Help'. (Both views should close on Windows)
*/

namespace 
{
	ContainerWindow::ShowMode GetShowMode (GUIView* view)
	{
		if (const ContainerWindow* c = view->GetWindow () )
			return c->GetShowMode ();
		else
			ErrorString ("Error: Could not find ContainerWindow (AuxWindowManager)");

		return ContainerWindow::kShowNormalWindow;
	}

	void RequestCloseView (GUIView *view)
	{
		view->RequestClose ();
	}

	const double s_DelayTimeInSeconds = 0.05;

	//#define DEBUGLOG(x) {LogString(x);}
	#define DEBUGLOG(x) {;}
}




AuxWindowManager* g_AuxWindowManager = NULL;

AuxWindowManager& GetAuxWindowManager ()
{
	if (g_AuxWindowManager == NULL)
		AuxWindowManager::Create ();
	return *g_AuxWindowManager;
}


void AuxWindowManager::Create ()
{
	Destroy ();
	g_AuxWindowManager = new AuxWindowManager ();
};

void AuxWindowManager::Destroy ()
{
	delete g_AuxWindowManager;
	g_AuxWindowManager = NULL;
};

AuxWindowManager::AuxWindowManager ()
{
	m_DoNotStealMouseDownFromThisView = NULL;
	m_StartTimeClosingViews = 0.0;
}


bool AuxWindowManager::IsValidView (GUIView* view)
{
	ContainerWindow::ShowMode showMode = GetShowMode (view);
	return (showMode == ContainerWindow::kShowPopupMenu || 
			showMode == ContainerWindow::kShowAuxWindow || 
			showMode == ContainerWindow::kShowUtility);
}


void AuxWindowManager::AddView (GUIView* view, GUIView* currentView)
{
	// Ensure that we only add valid views
	Assert (IsValidView (view));
	if (!IsValidView (view))
		return;

	// When adding the first aux window we register current gui view
	// This lets us allow mousedown event to go directly to widgets within same guiview but
	// get stolen for other guiviews.
	if (m_Views.empty ())
		m_DoNotStealMouseDownFromThisView = currentView;

	// Add to list if not already in it
	if (!IsViewOnStack (view))
		m_Views.push_back (view);
	
	// Ignore lost focus since we might have added a new window on the stack
	DEBUGLOG ("CancelClosingViews when AddView");
	CancelClosingViews ();
}

void AuxWindowManager::RemoveView (GUIView* view)
{
	std::vector<GUIView *>::iterator result;
	result = std::find(m_Views.begin(), m_Views.end(), view);
	if (result != m_Views.end())
		m_Views.erase (result);
}

bool AuxWindowManager::IsViewOnStack (GUIView* view)
{
	std::vector<GUIView *>::iterator result = std::find(m_Views.begin(), m_Views.end(), view);
	return (result != m_Views.end());
}


// OnMouseDown will close views on the stack down to the view that got clicked.
// If a delayed closing of all views on the stack was scheduled by OnLostFocus,
// that will be canceled ínside CloseViews.
// Returns true if mouse down is stolen.
bool AuxWindowManager::OnMouseDown (GUIView* view)
{
	DEBUGLOG ("OnMouseDown");
	if (m_Views.empty())
	{
		m_DoNotStealMouseDownFromThisView = NULL;
		return false;
	}

	// Instantly remove views on mouse down
	bool stealMouseDown = false;
	if (CloseViews (view))
	{
		if (m_DoNotStealMouseDownFromThisView != view)
			stealMouseDown = true;
	}

	if (m_Views.empty ())
		m_DoNotStealMouseDownFromThisView = NULL;

	return stealMouseDown;
}


void AuxWindowManager::OnGotFocus (GUIView* view)
{
	DEBUGLOG ("OnGotFocus");
}

// OnLostFocus basically handles when focus changes was not caused by mouse down events inside views.
// E.g clicking outside the Unity app, clicking the menubar etc.
// In OnLostFocus the closing of views is delayed in order to prevent views to
// be closed if a new view was added to the stack or if we click inside another view on the stack.
// In those cases OnMouseDown will cancel the delayed closing of all views on the stack and will
// instead only close views on the stack down to the view that got clicked.
void AuxWindowManager::OnLostFocus (GUIView* view)
{
	#if UNITY_OSX
	// The OSX ColorPicker does not use the aux windowmanager system
	// and when it opens and gets focus it issues an OnLostFocus to any aux windows that may be
	// open and therefore closes it. This is a problem for the GradientPicker Window.
	// To prevent this we make sure AuxWindows are not affected when the OSX ColorPicker
	// is visible. We assume that if there are AuxWindows open when the OSX ColorPicker is 
	// visible is must have been opened from within a AuxWindow and therefore this Aux Window should not be closed.
	if (OSColorPickerIsVisible ())
		return;
	#endif
	
	DEBUGLOG ("OnLostFocus");
	if (!m_Views.empty())
	{	
		if (IsViewOnStack (view))
			CloseViewsDelayed ();
	}
}

void AuxWindowManager::CloseViewsDelayed ()
{
	DEBUGLOG ("CloseViewsDelayed");
	m_StartTimeClosingViews = GetTimeSinceStartup () + s_DelayTimeInSeconds;
}

void AuxWindowManager::CancelClosingViews ()
{
	m_StartTimeClosingViews = 0.0;
}

void AuxWindowManager::Update ()
{
	if (m_StartTimeClosingViews > 0.0)
	{
		if (GetTimeSinceStartup () > m_StartTimeClosingViews)
		{
			CloseViews (NULL); // NULL ensures we close all views
			CancelClosingViews ();
		}	
	}
}

// Returns true if we closed any views
bool AuxWindowManager::CloseViews (GUIView *stopClosingAtThisGUIView)
{
	if (m_Views.empty ())
		return false;
	
	// Pop back until we meet 'oneThatHasFocus'
	bool closedAnyViews = false;
	while (!m_Views.empty() && m_Views.back() != stopClosingAtThisGUIView)
	{
		closedAnyViews = true;
		RequestCloseView (m_Views.back());
		DEBUGLOG (Format("CloseViews %d", stopClosingAtThisGUIView));
	}

	// Ensure to clean up so if we requested a delayed close from lost focus is cancelled
	CancelClosingViews ();
	
	return closedAnyViews;
}
