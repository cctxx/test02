#import "SecurityFoundation/SFAuthorization.h"

bool CreateLicenseDirectory(const char* path)
{
	SFAuthorization * authorization = [SFAuthorization authorization];
	
	FILE *pipe = NULL;
	char *args[] = {const_cast<char*>(path), NULL};
	char *args2[] = {(char*)"777", const_cast<char*>(path), NULL};
		OSStatus status = AuthorizationExecuteWithPrivileges([authorization authorizationRef],
		 "/bin/mkdir",
		 kAuthorizationFlagDefaults,
		 args,
		 &pipe);
	if (status == errAuthorizationSuccess)
	{
		status = AuthorizationExecuteWithPrivileges([authorization authorizationRef],
													"/bin/chmod",
													kAuthorizationFlagDefaults,
													args2,
													&pipe);
	}
	
	return status == errAuthorizationSuccess;
}
