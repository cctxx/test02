#pragma once

void LoadSerializedFileAndForget (const std::string& path, std::vector<Object*>& objects);
bool SaveToSerializedFileAndForget(const std::string& path, const std::vector<Object*>& objects, bool allowTextSerialization = false);

void SaveDefaultWindowPreferences ();
void LoadDefaultWindowPreferences ();
void ReloadWindowLayoutMenu ();
void RevertFactorySettings (bool quitOnCancel);
void LoadDefaultLayout ();

std::string GetUnityPreferencesFolder ();
std::string GetUnityLayoutsPreferencesFolder ();
