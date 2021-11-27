/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <libgen.h>
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <unistd.h>

static int
download(const char *url, char *filename)
{
	HINTERNET hInternetSession;
	HINTERNET hURL;
	BOOL success;
	DWORD num_bytes = 1;
	FILE *fp;
	DWORD flags;

	fp = fopen(filename, "wb");
	if (!fp) {
		return -errno;
	}

	hInternetSession = InternetOpen("Mirage Patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternetSession) {
		fclose(fp);
		return -1;
	}

	flags = INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE;
	hURL = InternetOpenUrl(hInternetSession, url,  NULL, 0, flags, 0);

	char buf[1024];

	while (num_bytes > 0) {
		success = InternetReadFile(hURL, buf, (DWORD)sizeof(buf), &num_bytes);
		if (!success) {
			break;
		}
		fwrite(buf, 1, (size_t)num_bytes, fp);
	}

	InternetCloseHandle(hURL);
	InternetCloseHandle(hInternetSession);

	fclose(fp);
	return success ? 0 : -1;
}

int WINAPI
WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance,
				LPSTR cmdline, int nCmdShow)
{
	HINSTANCE hinstLib; 
	int (*init)(HINSTANCE hinst); 
	int i, rc;

	if (access("patcher", F_OK) != 0) {
		MessageBox(NULL, "Can't find the \"patcher\" directory. Please redownload the full client.", "Error", MB_OK);
		return 1;
	}

	if (strcmp(cmdline, "--update-self") == 0) {
		rc = download("https://pwmirage.com/launcher.dll", "patcher/launcher.dll.new");
		if (rc != 0) {
			MessageBox(NULL, "Failed to download the new launcher.", "Error", MB_OK);
			return 1;
		}

		for (i = 0; i < 10; i++) {
			rc = unlink("patcher/launcher.dll");
			if (rc == 0) {
				break;
			}
			Sleep(300);
		}

		if (rename("patcher/launcher.dll.new", "patcher/launcher.dll") != 0) {
			MessageBox(0, "Failed to replace the old launcher. Could it be it's still running?", "Error", MB_OK);
			return 1;
		}
	}

	hinstLib = LoadLibrary("patcher/launcher.dll");
	if (hinstLib == NULL) { 
		rc = download("https://pwmirage.com/launcher.dll", "patcher/launcher.dll");
		if (rc) {
			MessageBox(NULL, "Failed to download the rest of the launcher", "Error", MB_OK);
			return 1;
		}

		hinstLib = LoadLibrary("patcher/launcher.dll");
		if (hinstLib == NULL) { 
			MessageBox(NULL, "Can't load the launcher dll. Please redownload the full client.", "Error", MB_OK);
			return 1;
		}
	}

	init = (void *)GetProcAddress(hinstLib, "init"); 
	if (init == NULL) {
		MessageBox(NULL, "Can't initialize the launcher dll. Please redownload the full client.", "Error", MB_OK);
		return 1;
	}

	rc = init(hInst);
	FreeLibrary(hinstLib);

	return rc;
}
