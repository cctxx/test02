#pragma once

std::string GetPersistentDataPathApplicationSpecific();
std::string GetTemporaryCachePathApplicationSpecific();
std::string GetAppDataPath();
std::string GetStreamingAssetsPath();

void ConvertToLegalPathNameCorrectly(std::string& path);
void ConvertToLegalPathNameBroken(std::string& path);