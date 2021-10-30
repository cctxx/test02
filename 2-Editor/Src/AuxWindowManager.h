#pragma once
class GUIView;

class AuxWindowManager
{
public:
	static void Create ();
	static void Destroy ();

	void AddView (GUIView* view, GUIView* currentView);
	void RemoveView (GUIView* view);
	void OnLostFocus (GUIView* view);
	void OnGotFocus (GUIView* view);
	bool OnMouseDown (GUIView* view); // returns true if mouse down is stolen
	bool CloseViews (GUIView *stopClosingAtThisGUIView); // closes all views on the stack until 'stopClosingAtThisGUIView'. If 'stopClosingAtThisGUIView' is NULL then all views are closed.
	void Update ();
private:
	bool IsValidView (GUIView* view);
	bool IsViewOnStack (GUIView* view);
	void CloseViewsDelayed ();
	void CancelClosingViews ();
	
	AuxWindowManager ();

	std::vector<GUIView*> m_Views;
	GUIView* m_DoNotStealMouseDownFromThisView;
	double m_StartTimeClosingViews;
};
AuxWindowManager& GetAuxWindowManager();

