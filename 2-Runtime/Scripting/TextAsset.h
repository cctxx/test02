#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Configuration/UnityConfigure.h"

#include "Runtime/Misc/Allocator.h"


class TextAsset : public NamedObject {
public:

	typedef UnityStr ScriptString;

	REGISTER_DERIVED_CLASS (TextAsset, NamedObject)
	DECLARE_OBJECT_SERIALIZE (TextAsset)

	TextAsset(MemLabelId label, ObjectCreationMode mode);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	// Set the script string.
	// Subclasses override this in order to implement compiling, etc...
	// @return whether the compilation succeeded
	virtual bool SetScript (const ScriptString& script, bool actuallyContainsBinaryData);
	virtual bool SetScript (const ScriptString& script);	

	// Get the script string.
	const ScriptString &GetScript () const { return m_Script; }
	
	void SetPathName (const std::string& path) { m_PathName = path; SetDirty ();  }
	const UnityStr& GetPathName () { return m_PathName; }
	
	virtual const UnityStr& GetScriptClassName() const;
	void SetScriptDontDirty (const ScriptString& script);

protected:

	UnityStr m_PathName;
  	ScriptString m_Script;
};
