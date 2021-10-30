#pragma once
/// Opens a file at path with the default application at line
/// if the default application supports opening the file at a given line

enum OpenFileAtLineMode {
	kOpenFileInternalEditor,
	kOpenFileExternalEditor,
};

bool OpenFileAtLine (const std::string& path, int line, const std::string& appPath, const std::string& appArgs, OpenFileAtLineMode openMode);
