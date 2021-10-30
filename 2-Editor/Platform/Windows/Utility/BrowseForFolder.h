#pragma once

typedef bool ValidateFolderFunc (const wchar_t* folder, std::wstring& outInfoMessage);

std::string BrowseForFolderDialog( HWND parent, const wchar_t* title, const wchar_t* initialFolder, const wchar_t* defaultFolder, bool existingFolder, ValidateFolderFunc validateFunc );
