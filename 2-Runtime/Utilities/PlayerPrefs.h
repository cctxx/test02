#ifndef PLAYERPREFS_H
#define PLAYERPREFS_H

#include <string>
#include <vector>

#if UNITY_EDITOR
class EditorPrefs {
public:
	
	static void UseCleanTestPrefs ();
	static bool SetInt (const std::string& name, int value);
	static bool SetBool (const std::string& name, bool value);
	static bool SetString (const std::string& name, const std::string& value);
	static bool SetFloat (const std::string& name, float value);

	static int GetInt (const std::string& name, int def = 0);
	static bool GetBool (const std::string& name, bool def = false);
	static std::string GetString (const std::string& name, const std::string& def = std::string ());
	static float GetFloat (const std::string& name, float def = 0.0F);

	static bool HasKey (const std::string& name);
	static void DeleteKey (const std::string& name);
	static void DeleteAll ();
	
	static void Sync ();
};
#endif

class PlayerPrefs {
public:
	typedef std::vector<UInt8> RawData;
public:
	static bool SetInt (const std::string& name, int value);
	static bool SetString (const std::string& name, const std::string& value);
	static bool SetFloat (const std::string& name, float value);

	static int GetInt (const std::string& name, int def = 0);
	static std::string GetString (const std::string& name, const std::string& def = std::string ());
	static float GetFloat (const std::string& name, float def = 0.0F);

	static bool HasKey (const std::string& name);
	static void DeleteKey (const std::string& name);
	static void DeleteAll ();

	#if UNITY_WII_API && (UNITY_WIN || UNITY_WII)
	static bool	GetRawData(RawData& rawData);
	static bool	SetRawData(const RawData& rawData);
	#endif
	
	static void Sync ();
	#if UNITY_METRO
	static void Init();
	#elif WEBPLUG
	static void Init (const std::string& playerURL);
	#endif
	#if UNITY_LINUX
	static std::string GetPath ();
	#endif

	static void StaticInitialize();
	static void StaticDestroy();
};

#endif
