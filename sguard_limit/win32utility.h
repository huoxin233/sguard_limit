#pragma once
#include <Windows.h>
#include <vector>
#include <string>


// system version (used in patch module)
enum class OSVersion { 
	WIN_7       = 1, 
	WIN_10_11,
	OTHERS
};


// thread module wrapper (raii, see: std::shared_ptr)
struct win32Thread {

	// general properties
	DWORD       tid;
	HANDLE      handle;

	// time properties
	ULONG64     cycles;
	ULONG64     cycleDelta;
	ULONG64     cycleDeltaAvg;

	// ctors
	win32Thread(DWORD tid, DWORD desiredAccess = THREAD_ALL_ACCESS);
	~win32Thread();
	win32Thread(const win32Thread& t);
	win32Thread(win32Thread&& t) noexcept;
	win32Thread& operator= (win32Thread t) noexcept;
	// win32Thread& operator= (win32Thread&&);  >> no need; overload resolution is before '=delete'
	
private:
	DWORD*      _refCount;

private:
	static void _mySwap(win32Thread& t1, win32Thread& t2);  // friend ADL? no way, i don't need inline
};


// general thread toolkit
class win32ThreadManager {

public:
	win32ThreadManager();
	~win32ThreadManager()                                      = default;
	win32ThreadManager(const win32ThreadManager&)              = delete;
	win32ThreadManager(win32ThreadManager&&)                   = delete;
	win32ThreadManager& operator= (const win32ThreadManager&)  = delete;
	win32ThreadManager& operator= (win32ThreadManager&&)       = delete;

	DWORD  getTargetPid();
	bool   enumTargetThread(DWORD desiredAccess = THREAD_ALL_ACCESS);

public:
	DWORD                        pid;
	DWORD                        threadCount;
	std::vector<win32Thread>     threadList;
};


// general system toolkit (sington)
class win32SystemManager {

private:
	static win32SystemManager    systemManager;

private:
	win32SystemManager();
	~win32SystemManager();
	win32SystemManager(const win32SystemManager&)               = delete;
	win32SystemManager(win32SystemManager&&)                    = delete;
	win32SystemManager& operator= (const win32SystemManager&)   = delete;
	win32SystemManager& operator= (win32SystemManager&&)        = delete;

public:
	static win32SystemManager&   getInstance();


public:
	HWND                         hWnd;
	HINSTANCE                    hInstance;

public:
	bool          init(HINSTANCE hInst, DWORD iconRcNum, UINT trayActiveMsg);
	void          setupProcessDpi();
	void          enableDebugPrivilege();
	bool          checkDebugPrivilege();
	bool          createWin32Window(WNDPROC WndProc);
	void          createTray();
	void          removeTray();
	WPARAM        messageLoop();
				  
public:			  
	void          log(const char* format, ...);
	void          panic(const char* format, ...);
	void          panic(DWORD errorCode, const char* format, ...);
				  
public:			  
	const CHAR*   profilePath();       // config manager
	const CHAR*   sysfilePath();       // mempatch module
	const CHAR*   profileDirPath();    // mempatch module
	OSVersion     getSystemVersion();  // mempatch module
	DWORD         getSystemBuildNum(); // mempatch module
	

private:		  
	ATOM          _registerMyClass(WNDPROC WndProc);
	void          _panic(DWORD code, char* showbuf);

private:
	HANDLE                       hProgram;

	OSVersion                    osVersion;
	DWORD                        osBuildNum;

	FILE*                        logfp;

	DWORD                        iconRcNum;
	NOTIFYICONDATA               icon;
	UINT                         trayActiveMsg;

	std::string                  profileDir;
	std::string                  profile;
	std::string                  sysfile;
	std::string                  logfile;
};