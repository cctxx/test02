#ifndef NAMEDKEYCONTROLLIST_H
#define NAMEDKEYCONTROLLIST_H

namespace IMGUI
{
	struct NamedControl
	{
		int ID;
		int windowID;
		NamedControl () 
		{ 
			ID = 0; 
			windowID = -1; 
		}
		NamedControl (int _ID, int _windowID)
		{
			ID = _ID;
			windowID = _windowID;
		}
	};
	
	class NamedKeyControlList
	{
	public:
		void AddNamedControl (const std::string &str, int id, int windowID);

		// Return ptr name of a given control, NULL if none.
		std::string GetNameOfControl (int id);
		
		// Return the ID of a named control.
		// Used in GUIState::FocusControl
		NamedControl* GetControlNamed (const std::string &name);
		
		void Clear () { m_NamedControls.clear (); }
	private:
		std::map<std::string, NamedControl> m_NamedControls;	
	};
};

#endif