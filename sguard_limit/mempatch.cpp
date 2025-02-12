// Memory Patch（用户态模块）
// 2021.10.4 雨
// 昨天吃坏肚子了，很疼。但是 2.2 复刻胡桃，开心。
// 2021.11.1 万圣节
// 明天已经到来了。
#include <Windows.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <unordered_map>
#include <algorithm>   // std::sort
#include <memory>      // std::unique_ptr
#include "mempatch.h"

// dependencies
#include "win32utility.h"
#include "kdriver.h"

extern volatile DWORD         g_Mode;      // ref: patch::init()
extern win32SystemManager&    systemMgr;
KernelDriver&                 driver       = KernelDriver::getInstance();


// patch module
PatchManager  PatchManager::patchManager;

PatchManager::PatchManager()
	: patchEnabled(true), patchPid(0), patchSwitches{}, patchStatus{}, patchDelay{},
	  patchDelayRange{
	   { 200, 1500, 2500 },   /* NtQueryVirtualMemory */
	   { 100, 1000, 1500 },   /* GetAsyncKeyState */
	   { 1,   10,   200  },   /* NtWaitForSingleObject */
	   { 500, 1250, 2000 }    /* NtDelayExecution */
	  }, vmStartAddress(0), vmbuf_ptr(new CHAR[0x4000]), vmalloc_ptr(new CHAR[0x4000]) {}

PatchManager& PatchManager::getInstance() {
	return patchManager;
}

void PatchManager::init() {

	// import certificate key.
	HKEY       key;
	DWORD      dwDisposition;
	LONG       status;
	const BYTE keyData[] = "\x53\x00\x00\x00\x01\x00\x00\x00\x23\x00\x00\x00\x30\x21\x30\x1f\x06\x09\x60\x86\x48\x01"
		"\xa4\xa2\x27\x02\x01\x30\x12\x30\x10\x06\x0a\x2b\x06\x01\x04\x01\x82\x37\x3c\x01\x01\x03\x02\x00\xc0"
		"\x5c\x00\x00\x00\x01\x00\x00\x00\x04\x00\x00\x00\x00\x10\x00\x00\x03\x00\x00\x00\x01\x00\x00\x00\x14"
		"\x00\x00\x00\xe4\x03\xa1\xdf\xc8\xf3\x77\xe0\xf4\xaa\x43\xa8\x3e\xe9\xea\x07\x9a\x1f\x55\xf2\x19\x00"
		"\x00\x00\x01\x00\x00\x00\x10\x00\x00\x00\x79\xd8\xe3\x98\x56\xb0\x54\x09\x13\xde\xfb\x48\x5e\x73\xed"
		"\x62\x14\x00\x00\x00\x01\x00\x00\x00\x14\x00\x00\x00\x05\x25\x86\x2f\x65\x36\xa1\xe5\x9d\x9e\xca\x5c"
		"\x09\x19\xad\x0e\x3d\x96\x26\x1d\x0f\x00\x00\x00\x01\x00\x00\x00\x14\x00\x00\x00\x52\xbf\x46\x22\x03"
		"\x12\x1a\xb2\x71\xf4\x8f\xf1\xa3\x2d\x37\x3f\xd9\xf1\x23\x99\x04\x00\x00\x00\x01\x00\x00\x00\x10\x00"
		"\x00\x00\xdc\x91\x1e\x8d\xa3\xa1\x86\xbb\x4d\x52\xee\xc0\xe5\x7b\x51\x55\x09\x00\x00\x00\x01\x00\x00"
		"\x00\x4c\x00\x00\x00\x30\x4a\x06\x0a\x2b\x06\x01\x04\x01\x82\x37\x0a\x06\x02\x06\x0a\x2b\x06\x01\x04"
		"\x01\x82\x37\x0a\x06\x01\x06\x08\x2b\x06\x01\x05\x05\x07\x03\x08\x06\x08\x2b\x06\x01\x05\x05\x07\x03"
		"\x04\x06\x08\x2b\x06\x01\x05\x05\x07\x03\x03\x06\x08\x2b\x06\x01\x05\x05\x07\x03\x02\x06\x08\x2b\x06"
		"\x01\x05\x05\x07\x03\x01\x20\x00\x00\x00\x01\x00\x00\x00\xd3\x05\x00\x00\x30\x82\x05\xcf\x30\x82\x03"
		"\xb7\xa0\x03\x02\x01\x02\x02\x04\x1e\xb1\x32\xd5\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05"
		"\x05\x00\x30\x76\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x43\x4e\x31\x23\x30\x21\x06\x03\x55\x04"
		"\x0a\x0c\x1a\x4a\x65\x6d\x6d\x79\x4c\x6f\x76\x65\x4a\x65\x6e\x6e\x79\x20\x50\x4b\x49\x20\x53\x65\x72"
		"\x76\x69\x63\x65\x31\x1e\x30\x1c\x06\x03\x55\x04\x0b\x0c\x15\x70\x6b\x69\x2e\x6a\x65\x6d\x6d\x79\x6c"
		"\x6f\x76\x65\x6a\x65\x6e\x6e\x79\x2e\x74\x6b\x31\x22\x30\x20\x06\x03\x55\x04\x03\x0c\x19\x4a\x65\x6d"
		"\x6d\x79\x4c\x6f\x76\x65\x4a\x65\x6e\x6e\x79\x20\x45\x56\x20\x52\x6f\x6f\x74\x20\x43\x41\x30\x20\x17"
		"\x0d\x30\x30\x30\x31\x30\x31\x30\x30\x30\x30\x30\x30\x5a\x18\x0f\x32\x30\x39\x39\x31\x32\x33\x31\x32"
		"\x33\x35\x39\x35\x39\x5a\x30\x76\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x43\x4e\x31\x23\x30\x21"
		"\x06\x03\x55\x04\x0a\x0c\x1a\x4a\x65\x6d\x6d\x79\x4c\x6f\x76\x65\x4a\x65\x6e\x6e\x79\x20\x50\x4b\x49"
		"\x20\x53\x65\x72\x76\x69\x63\x65\x31\x1e\x30\x1c\x06\x03\x55\x04\x0b\x0c\x15\x70\x6b\x69\x2e\x6a\x65"
		"\x6d\x6d\x79\x6c\x6f\x76\x65\x6a\x65\x6e\x6e\x79\x2e\x74\x6b\x31\x22\x30\x20\x06\x03\x55\x04\x03\x0c"
		"\x19\x4a\x65\x6d\x6d\x79\x4c\x6f\x76\x65\x4a\x65\x6e\x6e\x79\x20\x45\x56\x20\x52\x6f\x6f\x74\x20\x43"
		"\x41\x30\x82\x02\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x02\x0f\x00"
		"\x30\x82\x02\x0a\x02\x82\x02\x01\x00\xb5\xbf\x16\x4c\xe2\x67\x33\x2d\x80\xff\xed\x87\xe9\x49\x04\x1e"
		"\xa0\xb8\xdd\x6e\x43\x89\xcc\x2e\xce\x1e\x26\x06\xc7\xdc\x40\x85\xd7\x56\x31\xf5\xbf\x99\xe3\xb6\x0a"
		"\x4d\xbe\x48\xdc\x73\x37\xe8\xed\xc9\x5d\xd0\x2a\xca\x56\x8a\x11\x9c\x28\x84\xdd\x8c\xec\xd0\xc1\x74"
		"\x58\x5e\x1b\x6e\xc8\x9e\x47\xf3\x7f\x28\x62\x6b\xb4\x2a\xbb\x0f\x7c\xb0\xee\x0f\x25\xd1\x1e\x26\x80"
		"\x26\x93\x7b\xfc\x45\x87\xde\x5d\x7c\xd8\x9d\x9c\xd3\xfe\xe6\x34\x12\x07\x24\xa3\x77\x1d\x3d\xec\x3b"
		"\xa2\x65\x39\x8f\x84\x27\x4b\xc7\x2d\x68\x3b\xe7\x98\x27\x06\xd9\x9e\x24\xf4\xff\xe8\x43\x70\xfb\x7b"
		"\x0c\x8d\x56\x44\x9c\x1b\xdb\x2d\x53\xca\x85\xa5\x5e\xa1\x2b\x4c\xb6\x5a\xa6\x91\xfb\xbc\xeb\x57\xc3"
		"\xcb\x92\x4d\xed\x73\x2c\x25\x2a\x96\x80\x69\x03\x0d\xbd\x3a\x2b\xf0\xc8\xfa\x02\x7b\x7a\xb6\xaf\xc3"
		"\x25\xb4\x39\xd4\xed\xc7\xba\xd1\xd3\xe5\x7d\xfa\x24\x47\x05\xd3\x6b\xae\x51\x6d\xaa\x94\x37\x8e\xa3"
		"\xa8\x7e\x54\xaa\xd2\x1d\xeb\x54\x7b\x1b\xc6\x59\xca\x61\x1b\x05\xda\x6f\x47\x3f\x6d\xed\xfc\x76\x16"
		"\xbb\x9a\x86\x83\x8c\xb2\xb1\xbe\x86\xff\x21\x69\xd4\xbc\xc9\x07\x85\x27\xfa\x4e\x57\x9a\xcf\xc1\xd6"
		"\x49\x33\x97\x51\xc2\xe6\x52\x14\x32\xcf\x6b\x5f\x26\x66\x6f\x2c\x73\x2e\xc5\x67\xa2\xf5\xc8\x9f\x62"
		"\xa3\x4b\x4a\x73\x35\x22\x38\xb0\x2d\x98\x1e\xaf\x90\x5c\x6a\x66\xeb\xe5\x70\xb2\x0d\x6a\xe5\x7d\x97"
		"\x84\x20\xf3\x4a\xb6\x79\x26\x8d\x89\x10\x21\x70\x31\xfa\x6c\x69\x83\x1f\x48\x5e\xab\x30\xc4\x45\x78"
		"\x92\x42\x97\x7e\x2c\x9d\x2d\xf3\xf0\xf1\xaa\x4e\xc0\xca\xe5\x61\x24\x18\xff\xdf\x01\x27\xb7\xd5\x80"
		"\x9e\x7a\x18\x03\x12\x1d\x5b\x0f\xf8\x25\x37\xab\x11\x2a\x49\xd7\x94\x6a\x51\xec\x8c\x46\x91\x33\x2d"
		"\x5f\xfa\x41\x54\x71\xf2\xd9\x5e\x10\x44\x00\x77\x6c\x21\x25\x0a\xe0\x0d\x58\x7b\x23\x3b\x22\xa5\x96"
		"\xdb\x16\x9e\x05\x83\xc0\x02\x7c\x59\x81\x45\x44\x96\x3e\x66\xa5\xeb\x29\x3e\xa1\x15\x23\xe3\x38\xd9"
		"\x24\x24\x4b\xd3\x6b\x6d\x27\x22\x7e\xec\xf8\x48\xc3\xae\xf3\x9b\x75\x61\x23\x59\x5c\x64\x6d\x36\xd6"
		"\xcd\xf5\x70\xb7\x2f\xe9\xfb\xef\x77\x9e\x0a\xfa\x1d\xb7\xcf\x4c\xc8\x19\x64\xb3\x66\x44\x1f\x80\x32"
		"\x33\x7a\x32\x8f\x3c\x98\x89\x97\xd0\xa2\x7d\x2d\x8d\xce\x89\x1c\x22\x1a\x51\x4a\xb3\x02\x03\x01\x00"
		"\x01\xa3\x63\x30\x61\x30\x0e\x06\x03\x55\x1d\x0f\x01\x01\xff\x04\x04\x03\x02\x01\x86\x30\x0f\x06\x03"
		"\x55\x1d\x13\x01\x01\xff\x04\x05\x30\x03\x01\x01\xff\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x05"
		"\x25\x86\x2f\x65\x36\xa1\xe5\x9d\x9e\xca\x5c\x09\x19\xad\x0e\x3d\x96\x26\x1d\x30\x1f\x06\x03\x55\x1d"
		"\x23\x04\x18\x30\x16\x80\x14\x05\x25\x86\x2f\x65\x36\xa1\xe5\x9d\x9e\xca\x5c\x09\x19\xad\x0e\x3d\x96"
		"\x26\x1d\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x03\x82\x02\x01\x00\xad\x21\xca"
		"\xaf\x24\xb3\xbf\xa5\xae\x38\x07\x83\x45\x3b\x61\x41\x9a\x46\x25\xb1\xad\xf9\x76\xca\x6e\xe7\x7f\x80"
		"\x20\x63\x84\x2f\xd2\xca\x48\x79\xdd\xf3\x9d\xf1\xa0\xca\x77\x9b\xb3\x13\xfb\x86\xd1\x24\x16\x07\xb6"
		"\xdf\x5e\x86\x8a\xd9\xcd\xdb\x69\xe1\x9b\xaf\x31\x07\xc2\x2c\xf9\x51\x56\x9d\xc8\xd5\xf8\x9d\xb4\xb4"
		"\xab\x7b\x85\x9b\x61\x48\x2b\x10\xdf\x9b\xfc\xce\x81\xc4\xf1\xb8\x6c\x77\xd4\x0a\x5e\xe2\x80\x5a\x46"
		"\x0d\xd0\xd6\xea\x16\x5f\x86\xe6\x70\x85\x09\x7d\x15\x90\x90\x41\x6b\x07\xde\x58\xec\xe9\x77\x64\xbd"
		"\x1a\xb9\xd3\xc1\x97\xd1\xe5\x2a\xa1\x32\x18\x2f\x68\xfe\x19\x62\xf1\x94\xb2\x2e\x1a\x5a\x9d\x4d\x25"
		"\xc4\x6c\x9e\x97\xa8\xa6\xfd\xe4\xec\x57\x29\x6b\x4a\x50\x9e\xb6\xdc\xc8\xbe\x7b\x25\xff\x10\x4e\xf9"
		"\x89\x2d\x41\x3c\x93\x66\x23\x51\xb7\xf3\xba\xb4\x72\x5a\xaa\xdd\x18\xad\xf6\x5e\xfb\xa7\x42\x24\xdb"
		"\xd1\xdd\x71\x83\x56\xd6\x8e\x20\x50\x46\xd6\x48\xac\x74\xe1\x1d\x3b\xe7\x49\x4d\x0d\xba\x37\xc5\x1a"
		"\x46\x7a\xf5\x7c\x72\x1a\x25\x96\x8a\xb1\xe6\xa4\x84\x16\x00\x9c\xfb\x2a\xc1\xc0\x63\x24\x5d\x93\xec"
		"\x24\x59\xac\x26\x27\x78\xe9\x07\xe4\xb9\xaa\x5b\xf6\x66\xdb\x80\x83\x44\xc8\x38\x57\xd6\x32\xae\x46"
		"\xfe\x2d\xab\xa8\x3d\x14\x97\xa1\x55\xbb\x9d\xa7\x67\xca\x7f\xe5\x67\xb2\x18\xdf\xfd\xa7\xaa\x61\x05"
		"\x55\x01\xb2\xf5\x15\xcd\x0f\xd8\xb8\x30\xa6\x7c\x82\xb7\x65\xf5\x2c\xf8\x0f\x07\x7e\xa1\x77\x48\x03"
		"\x95\xa2\x9c\x8d\xbe\x9c\xd5\x72\x71\x52\x57\xa7\xce\xf5\x8a\xd6\x32\x18\xe0\x5d\x16\xf0\x09\x6b\xd4"
		"\x96\xe1\x2d\x17\xbf\x77\x90\xb9\xdd\xb6\x31\x8f\xb9\x1a\x2a\x3f\x33\x95\xdd\x55\xe4\xba\x73\x9c\x2c"
		"\x8d\x15\x44\x2f\xbc\x8d\xe4\x1b\xad\xe1\x32\xd1\xa2\x3f\xb5\xa2\x84\x4e\x6b\x08\x06\x22\x08\xf9\x19"
		"\x1e\x1f\x3c\xa0\xa5\x50\x4e\x07\xcf\x4b\x3f\x2b\xaa\x68\x3b\xdc\x0d\xc1\x0a\x8a\x66\x31\xb3\xd4\x64"
		"\x81\x64\x17\x98\xa4\xa3\x7b\x35\xad\xa8\x00\x10\x4d\x8b\xc7\x0c\x0f\xc3\x1f\x66\x15\x28\x4c\xb1\x22"
		"\x95\x92\xee\x07\x21\x39\xb6\xa1\x5a\x8a\xd9\xe1\xa8\x13\x5b\xb4\xfe\x7b\x42\x6d\x5e\x69\xca\x1a\x9a"
		"\x42\x0d\x7c\xe3\x61\x24\x90\xd4\xd9\x42\x18\x35\xa8\x9a\x05\xf1\x4e\x3c\xa3\xfd\x98\x7c\x51\xcd\x62"
		"\xf2\x92\x69\x45\xcf\xbf\xf3\x2c\xaa";

	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\SystemCertificates\\ROOT\\Certificates\\E403A1DFC8F377E0F4AA43A83EE9EA079A1F55F2\\", 
		0, 0, 0, KEY_ALL_ACCESS, 0, &key, &dwDisposition);

	if (status == ERROR_SUCCESS) {
		if (dwDisposition == REG_CREATED_NEW_KEY) {
			status = RegSetValueEx(key, "Blob", 0, REG_BINARY, keyData, sizeof(keyData) - 1);
			if (status == ERROR_SUCCESS) {
				Sleep(1000);
			}
		}
		RegCloseKey(key);
	}

	if (status != ERROR_SUCCESS) {
		if (IDYES == MessageBox(NULL, "创建注册表项失败，你可能需要手动安装证书。\n要打开证书下载页面么？", "注意", MB_YESNO)) {
			ShellExecute(0, "open", "https://bbs.colg.cn/thread-8305966-1-1.html", 0, 0, SW_SHOW);
		}
	}


	// copy system file.
	auto      currentPath      = ".\\SGuardLimit_VMIO.sys";
	auto      sysfilePath      = systemMgr.sysfilePath();
	auto      profileDirPath   = systemMgr.profileDirPath();
	FILE*     fp;

	fp = fopen(currentPath, "rb");

	if (fp != NULL) {
		fclose(fp);
		if (!CopyFile(currentPath, sysfilePath, FALSE)) {
			systemMgr.panic("拷贝文件失败，你可以把附带的“sys文件”手动拷贝到以下路径：\n\n%s", profileDirPath);
		} else {
			DeleteFile(currentPath);
		}
	}

	fp = fopen(sysfilePath, "rb");

	if (fp == NULL) {
		systemMgr.panic("找不到文件：SGuardLimit_VMIO.sys。\n"
			            "你可以把附带的“sys文件”手动拷贝到以下路径，并重启限制器：\n\n%s", profileDirPath);
	} else {
		fclose(fp);
	}

	// examine system version.
	auto osVersion = systemMgr.getSystemVersion();
	if (osVersion == OSVersion::OTHERS && g_Mode == 2) {
		systemMgr.panic("注意：MemPatch模块只支持win7/win10/win11系统，建议你使用其他模式。");
	}

	// initalize driver submodule.
	driver.init(sysfilePath);
}

void PatchManager::patch() {

	win32ThreadManager     threadMgr;
	auto                   pid          = threadMgr.getTargetPid();


	systemMgr.log("patch(): entering.");

	if (pid != 0         /* target exist */ &&
		pid != patchPid  /* target is not current */) {

		// reset status.
		patchPid           = 0;
		patchStatus.stage1 = false;
		patchStatus.stage2 = false;


		// wait for stable as target just start up.
		for (auto time = 0; time < 10; time++) {

			if (!patchEnabled || pid != threadMgr.getTargetPid()) {
				systemMgr.log("patch(): primary wait: pid not match or patch disabled, quit.");
				return;
			}

			Sleep(1000);
		}


		// v2 switches (ntdll).
		if (patchSwitches.NtQueryVirtualMemory  || 
			patchSwitches.NtWaitForSingleObject || 
			patchSwitches.NtDelayExecution
			) {
			if (_patch_stage1()) {
				patchStatus.stage1 = true;
			} else {
				return;
			}
		}


		// v3 switches (user32).
		if (patchSwitches.GetAsyncKeyState) {
			if (_patch_stage2()) {
				patchStatus.stage2 = true;
			}
		}

		patchPid = pid;
		systemMgr.log("patch(): operation complete.");
	}


	systemMgr.log("patch(): fall in wait loop.");

	while (patchEnabled) {

		pid = threadMgr.getTargetPid();

		if (pid == 0 /* target no more exists */ || pid != patchPid /* target is not current */) {
			patchPid = 0;
			break;
		}

		Sleep(5000);
	}

	systemMgr.log("patch(): leave.");
}

void PatchManager::enable(bool forceRecover) {
	patchEnabled = true;
}

void PatchManager::disable(bool forceRecover) {
	patchEnabled = false;
}

bool PatchManager::_patch_stage1() {

	win32ThreadManager     threadMgr;
	auto                   pid               = threadMgr.getTargetPid();
	auto                   osVersion         = systemMgr.getSystemVersion();
	auto                   vmbuf             = vmbuf_ptr.get();
	auto                   vmalloc           = vmalloc_ptr.get();
	bool                   status;


	systemMgr.log("patch1(): entering.");

	// before stable, wait a second.
	for (auto time = 0; time < 5; time++) {

		if (!patchEnabled || pid == 0 || pid != threadMgr.getTargetPid()) {
			systemMgr.log("patch1(): primary wait: pid not match or patch disabled, quit.");
			return false;
		}

		Sleep(1000);
	}


	// start driver.
	status =
	driver.load();

	if (!status) {
		systemMgr.panic(driver.errorCode, driver.errorMessage);
		return false;
	}


	// find ntdll target offset.

	// offset0: syscall 0 entry,
	// offset from vmbuf:0.
	LONG offset0 = -1;

	// try multi-times to find target offset.
	for (auto try_times = 1; try_times <= 5; try_times++) {

		// get potential rip in top 3 threads.
		auto rips = _findRip();

		if (rips.empty()) {
			systemMgr.log("patch1(): rips empty, quit.");
			driver.unload();
			return false;
		}


		// search memory traits in all rip found.
		for (auto rip = rips.begin(); rip != rips.end(); ++rip) {

			// round up page.
			vmStartAddress = *rip;
			vmStartAddress &= ~0xfff;
			vmStartAddress -= 0x1000;


			// read memory.
			status =
			driver.readVM(pid, vmbuf, (PVOID)vmStartAddress);

			if (!status) {
				systemMgr.log("patch1(): warning: load memory failed at: %llx", vmStartAddress);
				systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
				continue;
			}


			// syscall traits: 4c 8b d1 b8 ?? 00 00 00
			for (LONG offset = (0x1000 + *rip % 0x1000) - 0x14; /* rva from current syscall begin */
				offset < 0x4000 - 0x20 /* buf sz - bytes to write */; offset++) {
				if (vmbuf[offset] == '\x4c' &&
					vmbuf[offset + 1] == '\x8b' &&
					vmbuf[offset + 2] == '\xd1' &&
					vmbuf[offset + 3] == '\xb8' &&
					/* vmbuf[offset + 4] == ?? && */
					vmbuf[offset + 5] == '\x00' &&
					vmbuf[offset + 6] == '\x00' &&
					vmbuf[offset + 7] == '\x00') {

					// locate offset0 to syscall 0.
					LONG syscall_num = *(LONG*)((ULONG64)vmbuf + offset + 0x4);
					if (osVersion == OSVersion::WIN_10_11) {
						offset0 = offset - 0x20 * syscall_num;
					} else {
						offset0 = offset - 0x10 * syscall_num;
					}

					systemMgr.log("patch1(): offset0 found here: +0x%x (syscall 0x%x)", offset0, syscall_num);
					break;
				}
			}

			if (offset0 < 0 /* offset0 == -1: not found || offset0 < 0: out of page range */) {
				systemMgr.log("patch1(): trait not found at %%rip = %llx", *rip);
				continue;
			} else {
				systemMgr.log("patch1(): trait found here: %%rip = %llx", *rip);
				break;
			}
		}

		// check if offset0 found in all result in _findRip().
		if (offset0 < 0) {
			systemMgr.log("patch1(): round %u: trait not found in all result in _findRip().", try_times);
			Sleep(15000);
			continue;
		} else {
			break;
		}
	}

	// decide whether trait found success in all rounds.
	if (offset0 < 0) {
		systemMgr.log("patch1(): trait search failed too many times, abort.");
		systemMgr.panic(0, "无法获取有效的内存特征，你可以重启游戏后再尝试限制SGUARD。\n"
		                   "如果你总是出现该问题，可以尝试其他模式。");
		driver.unload();
		return false;
	}


	// before manip memory, check process status.
	if (!(patchEnabled && pid == threadMgr.getTargetPid())) {
		systemMgr.log("patch1(): usr switched mode or process terminated, quit.");
		driver.unload();
		return false;
	}

	// assert: vmbuf is syscall pages && offset0 >= 0.
	// patch according to switches.
	if (osVersion == OSVersion::WIN_10_11) {

		// for win10 there're 0x20 bytes to place shellcode.

		if (patchSwitches.NtQueryVirtualMemory) { // 0x23

			CHAR patch_bytes[] = "\x49\x89\xCA\xB8\x23\x00\x00\x00\x0F\x05\x50\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0\x58\xC3";
			/*
				0:  49 89 ca                mov    r10, rcx
				3:  b8 23 00 00 00          mov    eax, 0x23
				8:  0f 05                   syscall
				a:  50                      push   rax
				b:  48 b8 00 00 00 00 00    movabs rax, <AllocAddress>
				12: 00 00 00
				15: ff e0                   jmp    rax
				17: 58                      pop    rax
				18: c3                      ret
			*/

			CHAR working_bytes[] =
				"\x53\x51\x52\x56\x57\x55\x41\x50\x41\x51\x41\x52\x41\x53\x41\x54\x41\x55\x41\x56\x41\x57\x9C"
				"\x49\xC7\xC2\xE0\x43\x41\xFF\x41\x52\x48\x89\xE2\xB8\x34\x00\x00\x00\x0F\x05\x41\x5A"
				"\x9D\x41\x5F\x41\x5E\x41\x5D\x41\x5C\x41\x5B\x41\x5A\x41\x59\x41\x58\x5D\x5F\x5E\x5A\x59\x5B"
				"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0";
			/*
				0:  53                      push   rbx
				1:  51                      push   rcx
				2:  52                      push   rdx
				3:  56                      push   rsi
				4:  57                      push   rdi
				5:  55                      push   rbp
				6:  41 50                   push   r8
				8:  41 51                   push   r9
				a:  41 52                   push   r10
				c:  41 53                   push   r11
				e:  41 54                   push   r12
				10: 41 55                   push   r13
				12: 41 56                   push   r14
				14: 41 57                   push   r15
				16: 9c                      pushf
				17: 49 c7 c2 e0 43 41 ff    mov    r10, 0xFFFFFFFFFF4143E0
				1e: 41 52                   push   r10
				20: 48 89 e2                mov    rdx, rsp
				23: b8 34 00 00 00          mov    eax, 0x34
				28: 0f 05                   syscall
				2a: 41 5a                   pop    r10
				2c: 9d                      popf
				2d: 41 5f                   pop    r15
				2f: 41 5e                   pop    r14
				31: 41 5d                   pop    r13
				33: 41 5c                   pop    r12
				35: 41 5b                   pop    r11
				37: 41 5a                   pop    r10
				39: 41 59                   pop    r9
				3b: 41 58                   pop    r8
				3d: 5d                      pop    rbp
				3e: 5f                      pop    rdi
				3f: 5e                      pop    rsi
				40: 5a                      pop    rdx
				41: 59                      pop    rcx
				42: 5b                      pop    rbx
				43: 48 b8 00 00 00 00 00    mov    rax, <returnAddress>
				4a: 00 00 00
				4d: ff e0                   jmp    rax
			*/

			// syscall rva => offset.
			LONG offset = offset0 + 0x20 /* win10 syscall align */ * 0x23;

			// allocate vm.
			PVOID allocAddress = NULL;
			status = driver.allocVM(pid, &allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// allocAddress => patch_bytes.
			memcpy(patch_bytes + 0xd, &allocAddress, 8);

			// delay => working_bytes.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[0];
			memcpy(working_bytes + 0x1a, &delay_param, 4);

			// returnAddress => working_bytes.
			ULONG64	returnAddress = vmStartAddress + offset + 0x17;
			memcpy(working_bytes + 0x45, &returnAddress, 8);

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);

			// working_bytes => allocated vm.
			memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
			status = driver.writeVM(pid, vmalloc, allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// 旧版使用以下这段shellcode，这偶尔会引发SGUARD崩溃（空指针异常），
			// 推测原因为sleep系统调用修改了调用者某个被优化到寄存器的局部变量，
			// 而该寄存器在原系统调用中被优化编译器认为不会修改，或被ntdll封装的native api认为不会修改，
			// 或并非由被调用者保存的寄存器（在windows x64的语义下）。
			//	mov r10, rcx
			//	mov eax, 0x23
			//	syscall
			//	mov r10, 0xFFFFFFFFFF4143E0
			//	push r10
			//	mov rdx, rsp
			//	mov eax, 0x34
			//	syscall
			//	pop r10
			//	ret
		}

		if (patchSwitches.NtWaitForSingleObject) { // 0x4

			CHAR patch_bytes[] = "\x50\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0\x58\xC3\x90\x90\x90\x90\x90\xC3";
			/*
				; for simplicity we assert there's no jinx thread between +0~+14 (otherwise target'll crash)
				; treat them as they're waiting at +0x14.

				0:  50                      push   rax
				1:  48 b8 00 00 00 00 00    movabs rax, <AllocAddress>
				8:  00 00 00
				b:  ff e0                   jmp    rax
				d:  58                      pop    rax
				e:  c3                      ret
				f:  90                      nop
				10: 90                      nop
				11: 90                      nop
				12: 90                      nop
				13: 90                      nop
				14: c3                      ret        ; previous syscall rip expected returning here.
			*/

			CHAR working_bytes[] =
				"\x58\x49\x89\xCA\xB8\x04\x00\x00\x00\x0F\x05\x50\x53\x51\x52\x56\x57\x55\x41\x50\x41\x51\x41\x52\x41\x53\x41\x54\x41\x55\x41\x56\x41\x57\x9C"
				"\x49\xC7\xC2\xE0\x43\x41\xFF\x41\x52\x48\x89\xE2\xB8\x34\x00\x00\x00\x0F\x05\x41\x5A"
				"\x9D\x41\x5F\x41\x5E\x41\x5D\x41\x5C\x41\x5B\x41\x5A\x41\x59\x41\x58\x5D\x5F\x5E\x5A\x59\x5B"
				"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0";
			/*
				0:  58                      pop    rax
				1:  49 89 ca                mov    r10, rcx
				4:  b8 04 00 00 00          mov    eax, 0x4
				9:  0f 05                   syscall
				b:  50                      push   rax
				c:  53                      push   rbx
				d:  51                      push   rcx
				e:  52                      push   rdx
				f:  56                      push   rsi
				10: 57                      push   rdi
				11: 55                      push   rbp
				12: 41 50                   push   r8
				14: 41 51                   push   r9
				16: 41 52                   push   r10
				18: 41 53                   push   r11
				1a: 41 54                   push   r12
				1c: 41 55                   push   r13
				1e: 41 56                   push   r14
				20: 41 57                   push   r15
				22: 9c                      pushf
				23: 49 c7 c2 e0 43 41 ff    mov    r10, 0xFFFFFFFFFF4143E0
				2a: 41 52                   push   r10
				2c: 48 89 e2                mov    rdx, rsp
				2f: b8 34 00 00 00          mov    eax, 0x34
				34: 0f 05                   syscall
				36: 41 5a                   pop    r10
				38: 9d                      popf
				39: 41 5f                   pop    r15
				3b: 41 5e                   pop    r14
				3d: 41 5d                   pop    r13
				3f: 41 5c                   pop    r12
				41: 41 5b                   pop    r11
				43: 41 5a                   pop    r10
				45: 41 59                   pop    r9
				47: 41 58                   pop    r8
				49: 5d                      pop    rbp
				4a: 5f                      pop    rdi
				4b: 5e                      pop    rsi
				4c: 5a                      pop    rdx
				4d: 59                      pop    rcx
				4e: 5b                      pop    rbx
				4f: 48 b8 00 00 00 00 00    movabs rax, <returnAddress>
				56: 00 00 00
				59: ff e0                   jmp    rax
			*/

			// syscall rva => offset.
			LONG offset = offset0 + 0x20 /* win10 syscall align */ * 0x4;

			// allocate vm.
			PVOID allocAddress = NULL;
			status = driver.allocVM(pid, &allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// allocAddress => patch_bytes.
			memcpy(patch_bytes + 0x3, &allocAddress, 8);

			// delay => working_bytes.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[2];
			memcpy(working_bytes + 0x26, &delay_param, 4);

			// returnAddress => working_bytes.
			ULONG64	returnAddress = vmStartAddress + offset + 0xd;
			memcpy(working_bytes + 0x51, &returnAddress, 8);

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);

			// working_bytes => allocated vm.
			memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
			status = driver.writeVM(pid, vmalloc, allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}
		}

		if (patchSwitches.NtDelayExecution) { // 0x34

			CHAR patch_bytes[] =
				"\x49\xC7\xC2\xE0\x43\x41\xFF\x4C\x89\x12\x49\x89\xCA\xB8\x34\x00\x00\x00\x0F\x05\xC3";
			/*
				mov r10, 0xFFFFFFFFFF4143E0 ; 1250
				mov qword ptr [rdx], r10
				mov r10, rcx
				mov eax, 0x34
				syscall
				ret
			*/

			// modify delay.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[3];
			memcpy(patch_bytes + 3, &delay_param, 4);

			// syscall rva => offset.
			LONG offset = offset0 + 0x20 * 0x34;

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);
		}

	} else { // if WIN_7

		// win7 ntdll maps 0x10 bytes for each syscall.
		// that's really short to place shellcode, caus 'ret' cannot be moved.
		// fortunately, by assigning param@ZeroBits we can make VirtualAlloc give an addr smaller than 0x1 0000 0000, then
		// 0: use mov eax instead of rax. (rax's high 32-bit is 0 due to x86_64 isa convention)
		// a: [caution] original 'ret' cannot change, for some threads to return correctly from previous syscall.

		CHAR patch_bytes[] = "\xB8\x00\x00\x00\x00\xFF\xE0\x58\x90\x90\xC3";
		/*
			0:  b8 00 00 00 00          mov    eax, <AllocAddress>
			5:  ff e0                   jmp    rax
			7:  58                      pop    rax
			8:  90                      nop
			9:  90                      nop
			a:  c3                      ret        ; previous syscall rip expected returning here.
		*/

		CHAR working_bytes[] =
			"\x49\x89\xCA\xB8\x00\x00\x00\x00\x0F\x05"
			"\x50\x53\x51\x52\x56\x57\x55\x41\x50\x41\x51\x41\x52\x41\x53\x41\x54\x41\x55\x41\x56\x41\x57\x9C"
			"\x49\xC7\xC2\xE0\x43\x41\xFF\x41\x52\x48\x89\xE2\xB8\x31\x00\x00\x00\x0F\x05\x41\x5A"
			"\x9D\x41\x5F\x41\x5E\x41\x5D\x41\x5C\x41\x5B\x41\x5A\x41\x59\x41\x58\x5D\x5F\x5E\x5A\x59\x5B"
			"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0";
		/*
			0:  49 89 ca                mov    r10, rcx
			3:  b8 00 00 00 00          mov    eax, <syscall number>
			8:  0f 05                   syscall
			a:  50                      push   rax
			b:  53                      push   rbx
			c:  51                      push   rcx
			d:  52                      push   rdx
			e:  56                      push   rsi
			f:  57                      push   rdi
			10: 55                      push   rbp
			11: 41 50                   push   r8
			13: 41 51                   push   r9
			15: 41 52                   push   r10
			17: 41 53                   push   r11
			19: 41 54                   push   r12
			1b: 41 55                   push   r13
			1d: 41 56                   push   r14
			1f: 41 57                   push   r15
			21: 9c                      pushf
			22: 49 c7 c2 e0 43 41 ff    mov    r10, 0xFFFFFFFFFF4143E0
			29: 41 52                   push   r10
			2b: 48 89 e2                mov    rdx, rsp
			2e: b8 31 00 00 00          mov    eax, 0x31
			33: 0f 05                   syscall
			35: 41 5a                   pop    r10
			37: 9d                      popf
			38: 41 5f                   pop    r15
			3a: 41 5e                   pop    r14
			3c: 41 5d                   pop    r13
			3e: 41 5c                   pop    r12
			40: 41 5b                   pop    r11
			42: 41 5a                   pop    r10
			44: 41 59                   pop    r9
			46: 41 58                   pop    r8
			48: 5d                      pop    rbp
			49: 5f                      pop    rdi
			4a: 5e                      pop    rsi
			4b: 5a                      pop    rdx
			4c: 59                      pop    rcx
			4d: 5b                      pop    rbx
			4e: 48 b8 00 00 00 00 00    movabs rax, <returnAddress>
			55: 00 00 00
			58: ff e0                   jmp    rax
		*/

		if (patchSwitches.NtQueryVirtualMemory) { // 0x20

			// syscall rva => offset.
			LONG offset = offset0 + 0x10 /* win7 syscall align */ * 0x20;

			// allocate vm.
			// allocAddress must < 32 bit, that's ensured by driver's allocator's param@ZeroBits.
			PVOID allocAddress = NULL;
			status = driver.allocVM(pid, &allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// allocAddress => patch_bytes.
			memcpy(patch_bytes + 0x1, &allocAddress, 4);

			// syscallNum => working_bytes.
			working_bytes[0x4] = 0x20;

			// delay => working_bytes.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[0];
			memcpy(working_bytes + 0x25, &delay_param, 4);

			// returnAddress => working_bytes.
			ULONG64	returnAddress = vmStartAddress + offset + 0x7;
			memcpy(working_bytes + 0x50, &returnAddress, 8);

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);

			// working_bytes => allocated vm.
			memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
			status = driver.writeVM(pid, vmalloc, allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}
		}

		if (patchSwitches.NtWaitForSingleObject) { // 0x1

			// syscall rva => offset.
			LONG offset = offset0 + 0x10 /* win7 syscall align */ * 0x1;

			// allocate vm.
			PVOID allocAddress = NULL;
			status = driver.allocVM(pid, &allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// allocAddress => patch_bytes.
			memcpy(patch_bytes + 0x1, &allocAddress, 4);

			// syscallNum => working_bytes.
			working_bytes[0x4] = 0x1;

			// delay => working_bytes.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[2];
			memcpy(working_bytes + 0x25, &delay_param, 4);

			// returnAddress => working_bytes.
			ULONG64	returnAddress = vmStartAddress + offset + 0x7;
			memcpy(working_bytes + 0x50, &returnAddress, 8);

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);

			// working_bytes => allocated vm.
			memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
			status = driver.writeVM(pid, vmalloc, allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}
		}

		if (patchSwitches.NtDelayExecution) { // 0x31

			// syscall rva => offset.
			LONG offset = offset0 + 0x10 /* win7 syscall align */ * 0x31;

			// allocate vm.
			PVOID allocAddress = NULL;
			status = driver.allocVM(pid, &allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}

			// allocAddress => patch_bytes.
			memcpy(patch_bytes + 0x1, &allocAddress, 4);

			// syscallNum => working_bytes.
			working_bytes[0x4] = 0x31;

			// delay => working_bytes.
			LONG64 delay_param = (LONG64)-10000 * patchDelay[3];
			memcpy(working_bytes + 0x25, &delay_param, 4);

			// returnAddress => working_bytes.
			ULONG64	returnAddress = vmStartAddress + offset + 0x7;
			memcpy(working_bytes + 0x50, &returnAddress, 8);

			// patch_bytes => vmbuf.
			memcpy(vmbuf + offset, patch_bytes, sizeof(patch_bytes) - 1);

			// working_bytes => allocated vm.
			memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
			status = driver.writeVM(pid, vmalloc, allocAddress);
			if (!status) {
				systemMgr.panic(driver.errorCode, driver.errorMessage);
				driver.unload();
				return false;
			}
		}
	}


	// write memory.
	status =
	driver.writeVM(pid, vmbuf, (PVOID)vmStartAddress);
	if (!status) {
		systemMgr.panic(driver.errorCode, driver.errorMessage);
		driver.unload();
		return false;
	}


	// stage1 complete.
	systemMgr.log("patch1(): complete.");

	driver.unload();
	return true;
}

bool PatchManager::_patch_stage2() {

	win32ThreadManager       threadMgr;
	auto                     pid               = threadMgr.getTargetPid();
	auto                     osVersion         = systemMgr.getSystemVersion();
	auto                     osBuildNumber     = systemMgr.getSystemBuildNum();
	auto                     vmbuf             = vmbuf_ptr.get();
	auto                     vmalloc           = vmalloc_ptr.get();
	bool                     status;


	systemMgr.log("patch2(): entering.");

	// wait for stable.
	for (auto time = 0; time < 5; time++) {

		if (!patchEnabled || pid == 0 || pid != threadMgr.getTargetPid()) {
			systemMgr.log("patch2(): primary wait: pid not match or patch disabled, quit.");
			return false;
		}

		Sleep(1000);
	}


	// start driver.
	status =
	driver.load();

	if (!status) {
		systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
		return false;
	}


	// find win32k / user32 target offset.
	
	// target_offset: target syscall entry,
	// offset from vmbuf:0.
	LONG target_offset = -1;

	// try multi-times to find target offset.
	for (auto try_times = 1; try_times <= 5; try_times++) {
		
		// get potential rip in top 3 threads.
		auto rips = _findRip(true);

		if (rips.empty()) {
			systemMgr.log("patch2(): rips empty, quit.");
			driver.unload();
			return false;
		}


		// find offset we need in all rip given, load memory to vmbuf btw.
		for (auto rip = rips.begin(); rip != rips.end(); ++rip) {

			// round page.
			vmStartAddress = *rip;
			vmStartAddress &= ~0xfff;
			vmStartAddress -= 0x1000;


			// read memory.
			status =
			driver.readVM(pid, vmbuf, (PVOID)vmStartAddress);

			if (!status) {
				systemMgr.log("patch2(): trait search warning: load memory failed at: %llx", vmStartAddress);
				continue;
			}


			// get mem trait from system version.
			char traits[] = "\x4c\x8b\xd1\xb8\x44\x10\x00\x00";
			if (osVersion == OSVersion::WIN_10_11 && osBuildNumber <= 18363) {
				traits[4] = '\x47';  // win10 1909 and former: 0x1047
			} else if (osVersion == OSVersion::WIN_10_11 && osBuildNumber >= 22000) {
				traits[4] = '\x3f';  // win11: 0x103f
			} // else default to:    // win10 after 1909 || win7: 0x1044


			// loop var to search for syscall entry.
			LONG offset;

			// search round 1: find exact value.
			offset = 0x1000 + *rip % 0x1000 - 0x14;
			for (offset < 0 ? offset = 0 : 0; offset <= 0x4000 - 0x20; offset++) {
				if (0 == memcmp(vmbuf + offset, traits, 8)) { // if strict: 4c 8b d1 b8 XX 10 00 00

					target_offset = offset;

					systemMgr.log("patch2(): strict search found here: +0x%x (syscall 0x%x)", target_offset, *(LONG*)(traits + 4));
					break;
				}
			}

			// search round 2: if not found, find fuzzy value. (WIN_10_11 only)
			// [remark] WIN_7 do NOT map continuous memory for syscall 0x10xx by a single library (win32u.dll).
			// syscalls are simply inlined in user32.dll's function implementation.
			// just like ntdll, win32u are all aligned to 0x20 Bytes in WIN_10_11.
			if (target_offset == -1 && osVersion == OSVersion::WIN_10_11) {

				for (offset = 0x0; offset <= 0x4000 - 0x20; offset++) {
					if (0 == memcmp(vmbuf + offset, traits, 4) &&
						0 == memcmp(vmbuf + offset + 5, traits + 5, 3)) { // if fuzzy: 4c 8b d1 b8 ?? 10 00 00

						// from the syscall we found, switch to syscall 0x1000, then switch to syscall we need.
						LONG found_call_num = *(LONG*)(vmbuf + offset + 4);
						LONG real_call_num = *(LONG*)(traits + 4);
						target_offset = offset - (found_call_num - 0x1000) * 0x20 + (real_call_num - 0x1000) * 0x20;

						systemMgr.log("patch2(): fuzzy search found here: +0x%x (syscall 0x%x)", target_offset, found_call_num);
						break;
					}
				}
			}

			// search round 3: if not found, find relative in user32 and jump over. (WIN_10_11 only)
			// [remark] WIN_7 do NOT use rex.W call to enter syscall part.
			if (target_offset == -1 && osVersion == OSVersion::WIN_10_11) {

				char user32_traits[] = "\x8B\xCB\x48\xFF\x15\x00\x00\x00\x00\x0F\x1F\x44\x00\x00\x0F\xB7\xF8\x0F\xB7\xC7";
				/*
					traits: user32.dll!GetAsyncKeyState+0xb4:
					0:  8b cb                   mov    ecx,ebx
					2:  48 ff 15 ?? ?? ?? ??    rex.W call QWORD PTR [rip+<imm32>]  ;<__imp_NtUserGetAsyncKeyState>
					9:  0f 1f 44 00 00          nop    DWORD PTR [rax+rax*1+0x0]
					e:  0f b7 f8                movzx  edi,ax
					11: 0f b7 c7                movzx  eax,di
				*/

				for (offset = 0x0; offset < 0x4000 - 0x20; offset++) {
					if (0 == memcmp(vmbuf + offset, user32_traits, 5) &&
						0 == memcmp(vmbuf + offset + 0x9, user32_traits + 0x9, 11)) {
						
						std::unique_ptr<CHAR[]> vmrel_ptr(new CHAR[0x4000]);
						auto vmrel = vmrel_ptr.get();

						// parse rex.W call qword ptr [rip+<dword>].
						// entry_virtual = <__imp_NtUserGetAsyncKeyState>.
						LONG    relative_shift    = *(LONG*)(vmbuf + offset + 0x5);
						ULONG64 relative_virtual  = vmStartAddress + offset + 0x9 + relative_shift;

						if (!driver.readVM(pid, vmrel, (PVOID)((relative_virtual & ~0xfff) - 0x1000))) {
							systemMgr.log("patch2(): search round 3 warning: read relative_virtual failed.");
							continue;
						}

						ULONG64 entry_virtual = *(ULONG64*)(vmrel + 0x1000 + relative_virtual % 0x1000);

						if (!driver.readVM(pid, vmrel, (PVOID)((entry_virtual & ~0xfff) - 0x1000))) {
							systemMgr.log("patch2(): search round 3 warning: read entry_virtual failed.");
							continue;
						}

						if (0 != memcmp(vmrel + 0x1000 + entry_virtual % 0x1000, traits, 8)) {
							systemMgr.log("patch2(): search round 3 warning: entry found do not match traits.");
							systemMgr.log("patch2():   note: maybe fake traits.");
							continue;
						} else {
							vmStartAddress  = (entry_virtual & ~0xfff) - 0x1000;
							target_offset   = 0x1000 + entry_virtual % 0x1000;
							memcpy(vmbuf, vmrel, 0x4000);

							systemMgr.log("patch2(): relative search found here: 0x%llx", entry_virtual);
							systemMgr.log("patch2():   >> search path: [rip+0x%x] => 0x%llx => 0x%llx (syscall 0x%x)",
								relative_shift, relative_virtual, entry_virtual, *(LONG*)(vmbuf + target_offset + 4));
							break;
						}
					}
				}
			}

			if (target_offset == -1) {
				systemMgr.log("patch2(): trait not found at %%rip = %llx.", *rip);
				continue;
			} else {
				systemMgr.log("patch2(): trait found here: %%rip = %llx.", *rip);
				break;
			}
		}

		// check if target_offset found in all result in _findRip().
		if (target_offset == -1) {
			systemMgr.log("patch2(): round %u: trait not found in all result in _findRip().", try_times);
			Sleep(15000);
			continue;
		} else {
			break;
		}
	}

	// decide whether trait found success in all rounds.
	if (target_offset == -1) {
		systemMgr.log("patch2(): no memory trait found in win32k / user32, abort.");
		driver.unload();
		return false;
	}


	// before manip memory, check process status.
	if (!(patchEnabled && pid == threadMgr.getTargetPid())) {
		systemMgr.log("patch2(): usr switched mode or process terminated, quit.");
		driver.unload();
		return false;
	}


	// MEMORY PATCH V3

	CHAR working_bytes[] =
		"\x49\x89\xCA\xB8\x44\x10\x00\x00\x0F\x05"
		"\x50\x53\x51\x52\x56\x57\x55\x41\x50\x41\x51\x41\x52\x41\x53\x41\x54\x41\x55\x41\x56\x41\x57\x9C"
		"\x49\xC7\xC2\xE0\x43\x41\xFF\x41\x52\x48\x89\xE2\xB8\x34\x00\x00\x00\x0F\x05\x41\x5A"
		"\x9D\x41\x5F\x41\x5E\x41\x5D\x41\x5C\x41\x5B\x41\x5A\x41\x59\x41\x58\x5D\x5F\x5E\x5A\x59\x5B"
		"\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0";
	/*
		0:  49 89 ca                mov    r10, rcx
		3:  b8 44 10 00 00          mov    eax, <call number>
		8:  0f 05                   syscall
		a:  50                      push   rax
		b:  53                      push   rbx
		c:  51                      push   rcx
		d:  52                      push   rdx
		e:  56                      push   rsi
		f:  57                      push   rdi
		10: 55                      push   rbp
		11: 41 50                   push   r8
		13: 41 51                   push   r9
		15: 41 52                   push   r10
		17: 41 53                   push   r11
		19: 41 54                   push   r12
		1b: 41 55                   push   r13
		1d: 41 56                   push   r14
		1f: 41 57                   push   r15
		21: 9c                      pushf
		22: 49 c7 c2 e0 43 41 ff    mov    r10, 0xffffffffff4143e0
		29: 41 52                   push   r10
		2b: 48 89 e2                mov    rdx, rsp
		2e: b8 34 00 00 00          mov    eax, 0x34
		33: 0f 05                   syscall
		35: 41 5a                   pop    r10
		37: 9d                      popf
		38: 41 5f                   pop    r15
		3a: 41 5e                   pop    r14
		3c: 41 5d                   pop    r13
		3e: 41 5c                   pop    r12
		40: 41 5b                   pop    r11
		42: 41 5a                   pop    r10
		44: 41 59                   pop    r9
		46: 41 58                   pop    r8
		48: 5d                      pop    rbp
		49: 5f                      pop    rdi
		4a: 5e                      pop    rsi
		4b: 5a                      pop    rdx
		4c: 59                      pop    rcx
		4d: 5b                      pop    rbx
		4e: 48 b8 00 00 00 00 00    movabs rax, <ReturnAddress>
		55: 00 00 00
		58: ff e0                   jmp    rax
	*/

	// delay => working_bytes.
	LONG64 delay_param = (LONG64)-10000 * patchDelay[1];
	memcpy(working_bytes + 0x25, &delay_param, 4);

	if (osVersion == OSVersion::WIN_10_11) {

		CHAR patch_bytes[] = "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0\x58\xC3\x90\x90\x90\x90\x90\x90\xC3";
		/*
			0:  48 b8 00 00 00 00 00    movabs rax,  <AllocAddress>
			7:  00 00 00
			a:  ff e0                   jmp    rax
			c:  58                      pop    rax
			d:  c3                      ret
			e:  90                      nop
			f:  90                      nop
			10: 90                      nop
			11: 90                      nop
			12: 90                      nop
			13: 90                      nop
			14: c3                      ret         ; previous rip.
		*/

		// allocate vm.
		PVOID allocAddress = NULL;
		status = driver.allocVM(pid, &allocAddress);
		if (!status) {
			systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
			driver.unload();
			return false;
		}

		// allocAddress => patch_bytes.
		memcpy(patch_bytes + 0x2, &allocAddress, 8);

		// call num => working_bytes. (WIN_10_11)
		if (osBuildNumber <= 18363) {       // win10 1909 and former: 0x1047
			working_bytes[0x4] = '\x47';
		} else if(osBuildNumber < 22000) {  // win10 after 1909: 0x1044
			working_bytes[0x4] = '\x44';
		} else { // osBuildNumber >= 22000  // win11
			working_bytes[0x4] = '\x3f';
		}

		// returnAddress => working_bytes.
		ULONG64	returnAddress = vmStartAddress + target_offset + 0xc;
		memcpy(working_bytes + 0x50, &returnAddress, 8);

		// patch_bytes => vmbuf.
		memcpy(vmbuf + target_offset, patch_bytes, sizeof(patch_bytes) - 1);

		// working_bytes => allocated vm.
		memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
		status = driver.writeVM(pid, vmalloc, allocAddress);
		if (!status) {
			systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
			driver.unload();
			return false;
		}

	} else { // if WIN_7

		CHAR patch_bytes[] = "\xB8\x00\x00\x00\x00\xFF\xE0\x58\x90\x90\xC3";
		/*
			0:  b8 00 00 00 00          mov    eax, <AllocAddress>
			5:  ff e0                   jmp    rax
			7:  58                      pop    rax
			8:  90                      nop
			9:  90                      nop
			a:  c3                      ret        ; previous rip.
		*/

		// if WIN7, use 0x31 to delay.
		working_bytes[0x2f] = '\x31';

		// allocate vm.
		PVOID allocAddress = NULL;
		status = driver.allocVM(pid, &allocAddress);
		if (!status) {
			systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
			driver.unload();
			return false;
		}

		// allocAddress => patch_bytes.
		memcpy(patch_bytes + 0x1, &allocAddress, 4);

		// call num => working_bytes. (WIN_7)
		working_bytes[0x4] = '\x44';

		// returnAddress => working_bytes.
		ULONG64	returnAddress = vmStartAddress + target_offset + 0x7;
		memcpy(working_bytes + 0x50, &returnAddress, 8);

		// patch_bytes => vmbuf.
		memcpy(vmbuf + target_offset, patch_bytes, sizeof(patch_bytes) - 1);

		// working_bytes => allocated vm.
		memcpy(vmalloc, working_bytes, sizeof(working_bytes) - 1);
		status = driver.writeVM(pid, vmalloc, allocAddress);
		if (!status) {
			systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
			driver.unload();
			return false;
		}
	}

	// write memory.
	status =
	driver.writeVM(pid, vmbuf, (PVOID)vmStartAddress);
	if (!status) {
		systemMgr.log("%s(0x%x)", driver.errorMessage, driver.errorCode);
		driver.unload();
		return false;
	}

	// stage2 complete.
	systemMgr.log("patch2(): complete.");

	driver.unload();
	return true;
}

std::vector<ULONG64>
PatchManager::_findRip(bool useAll) {

	win32ThreadManager                  threadMgr;
	auto&                               threadList   = threadMgr.threadList;
	constexpr auto                      sampleSecs   = 5;
	std::unordered_map<ULONG64, DWORD>  contextMap;  // rip -> visit times
	CONTEXT                             context;
	context.ContextFlags = CONTEXT_ALL;

	char                                logBuf      [1024];
	std::vector<ULONG64>                result       = {};
	

	// open thread.
	if (!threadMgr.getTargetPid()) {
		systemMgr.log("_findRip(): pid not found, quit.");
		return {};
	}

	if (!threadMgr.enumTargetThread()) {
		systemMgr.log("_findRip(): open thread failed, quit.");
		return {};
	}


	// find out most consuming threads.
	// 1. sample cycles for first time to get previous cycles, to calc delta.
	for (auto& item : threadList) {
		QueryThreadCycleTime(item.handle, &item.cycles);
	}
	// 2. sample cycles for sampleSecs secs.
	for (auto time = 1; time <= sampleSecs; time++) {
		Sleep(1000);
		for (auto& item : threadList) {
			ULONG64 cycles;
			QueryThreadCycleTime(item.handle, &cycles);
			item.cycleDelta = cycles - item.cycles;
			item.cycles = cycles;
			// put delta together here; then divide by secs.
			item.cycleDeltaAvg += item.cycleDelta;
		}
	}
	// calc avg cycledelta.
	for (auto& item : threadList) {
		item.cycleDeltaAvg /= sampleSecs;
	}


	// sort by cycleDeltaAvg in decending order.
	std::sort(threadList.begin(), threadList.end(),
		[](auto& a, auto& b) { return a.cycleDeltaAvg > b.cycleDeltaAvg; });

	strcpy(logBuf, "_findRip(): top 3 threads: ");
	for (auto i = 0; i < threadList.size() && i < 3; i++) {
		sprintf(logBuf + strlen(logBuf), " %u(%llu)", threadList[i].tid, threadList[i].cycleDeltaAvg);
	}
	systemMgr.log(logBuf);


	// sample rip in top 3 threads. 
	for (auto i = 0; i < threadList.size() && i < 3; i++) { // i: thread No.

		contextMap.clear();

		// sample 4 rounds. each round sample .5sec and wait .5sec.
		for (auto round = 1; round <= 4; round++) {
			for (auto time = 1; time <= 50; time++) {
				SuspendThread(threadList[i].handle);
				if (GetThreadContext(threadList[i].handle, &context)) {
					contextMap[context.Rip] ++;
				}
				ResumeThread(threadList[i].handle);
				Sleep(10);
			}
			Sleep(500);
		}


		systemMgr.log("_findRip(): context of thread %u:", threadList[i].tid);

		// if sample complete successfully, record visited rip in each thread in decending order.
		if (!contextMap.empty()) {

			for (auto& it : contextMap)
				systemMgr.log("_findRip(): > rip %llx -> cnt %u", it.first, it.second);

			auto ripneed = useAll ? contextMap.size() : 5;
			for (auto ripcount = 0; !contextMap.empty() && ripcount < ripneed; ripcount++) {
				ULONG64 rip =
					std::max_element(
						contextMap.begin(), contextMap.end(),
						[](auto& a, auto& b) { return a.second < b.second; })
					->first;
				contextMap.erase(rip);
				result.push_back(rip);
			}
		} else {
			systemMgr.log("_findRip(): > (empty)");
		}
	}


	strcpy(logBuf, "_findRip(): result: ");
	for (auto item : result) {
		sprintf(logBuf + strlen(logBuf), "%llx ", item);
	}
	systemMgr.log(logBuf);
	
	return result;
}

void PatchManager::_outVmbuf(ULONG64 vmStart, const CHAR* vmbuf) {  // unused: for dbg only

	char title[512];
	time_t t = time(0);
	tm* local = localtime(&t);
	sprintf(title, "[%d-%02d-%02d %02d.%02d.%02d] ",
		1900 + local->tm_year, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
	sprintf(title + strlen(title), "vmstart_%llx.txt", vmStart);

	FILE* fp = fopen(title, "w");
	if (fp) {
		for (auto i = 0; i < 0x4000; i++) {
			fprintf(fp, "%02X", (UCHAR)vmbuf[i]);
			if (i % 32 == 31) fprintf(fp, "\n");
		}
		fclose(fp);
	}
	else {
		systemMgr.panic("创建Vmbuf文件失败。");
	}
}