#ifndef USERAUTHORIZATIONMANAGER_H
#define USERAUTHORIZATIONMANAGER_H

#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Misc/AsyncOperation.h"

class UserAuthorizationManager;

UserAuthorizationManager &GetUserAuthorizationManager();

class UserAuthorizationManager {
public:
	UserAuthorizationManager ();
	
	enum Mode {
		kNone = 0,
		kWebCam = 1 << 0,
		kMicrophone = 1 << 1
	};
	
	void Reset ();
	AsyncOperation *RequestUserAuthorization (int mode);
	void ReplyToUserAuthorizationRequest (bool reply, bool remember = false);
	bool HasUserAuthorization (int mode) const  { return (m_AuthorizationMode & mode) == mode; }
	
	int GetAuthorizationRequest() const { return m_AuthorizationRequest; }
	
	MonoBehaviour *GetAuthorizationDialog ();
private:
	class UserAuthorizationManagerOperation : public AsyncOperation
	{
		virtual float GetProgress () { return 0.0f; }
		virtual bool IsDone () { return GetUserAuthorizationManager().m_AuthorizationRequest == 0; }
	};

	class UserAuthorizationManagerErrorOperation : public AsyncOperation
	{
		virtual float GetProgress () { return 0.0f; }
		virtual bool IsDone () { return true; }
	};

	int m_AuthorizationMode;
	int m_AuthorizationRequest;
	UserAuthorizationManagerOperation *m_AuthorizationOperation;
	PPtr<GameObject> m_AuthorizationDialog;
};
#endif