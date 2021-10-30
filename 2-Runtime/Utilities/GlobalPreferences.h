#ifndef GLOBALPREFERENCES_H
#define GLOBALPREFERENCES_H

#if WEBPLUG
std::string GetStrippedPlayerDomain ();
#endif

// Used for hardware stats and webplayer settings.
std::string GetGlobalPreference(const char *key);
bool GetGlobalBoolPreference(const char *key, bool defaultValue);

void SetGlobalPreference(const char *key, std::string value);
void SetGlobalBoolPreference(const char *key, bool value);

#endif