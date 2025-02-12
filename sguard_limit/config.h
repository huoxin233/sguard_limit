#pragma once
#include <Windows.h>


// config load & write module
class ConfigManager {

private:
	static ConfigManager     configManager;

private:
	ConfigManager();
	~ConfigManager()                                   = default;
	ConfigManager(const ConfigManager&)                = delete;
	ConfigManager(ConfigManager&&)                     = delete;
	ConfigManager& operator= (const ConfigManager&)    = delete;
	ConfigManager& operator= (ConfigManager&&)         = delete;

public:
	static ConfigManager&    getInstance();

public:
	void    init(const CHAR* profilepath);
	bool    loadConfig();
	void    writeConfig();

private:
	const CHAR*   profile;
};