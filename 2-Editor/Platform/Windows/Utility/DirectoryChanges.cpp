#include "UnityPrefix.h"
#include "DirectoryChanges.h"
#include "Runtime/Threads/Mutex.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"

using namespace	dirchanges;

const int kMaxNotifyBuffer = 1024*1024;


struct WatchedDirectory
{
	HANDLE		dirHandle;
	std::wstring name;
	UInt8		buffer[kMaxNotifyBuffer];
	DWORD		bufferLength;
	OVERLAPPED	overlapped;
};



class dirchanges::DirectoryWatcher {
public:
	DirectoryWatcher()
	:	m_ThreadExitRequested(false)
	,	m_CompletionPort(0)
	,	m_Thread(0)
	,	m_NotifyFlags(0)
	{
	}

	~DirectoryWatcher()
	{
		if( m_Thread )
		{
			Stop();
		}
	}


	bool Start( const int count, const char** directories, DWORD notifyFlags );
	void Stop();

	bool GetChanges( ChangeRecords*	into );
	bool DiscardChanges();

private:
	static DWORD WINAPI ThreadProc( void* lpParameter );
	void AddChangeRecord( const std::wstring& path, ChangeActions action );
	void CheckChangedFile( const WatchedDirectory* dir, const FILE_NOTIFY_INFORMATION* notify );

	std::vector<WatchedDirectory*> m_Dirs;
	Mutex			m_PendingMutex;
	ChangeRecords	m_PendingRecords;
	DWORD			m_NotifyFlags;
	HANDLE			m_CompletionPort;
	HANDLE			m_Thread;
	bool m_ThreadExitRequested;
};



void DirectoryWatcher::AddChangeRecord(	const std::wstring& path, ChangeActions action )
{
	ChangeRecord rec;

	rec.action = action;
	ConvertWindowsPathName( path, rec.path );

	m_PendingRecords.push_back( rec );
}


// NOTE: this function is called from the watcher thread; cannot touch shared stuff!
void DirectoryWatcher::CheckChangedFile( const WatchedDirectory* dir, const FILE_NOTIFY_INFORMATION* notify )
{
	// file name length is in bytes!
	int nameLength = notify->FileNameLength / sizeof(wchar_t);
	AssertIf( (notify->FileNameLength & 1) != 0 );

	// copy the file name
	std::wstring wideName;
	wideName.resize( nameLength );
	wideName.assign( notify->FileName, nameLength );

	// construct full file name
	std::wstring fullWideName = dir->name;
	fullWideName += wideName;

	// add change record
	Mutex::AutoLock lock(m_PendingMutex);
	AddChangeRecord( fullWideName, static_cast<ChangeActions>(notify->Action) );
}


bool DirectoryWatcher::GetChanges( ChangeRecords* into )
{
	Mutex::AutoLock lock(m_PendingMutex);
	
	if( m_PendingRecords.empty() )
		return false;
	into->insert( into->end(), m_PendingRecords.begin(), m_PendingRecords.end() );
	m_PendingRecords.clear();

	return true;
}

bool DirectoryWatcher::DiscardChanges()
{
	Mutex::AutoLock lock(m_PendingMutex);
	
	if( m_PendingRecords.empty() )
		return false;
	m_PendingRecords.clear();

	return true;
}


DWORD WINAPI DirectoryWatcher::ThreadProc( void* threadParam )
{
	DirectoryWatcher* watcher = (DirectoryWatcher*)threadParam;

	// copy out completionPort
	HANDLE completionPort = 0;
	{
		Mutex::AutoLock lock( watcher->m_PendingMutex );
		completionPort = watcher->m_CompletionPort;
	}
	
	for(;;)
	{
		DWORD numBytes = 0;
		WatchedDirectory *di = NULL;
		OVERLAPPED* overlapped = NULL;
	
		// Fetch results for this directory through the completion port.
		// This will stall until something is available.
		BOOL ret = GetQueuedCompletionStatus( completionPort, &numBytes, (PULONG_PTR)&di, &overlapped, INFINITE );

		// Maybe we have to exit now?
		{
			Mutex::AutoLock lock( watcher->m_PendingMutex );
			if( watcher->m_ThreadExitRequested )
				break;
		}
		
		if( ret == 0 )
		{
			continue;
		}

		// If we have directory and actually got some bytes, process them
		if( di && numBytes > 0 )
		{
			FILE_NOTIFY_INFORMATION* notify = (FILE_NOTIFY_INFORMATION*)di->buffer;

			DWORD byteOffset;
			do
			{
				watcher->CheckChangedFile( di, notify );
				byteOffset = notify->NextEntryOffset;
				notify = (FILE_NOTIFY_INFORMATION*)( (BYTE*)notify + byteOffset );
			} while( byteOffset );

			// reissue directory watch
			if( !ReadDirectoryChangesW( di->dirHandle,
				di->buffer,
				kMaxNotifyBuffer,
				TRUE,
				watcher->m_NotifyFlags,
				&di->bufferLength,
				&di->overlapped,
				NULL) )
			{
				// don't log errors, we're on a thread
			}
		}
	}

	return 0;
}


void DirectoryWatcher::Stop()
{
	// tell the thread to exit
	{
		Mutex::AutoLock	lock( m_PendingMutex );
		m_ThreadExitRequested = true;
	}

	PostQueuedCompletionStatus(	m_CompletionPort, 0, 0, NULL );

	// wait for thread to exit
	if( WaitForSingleObject( m_Thread, 5000 ) != WAIT_OBJECT_0 )
	{
		printf_console( "DirectoryWatcher: watcher thread did not exit nicely!\n" );
	}

	CloseHandle( m_Thread );
	m_Thread = 0;
	
	CloseHandle( m_CompletionPort );
	m_CompletionPort = 0;
	
	for( int i = 0; i < m_Dirs.size(); ++i )
	{
		CloseHandle( m_Dirs[i]->dirHandle );
		delete m_Dirs[i];
	}
	m_Dirs.clear();
}


bool DirectoryWatcher::Start( const int count, const char** directories, DWORD notifyFlags )
{
	m_PendingRecords.reserve( 256 );
	m_NotifyFlags = notifyFlags;

	// Create IO completion	ports for each directory
	m_Dirs.reserve(count);
	for( int i = 0; i < count; ++i )
	{
		m_Dirs.push_back( new WatchedDirectory() );
		WatchedDirectory& dir = *m_Dirs.back();

		// Open directory
		ConvertUnityPathName( directories[i], dir.name );
		dir.dirHandle = CreateFileW( dir.name.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL );

		if(	dir.dirHandle == INVALID_HANDLE_VALUE )
		{
			printf_console( "DirectoryWatcher: can't open directory: %s\n", directories[i] );
			m_Dirs.pop_back();
			continue;
		}

		// add ending path separator if does not exist yet
		if( !dir.name.empty() )
		{
			wchar_t last = dir.name.back();
			if( last != '\\' && last != '/' )
			{
				dir.name.push_back(L'\\');
			}
		}

		// create completion port
		m_CompletionPort = CreateIoCompletionPort( dir.dirHandle, m_CompletionPort, (ULONG_PTR)&dir, 0 );
		if( m_CompletionPort == INVALID_HANDLE_VALUE )
		{
			ErrorString( "DirectoryWatcher: can't create IO completion port" );
			return false;
		}
	}

	if( m_Dirs.empty() )
	{
		ErrorString( "DirectoryWatcher: no valid directories" );
		if( m_CompletionPort )
			CloseHandle( m_CompletionPort );
		m_CompletionPort = 0;
		return false;
	}

	// start watching directories
	for( int i = 0; i < m_Dirs.size(); ++i )
	{
		if( !ReadDirectoryChangesW(
			m_Dirs[i]->dirHandle,
			m_Dirs[i]->buffer,
			kMaxNotifyBuffer,
			TRUE,
			m_NotifyFlags,
			&m_Dirs[i]->bufferLength,
			&m_Dirs[i]->overlapped,
			NULL) )
		{
			printf_console( "DirectoryWatcher: ReadDirectoryChangesW failed\n" );
		}
	}

	// spawn a thread to manage the changes
	DWORD threadID;
	m_Thread = CreateThread( NULL, 0, ThreadProc, (LPVOID)this, 0, &threadID );
	if( m_Thread == INVALID_HANDLE_VALUE )
	{
		ErrorString( "DirectoryWatcher: failed to create thread" );
		return false;
	}
	
	return true;
}

DirectoryWatcher* dirchanges::StartWatchingDirectories(	const int count, const char** dirs, UInt32 notifyFlags )
{
	DirectoryWatcher* watcher = new DirectoryWatcher();
	if( !watcher->Start(count, dirs, notifyFlags) )
	{
		delete watcher;
		return NULL;
	}
	
	return watcher;
}

bool dirchanges::GetDirectoryChanges( DirectoryWatcher*	obj, ChangeRecords* into )
{
	if(	!obj )
		return false;
	return obj->GetChanges( into );
}

bool dirchanges::DiscardDirectoryChanges( DirectoryWatcher*	obj )
{
	if( !obj )
		return false;
	return obj->DiscardChanges();
}

void dirchanges::StopWatchingDirectories( DirectoryWatcher*	obj )
{
	if(	!obj )
		return;
	obj->Stop();
	delete obj;
}
