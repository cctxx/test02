#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ExternalProcess.h"
#include "Runtime/Utilities/File.h"
#include <iostream>
#include <string>

using namespace std;

ExternalProcess::ExternalProcess(const string& app, const vector<string>& arguments)
: m_LineBufferValid(false), m_ApplicationPath(app), m_Arguments(arguments), 
  m_ReadTimeout(120.0), m_WriteTimeout(30.0), m_ReadHandle(NULL), m_WriteHandle(NULL),
  m_PipeIn(NULL), m_PipeOut(NULL), m_Task(NULL)
{
	m_Buffer = [[NSMutableData alloc] init];
}

ExternalProcess::~ExternalProcess()
{	
	Shutdown();
	[m_Buffer release];
}

bool ExternalProcess::Launch()
{
	cout << "Launching external process: " << m_ApplicationPath << endl;
	[m_Buffer setLength:0]; // clear in case of relaunch

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	m_Task = [[NSTask alloc]init];
	[m_Task retain];
	NSString* nsstr = [[NSString alloc] initWithUTF8String: m_ApplicationPath.c_str()];
	[m_Task setLaunchPath: nsstr];
	[nsstr release];
	
	string currentDirectory = File::GetCurrentDirectory();
	
	if (!currentDirectory.empty ())
		[m_Task setCurrentDirectoryPath: [NSString stringWithUTF8String: currentDirectory.c_str ()]];
	
	m_PipeOut = [[NSPipe alloc] init];
	m_ReadHandle = [m_PipeOut fileHandleForReading];
	[m_ReadHandle retain];
	
	m_PipeIn = [[NSPipe alloc] init];
	m_WriteHandle = [m_PipeIn fileHandleForWriting];
	[m_WriteHandle retain];
	
	[m_Task setStandardInput: m_PipeIn];
	[m_Task setStandardOutput: m_PipeOut];
	
	// Set argument list
	NSMutableArray* nsargs = [[NSMutableArray alloc]initWithCapacity: m_Arguments.size ()];
	for (int i=0;i<m_Arguments.size ();i++)
	{
		NSString* str = [[NSString alloc]initWithUTF8String: m_Arguments[i].c_str ()];
		[nsargs addObject: str];
		[str release];
	}
	[m_Task setArguments: nsargs];
	[nsargs release];
		
	NS_DURING
	[m_Task launch];
	NS_HANDLER
	// Exception handling
	{
		Cleanup();
		[pool release];
	}
	NS_VALUERETURN (false, bool);
	NS_ENDHANDLER
	[pool release];
	return true;
}

bool ExternalProcess::IsRunning()
{
	return m_Task && [m_Task isRunning];
}

string ExternalProcess::ReadLine()
{
	if (m_LineBufferValid)
	{
		m_LineBufferValid = false;
		return m_LineBuffer;
	}
	
    int readyFDs;
    fd_set fdReadSet;
    struct timeval timeout;
	timeout.tv_sec = (time_t) m_ReadTimeout;
	timeout.tv_usec = (suseconds_t) ((m_ReadTimeout - timeout.tv_sec) * 1000000.0);

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	int fd = [m_ReadHandle fileDescriptor];
		
	int retries = 3;
	while (retries > 0)
	{
		if (!IsRunning())
		{
			[pool release];
			throw ExternalProcessException(EPSTATE_NotRunning);
		}

		char* bytes = (char*)[m_Buffer bytes];
		string str(bytes, [m_Buffer length]);
		string::size_type i = str.find('\n');
		if ( i == string::npos)
		{
			// No newline found - read more data

			FD_ZERO(&fdReadSet);
			FD_SET(fd, &fdReadSet);
			readyFDs = select( fd + 1, &fdReadSet, nil, nil, &timeout );
			
			if ( readyFDs == -1 ) 
			{
				if (errno == EAGAIN)
				{
					--retries;
					continue;
				}
				[pool release];
				throw ExternalProcessException(EPSTATE_BrokenPipe, "Error polling external process");
			}
			else if ( readyFDs == 0 )
			{
				[pool release];
				throw ExternalProcessException(EPSTATE_TimeoutReading, "Timeout reading from external process");
			} 
			else if ( ! FD_ISSET(fd, &fdReadSet) )
			{
				[pool release];
				throw ExternalProcessException(EPSTATE_BrokenPipe, "Got unknown read ready file description while reading from external process");
			}
			
			string err;
			NS_DURING
			NSData* nsoutput = [m_ReadHandle availableData];	
			[m_Buffer appendData:nsoutput];
			NS_HANDLER
			{
			err = Format("Caught exception while reading from external process: %s %s",
						 [[localException name] UTF8String],
						 [[localException reason] UTF8String]);
			}
			NS_ENDHANDLER
			if (!err.empty())
			{
				[pool release];
				throw ExternalProcessException(EPSTATE_BrokenPipe, err);
			}
			continue;
		}
		
		retries = 3;
		string result(str, 0, i);
		[m_Buffer setLength:0];
		i++;
		if (i >= str.length())
		{
			[pool release];
			return result;
		}

		string dataStr = str.substr(i);
		[m_Buffer appendBytes:dataStr.c_str() length:dataStr.length()];
		[pool release];
		return result;
	}
	[pool release];
	return "";
}
	
string ExternalProcess::PeekLine()
{
	m_LineBuffer = ReadLine();
	m_LineBufferValid = true;
	return m_LineBuffer;
}

bool ExternalProcess::Write(const string& data)
{
	int readyFDs;
	fd_set fdWriteSet;
	struct timeval timeout;
	timeout.tv_sec = (time_t) m_WriteTimeout;
	timeout.tv_usec = (suseconds_t) ((m_WriteTimeout - timeout.tv_sec) * 1000000.0);
	
	int fd = [m_WriteHandle fileDescriptor];


	int retries = 3;
	while (retries > 0)
	{
		if (!IsRunning())
		{
			throw ExternalProcessException(EPSTATE_NotRunning);
		}

		FD_ZERO(&fdWriteSet);
		FD_SET(fd, &fdWriteSet);
		readyFDs = select( fd + 1, nil, &fdWriteSet, nil, &timeout );
		
		if ( readyFDs == -1 ) 
		{
			if (errno == EAGAIN)
			{
				--retries;
				continue;
			}
			throw ExternalProcessException(EPSTATE_BrokenPipe, "Error polling external process");
		}
		else if ( readyFDs == 0 )
		{
			throw ExternalProcessException(EPSTATE_TimeoutWriting, "Timeout writing from external process");
		} 
		else if ( ! FD_ISSET(fd, &fdWriteSet) )
		{
			throw ExternalProcessException(EPSTATE_BrokenPipe, "Got unknown write ready file description while write from external process");
		}
		break;
	}
	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	NSData* strdata = [NSData dataWithBytes:data.c_str() length:data.length()];

	string err;
	NS_DURING
	[m_WriteHandle writeData:strdata];
	NS_HANDLER
	// Exception handling
	{
		err = Format("Caught exception while writing from external process: %s %s",
					 [[localException name] UTF8String],
					 [[localException reason] UTF8String]);
	}
	NS_ENDHANDLER
	
	[pool release];

	if (!err.empty())
		throw ExternalProcessException(EPSTATE_BrokenPipe, err);
	
	return true;
}

void ExternalProcess::Shutdown()
{
	// Wait until exit is needed in order for the task to properly deallocate
	if (IsRunning())
	{
		[m_Task terminate];
		
		// [m_Task waitUntilExit]; not good -> blocking
		int terminateTimeout = 5; // seconds
		while (IsRunning() && terminateTimeout--)
		{
			sleep(1);
		}
	}
	Cleanup();
}

void ExternalProcess::SetReadTimeout(double secs)
{
	m_ReadTimeout = secs;
}

double ExternalProcess::GetReadTimeout()
{
	return m_ReadTimeout;
}

void ExternalProcess::SetWriteTimeout(double secs)
{
	m_WriteTimeout = secs;
}

double ExternalProcess::GetWriteTimeout()
{
	return m_WriteTimeout;
}


void ExternalProcess::Cleanup()
{	
	if (m_Task) 
	{
		[m_Task release];
		m_Task = NULL;
	}
	if (m_ReadHandle)
	{
		[m_ReadHandle closeFile];
		[m_ReadHandle release];
		m_ReadHandle = NULL;
	}
	if (m_PipeOut)
	{
		[m_PipeOut release];
		m_PipeOut = NULL;
	}
	if (m_WriteHandle)
	{
		[m_WriteHandle closeFile];
		[m_WriteHandle release];
		m_WriteHandle = NULL;
	}
	if (m_PipeIn)
	{
		[m_PipeIn release];
		m_PipeIn = NULL;
	}
	if (m_Task)
	{
		[m_Task release];
		m_Task = NULL;
	}
}
