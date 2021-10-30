#pragma once

#include "Runtime/Modules/ExportModules.h"

class EXPORT_COREMODULE CallbackArray
{
	public:

	typedef void SimpleCallback ();
	
	CallbackArray ();
	
	void Register (SimpleCallback* callback);
	void Unregister (SimpleCallback* callback);
	void Invoke ();
	
	private:
	
	enum { kMaxCallback = 6 };
	SimpleCallback* m_Callbacks[kMaxCallback];
};
