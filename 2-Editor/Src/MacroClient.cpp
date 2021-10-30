#include "UnityPrefix.h"
#include "MacroClient.h"

#include "Runtime/Network/SocketStreams.h"
#include "Runtime/Utilities/Argv.h"
#include "MacroClientCommands.h"

using namespace std;

MacroClient::MacroClient (int portNum)
:	m_Socket(NULL)
{
	printf_console("about to connect to macroserver at port %d",portNum);
	int socketHandle = Socket::Connect("127.0.0.1", portNum);
	if ( socketHandle < 0 )
	{
		CheckBatchModeErrorString ("MacroClient connection failed because the socket couldn't connect");
		return;
	}
	m_Socket = new SocketStream(socketHandle, false);
	printf_console("** Successfully Connected to macro server **\n");
	return;
}

MacroClient::~MacroClient ()
{
	delete m_Socket;
}

void MacroClient::ReadFromBuffer ()
{
	const int maxLength = 4*4096;
	char buffer[maxLength];
	int numbytes;
	
	if ( ( numbytes = m_Socket->Recv( buffer, maxLength ) ) < 0 )
	{
		if (m_Socket->WouldBlockError())
			return;
		CheckBatchModeErrorString ("MacroClient connection failure ");
		return;
	}
	
	m_MacroBuffer.append(buffer, buffer + numbytes);
}


void MacroClient::WriteReturnValueToSocket(std::string& response)
{
	std::string completeresponse("RESPONSE: ");
	completeresponse += response;
	if (!m_Socket->SendAll(completeresponse.c_str(), completeresponse.size()+1))
		ErrorString("unable to write response of evaluation to socket");
}

void MacroClient::Poll ()
{
	ReadFromBuffer ();
	
	// Parse current buffer and send one event
	const char* kFlush = "FLUSH\n";
	
	// Extract all code until the next flush command
	string::size_type position = m_MacroBuffer.find(kFlush);
	if (position != string::npos)
	{
		string macro (m_MacroBuffer.begin(), m_MacroBuffer.begin() + position);
		
		m_MacroBuffer.erase(m_MacroBuffer.begin(), m_MacroBuffer.begin() + position + strlen(kFlush));
		
		string returnvalue = ExecuteMacroClientCommand(macro);
		WriteReturnValueToSocket(returnvalue);
	}
}
