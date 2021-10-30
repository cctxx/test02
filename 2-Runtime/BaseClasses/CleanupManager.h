#pragma once

#if UNITY_EDITOR

#include "Runtime/BaseClasses/GameObject.h"

#include <string>
#include <list>

class CleanupManager
{
private:
	struct MarkedComponent {
		PPtr<Unity::Component> component;
		std::string reason;

		bool operator==(PPtr<Unity::Component> const& comp)
		{
			return this->component == comp;
		}
	};

public:
	CleanupManager() {}

	void MarkForDeletion(PPtr<Unity::Component> comp, std::string const& reason);
	void Flush();

	static void DidDestroyObjectNotification (Object* comp, void* userData);

private:
	std::list<struct MarkedComponent> m_markedComponents;
};

CleanupManager& GetCleanupManager ();

#endif