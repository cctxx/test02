#include "UnityPrefix.h"
#include "File.h"
#include "PathNameUtility.h"
#include "PlatformDependent/Win/unistd.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if UNITY_EDITOR
#include "FileUtilities.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#endif
#include "Runtime/Scripting/ScriptingUtility.h"
#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_FLASH || UNITY_WEBGL || UNITY_TIZEN
#include <sys/types.h>
#include <sys/stat.h>
#include "dirent.h"
#endif
#if UNITY_BB10
#include <unistd.h>
#include <dirent.h>
#endif
#include "Runtime/Threads/Thread.h"

// This is File implementation for OSX and possibly others. Windows implementation is in PlatformDependent
#if UNITY_WIN
#error "Windows implementation of File is not here!"
#endif

using namespace std;

#if UNITY_EDITOR
string GetFormattedFileError (int error, const std::string& operation);
#endif

static string gCurrentDirectory;

static FILE* OpenFileWithPath( const string& path, File::Permission permission )
{
#if SUPPORT_DIRECT_FILE_ACCESS
	const char* fileMode = NULL;
	switch (permission) {
		case File::kReadPermission:
			fileMode="rb";
			break;
		case File::kWritePermission:
			fileMode="wb";
			break;
		case File::kReadWritePermission:
			fileMode="r+b";
			break;
		case File::kAppendPermission:
			fileMode="ab";
			break;
	}

	return fopen( PathToAbsolutePath(path).c_str(), fileMode );

#else //SUPPORT_DIRECT_FILE_ACCESS

	ErrorString("No file access allowed!");
	return NULL;
#endif
}

bool IsAbsoluteFilePath( const std::string& path )
{
	if( path.empty() )
		return false;

	if( path[0] == kPlatformPathNameSeparator )
		return true; // paths starting with separator are absolute

	return false;
}


string PathToAbsolutePath (const string& path)
{
	if( IsAbsoluteFilePath(path) )
		return path;
	else
		return AppendPathName (File::GetCurrentDirectory (), path);
}

bool ReadFromFile (const string& pathName, void *data, int fileStart, int byteLength)
{
	FILE* file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);

	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < byteLength)
	{
		fclose(file);
		return false;
	}

	int readLength = fread(data, 1, byteLength, file);

	fclose(file);

	if (readLength != byteLength)
		return false;

	return true;
}


#if !UNITY_FLASH  //flash implementation in AS3Utility.cpp
bool ReadStringFromFile (InputString* outData, const string& pathName)
{
	FILE* file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);

	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0)
	{
		fclose( file );
		return false;
	}

	outData->resize(length);
	int readLength = fread(&*outData->begin(), 1, length, file);

	fclose(file);

	if (readLength != length)
	{
		outData->clear();
		return false;
	}

	return true;
}
#endif


bool WriteBytesToFile (const void *data, int byteLength, const string& pathName)
{
	File file;
	if (!file.Open(pathName, File::kWritePermission))
		return false;

	bool success = file.Write(data, byteLength);

	file.Close();

	return success;
}


int GetFileLength (const string& pathName)
{
	#if UNITY_OSX || UNITY_BB10
	struct stat statbuffer;
	if( stat(pathName.c_str(), &statbuffer) != 0 )
		return 0; /// return -1 for error???
	Assert (S_ISREG(statbuffer.st_mode));
	return statbuffer.st_size;
	#elif UNITY_FLASH
	return Ext_FileContainer_GetFileLength(pathName.c_str());
	#else
	FILE* file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file == NULL)
		return 0; /// return -1???

	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fclose(file);

	return length;
	#endif
}

File::File () { m_File = NULL;m_Position = 0; }

File::~File () { AssertIf(m_File != NULL); }

#if UNITY_EDITOR
string GetFormattedFileError (int error, const std::string& operation)
{
	const char* opstr = operation.c_str();
	switch (error)
	{
		case ENAMETOOLONG:
			return Format("%s failed because file name is too long", opstr);
		case ENOTDIR:
			return Format("%s failed: from directory to non-directory", opstr);
		case EISDIR:
			return Format("%s failed: from non-directory to directory", opstr);
		case EXDEV:
			return Format("%s failed: to and from are on different file systems", opstr);
		case EIO:
			return Format("%s failed: I/O error updating directory", opstr);
		case EROFS:
			return Format("%s failed: read only file system", opstr);
		case EFAULT:
			return Format("%s failed: segmentation fault", opstr);
		case EINVAL:
			return Format("%s failed: from is a parent of to, or rename of . or ..", opstr);
		case ENOTEMPTY:
			return Format("%s failed: to is a directory and not empty", opstr);
		case EPERM:
			return Format("%s failed because the operation was not permitted", opstr);
		case ENOENT:
			return Format("%s failed because the file or directory does not exist", opstr);
		case ENOMEM:
			return Format("%s failed because there was not enough memory available", opstr);
		case EACCES:
			return Format("%s failed because permission for the file was denied", opstr);
		case EEXIST:
			return Format("%s failed because the file already exists", opstr);
		case ENOSPC:
			return Format("%s failed because there is no disk space left. Please free some disk space and continue.", opstr);
		#if UNITY_OSX
		case EAUTH:
			return Format("%s failed because of an authentication failure", opstr);
		case ENEEDAUTH:
			return Format("%s failed because you need an authenticator", opstr);
		case ELOOP:
			return Format("%s failed: too many symbolic links", opstr);
		case EDQUOT:
			return Format("%s failed: quota limit reached", opstr);
		#endif
		default:
			return Format("%s failed with error: %s", opstr, strerror(error));
	}
}

#endif
static bool HandleFileError (const char* title, const string& operation, FILE* file)
{
	#if UNITY_EDITOR
	int err = ferror(file);
	clearerr(file);

	int result = DisplayDialogComplex (title, GetFormattedFileError(err, operation), "Try Again", "Cancel", "Force Quit");
	if (result == 1)
		return false;
	else if (result == 2)
		exit(1);
	else
		return true;
	#else
	return false;
	#endif
}

bool File::Open (const std::string& path, File::Permission perm, AutoBehavior behavior)
{
	Close();
	m_Path = path;

	int retryCount = 5;

	while (true)
	{
		m_File = OpenFileWithPath( path, perm );
		m_Position = 0;
		if (m_File != NULL)
		{
			if (perm == kAppendPermission)
				m_Position = ftell(m_File);
			return true;
		}
		else
		{
#if SUPPORT_THREADS
			if ( (behavior & kRetryOnOpenFail) && (--retryCount > 0))
			{
				Thread::Sleep(0.2);
				continue;
			}
#endif

			if ( behavior & kSilentReturnOnOpenFail )
				return false;

			#if UNITY_EDITOR
			int result = DisplayDialogComplex ("Opening file failed", GetFormattedFileError(errno, "Opening file "+path), "Try Again", "Cancel", "Force Quit");
			if (result == 1)
				return false;
			else if (result == 2)
				exit(1);
			#else
			ErrorString("Failed to open file at path: " + path);
			return false;
			#endif
		}
	}
	return false;
}

bool File::Close ()
{
	if (m_File != NULL)
	{
		if (fclose(m_File) != 0)
		{
			#if UNITY_EDITOR
			ErrorString(GetFormattedFileError(errno, "Closing file " + m_Path));
			#elif UNITY_FLASH
			//no problemo
			#else
			ErrorString("Closing file " + m_Path);
			#endif
		}
		m_File = NULL;
	}

	m_Path.clear();
	return true;
}

int File::Read (void* buffer, int size)
{
	if (m_File)
	{
		int s = fread(buffer, 1, size, m_File);

		if (s == size || ferror(m_File) == 0)
		{
			m_Position += s;
			return s;
		}
		else
		{
			//m_Position = -1;
			if (!HandleFileError("Reading file failed", "Reading from file " + m_Path, m_File))
				return false;

			// We don't know how far the file was read.
			// So we just play safe and go through the API that seeks from a specific offset
			int oldPos = m_Position;
			m_Position = -1;
			return Read(oldPos, buffer, size);
		}
	}
	else
	{
		ErrorString("Reading failed because the file was not opened");
		return 0;
	}
}

int File::Read (int position, void* buffer, int size)
{
	if (m_File)
	{
		while (true)
		{
			// Seek if necessary
			if (position != m_Position)
			{
				if (fseek(m_File, position, SEEK_SET) != -1)
					m_Position = position;
				else
				{
					m_Position = -1;
					if (!HandleFileError("Reading file failed", "Seeking in file " + m_Path, m_File ))
						return 0;

					continue;
				}
			}

			int s = fread(buffer, 1, size, m_File);
			if (s == size || ferror(m_File) == 0)
			{
				m_Position += s;
				return s;
			}
			else
			{
				m_Position = -1;
				if (!HandleFileError("Reading file failed", "Reading from file " + m_Path, m_File ))
					return 0;
			}
		}
	}
	else
	{
		ErrorString("Reading failed because the file was not opened");
		return 0;
	}
	return 0;
}



bool File::Write (const void* buffer, int size)
{
	if (m_File)
	{
		int s = fwrite(buffer, 1, size, m_File);
		if (s == size)
		{
			m_Position += s;
			return true;
		}
		else
		{
			if (!HandleFileError("Writing file failed", "Writing to file " + m_Path, m_File))
				return false;

			// We don't know how far the file was read.
			// So we just play safe and go through the API that seeks from a specific offset
			int oldPos = m_Position;
			m_Position = -1;
			return Write(oldPos, buffer, size);
		}
	}
	else
	{
		ErrorString("Writing failed because the file was not opened");
		return false;
	}
}

bool File::Write (int position, const void* buffer, int size)
{
	if (m_File)
	{
		while (true)
		{
			// Seek if necessary
			if (position != m_Position)
			{
				if (fseek(m_File, position, SEEK_SET) != -1)
					m_Position = position;
				else
				{
					m_Position = -1;
					if (!HandleFileError("Writing file failed", "Seeking in file "+m_Path, m_File))
						return false;

					continue;
				}
			}

			int s = fwrite(buffer, 1, size, m_File);
			if (s == size)
			{
				m_Position += s;
				return true;
			}
			else
			{
				m_Position = -1;
				if (!HandleFileError("Writing file failed", "Writing to file "+m_Path, m_File))
					return false;
			}
		}
	}
	else
	{
		ErrorString("Writing failed because the file was not opened");
	}
	return false;
}

bool File::SetFileLength (int size)
{
	return ::SetFileLength(m_Path, size);
}

int File::GetFileLength()
{
	return ::GetFileLength(m_Path);
}

void File::SetCurrentDirectory (const string& path)
{
	gCurrentDirectory = path;
}

const string& File::GetCurrentDirectory ()
{
	return gCurrentDirectory;
}

void File::CleanupClass()
{
	gCurrentDirectory = string();
}

bool SetFileLength (const std::string& path, int size)
{
	#if UNITY_PEPPER || UNITY_FLASH || UNITY_WEBGL
	ErrorString("SetFileLength not supported on this platform!");
	return false;
	#else
	while (true)
	{
		int error = truncate(path.c_str(), size);
		if (error == 0)
			return true;

		#if UNITY_EDITOR
		int result = DisplayDialogComplex ("Writing file error", GetFormattedFileError(errno, "Resizing file " + path), "Try Again", "Cancel", "Quit");
		if (result == 1)
			return false;
		else if (result == 2)
			exit(1);
		#else
		return false;
		#endif
	}
	#endif

	return true;
}

static bool IsFileCreatedAtAbsolutePath (const string & path)
{
	#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_FLASH || UNITY_WEBGL || UNITY_BB10 || UNITY_TIZEN
	
	struct stat status;

	if (stat(path.c_str(), &status) != 0)
		return false;

#if TARGET_IPHONE_SIMULATOR
	// some bad hack for simulator, if we can stat it then file exists...
	return true;
#endif

	return S_ISREG (status.st_mode);

	#elif !SUPPORT_DIRECT_FILE_ACCESS

	ErrorString("No file access allowed!");
	return false;

	#else

	#error "Unsupported platform!"

	#endif
}

#if !UNITY_FLASH //Flash implementation in AS3Utility.cpp
bool IsFileCreated (const string& path)
{
	return IsFileCreatedAtAbsolutePath (PathToAbsolutePath (path));
}
#endif


#if UNITY_OSX || UNITY_IPHONE || UNITY_PEPPER || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
bool IsFileOrDirectoryInUse ( const string& path )
{
#if UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
	return false;
#elif !UNITY_PEPPER
	bool isUsed = false;

	if ( IsDirectoryCreated( path ) )
	{
		set<string> paths;
		if ( GetFolderContentsAtPath( path, paths ) )
		{
			for (set<string>::iterator i = paths.begin(); i != paths.end(); i++)
			{
				if ( IsFileOrDirectoryInUse( *i ) )
					return true;
			}
		}
	}
	else if ( IsFileCreated( path ) )
	{
		File openTest;
		if ( openTest.Open( path, File::kReadPermission ) )
		{
			if ( !openTest.Lock( File::kExclusive, false ) )
				isUsed = true;
			openTest.Lock( File::kNone, false );
			openTest.Close();
		}
		else
			isUsed = true;
	}

	return isUsed;
#else
	ErrorString("No file access allowed!");
	return false;
#endif
}
#endif

static bool IsDirectoryCreatedAtAbsolutePath (const string & path)
{
	#if UNITY_OSX || UNITY_LINUX || UNITY_IPHONE || UNITY_BB10 || UNITY_WEBGL || UNITY_TIZEN
	struct stat status;
	if (stat(path.c_str(), &status) != 0)
		return false;
	return S_ISDIR(status.st_mode);

	#elif UNITY_FLASH
	#warning "IsDirectoryCreatedAtAbsolutePath() NOT implemented for UNITY_FLASH"
	return false;

	#elif !SUPPORT_DIRECT_FILE_ACCESS

	ErrorString("No file access allowed!");
	return false;
	#else
	#error Unknown platform
	#endif
}

bool IsDirectoryCreated (const string& path)
{
	return IsDirectoryCreatedAtAbsolutePath (PathToAbsolutePath (path));
}

bool CreateDirectory (const string& pathName)
{
#if SUPPORT_DIRECT_FILE_ACCESS && !UNITY_FLASH
	if (IsFileCreated (pathName))
		return false;
	else if (IsDirectoryCreated (pathName))
		return true;

	string absolutePath = PathToAbsolutePath (pathName);

#if UNITY_EDITOR
	std::string fileNamePart = GetLastPathNameComponent (absolutePath);
	if (CheckValidFileNameDetail (fileNamePart) == kFileNameInvalid)
	{
		ErrorStringMsg ("%s is not a valid directory name. Please make sure there are no unallowed characters in the name.", fileNamePart.c_str ());
		return false;
	}
#endif

	int res = mkdir(absolutePath.c_str(), 0755);
	return res == 0;
#else
	ErrorString("No file access allowed!");
	return false;
#endif
}

bool DeleteFile (const string& path)
{
#if !UNITY_PEPPER && !UNITY_FLASH && !UNITY_WEBGL
	string absolutePath = PathToAbsolutePath (path);
	if (IsFileCreated (absolutePath))
	{
		return unlink(absolutePath.c_str()) == 0;
	}
	else
	{
		return false;
	}
#else
	ErrorString("No file access allowed!");
	return false;
#endif
}

int DeleteFilesAndDirsRecursive(const string& path)
{
#if SUPPORT_DIRECT_FILE_ACCESS && !UNITY_FLASH && !UNITY_WEBGL
	int res;

	struct stat status;
	res = lstat(path.c_str(), &status);
	if (res != 0)
		return res;

	if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode))
	{
		DIR *dirp = opendir (path.c_str());
		if (dirp == NULL)
			return -1;

		struct dirent *dp;
		while ( (dp = readdir(dirp)) )
		{
			if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
			{
				string name = dp->d_name;
				res = DeleteFilesAndDirsRecursive(AppendPathName (path, name));
				if (res != 0)
				{
					closedir(dirp);
					return res;
				}
			}
		}
		closedir(dirp);

		res = rmdir(path.c_str());
	}
	else
	{
		res = unlink(path.c_str());
	}

	return res;
#else
	ErrorString("No file access allowed!");
	return false;
#endif
}

bool DeleteFileOrDirectory (const string& path)
{
	int res = DeleteFilesAndDirsRecursive(PathToAbsolutePath(path));
	return res == 0;
}

bool ShouldIgnoreFile (const char* name, size_t length)
{
	AssertIf(!name);
	if (length < 1)
		return true;

	// ignore hidden files
	if (name[0] == '.')
		return true;
	// ignore CVS files
	if (StrICmp(name, "cvs") == 0)
		return true;

	// ignore temporary and backup files
	if ((name[length-1] == '~') || (length >= 4 && StrICmp(name + length - 4, ".tmp" ) == 0))
		return true;

	return false;
}

bool GetFolderContentsAtPath (const string& pathName, set<string>& paths)
{
#if SUPPORT_DIRECT_FILE_ACCESS && !UNITY_FLASH
	string absolutePath = PathToAbsolutePath(pathName);
	DIR *dirp;
    struct dirent *dp;

    if ((dirp = opendir(absolutePath.c_str())) == NULL)
        return false;

	while ( (dp = readdir(dirp)) )
	{
		if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
		{
			if (!ShouldIgnoreFile (dp->d_name, strlen (dp->d_name)))
			{
				string name = dp->d_name;
				paths.insert (AppendPathName (pathName, name));
			}
		}
	}
	closedir(dirp);
	return true;
#else
	ErrorString("No file access allowed!");
	return false;
#endif
}

#if UNITY_EDITOR

bool MoveReplaceFile (const string& fromPath, const string& toPath)
{
	if( IsDirectoryCreated(fromPath) )
	{
		ErrorString( Format("Path %s is a directory", fromPath.c_str()) );
		return false;
	}
	while (true)
	{
		int error = rename( PathToAbsolutePath(fromPath).c_str(), PathToAbsolutePath(toPath).c_str() );
		if( error == 0 )
			return true;

		int errorCode = errno;
		if( errorCode == EXDEV )
		{
			DeleteFileOrDirectory(toPath);
			if (CopyFileOrDirectory(fromPath, toPath))
			{
				if (DeleteFileOrDirectory(fromPath))
					return true;
			}
		}

		#if UNITY_EDITOR
		int result = DisplayDialogComplex ("Moving file failed", GetFormattedFileError(errno, "Moving "+fromPath+" to "+toPath), "Try Again", "Cancel", "Force Quit");
		if (result == 1)
			return false;
		else if (result == 2)
			exit(1);
		#else
		return false;
		#endif
	}
	return false;
}

#endif

#if UNITY_OSX || UNITY_IPHONE
bool File::Lock(File::LockMode mode, bool block)
{
	int fd = fileno (m_File);
	return flock (fd, mode + (block?0:LOCK_NB) ) == 0;
}

#include <sys/resource.h>
//Local testing and various internet sources suggest that this is the absolute maximim of open files OSX can hande.
#define kMaxOpenFile 10240

void SetupFileLimits ()
{
	struct rlimit limit;
    limit.rlim_cur = kMaxOpenFile;
    limit.rlim_max = kMaxOpenFile;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
		printf_console("setrlimit() failed with errno=%d\n", errno);
}
#endif
