#ifndef __MACRO_CLIENT_H__
#define __MACRO_CLIENT_H__

#include "Runtime/Network/SocketStreams.h"

class MacroClient
{
public:
	
	MacroClient (int portNum);
	~MacroClient();
	
	void Poll ();

private:

	void ReadFromBuffer ();
	void WriteReturnValueToSocket(std::string& response);
	SocketStream* m_Socket;
	std::string m_MacroBuffer;
};

#endif