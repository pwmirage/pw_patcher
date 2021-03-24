#include <stdio.h>
#include <libgen.h>
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>

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
	hURL = InternetOpenUrl(hInternetSession, url,  NULL, 0, 0, 0);

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

int
main(void)
{
	int rc, i;
	char buf[MAX_PATH] = {};

	GetModuleFileName(NULL, buf, sizeof(buf));
	SetCurrentDirectory(dirname(buf));

	rc = download("https://pwmirage.com/pwmirage.exe", "pwmirage.dat");
	if (rc != 0) {
		MessageBox(0, "Failed to download the new patcher", "Status", MB_OK);
		return 1;
	}

	for (i = 0; i < 10; i++) {
		rc = unlink("../pwmirage.exe");
		if (rc == 0) {
			break;
		}
		Sleep(300);
	}

	if (i == 10 || rename("pwmirage.dat", "../pwmirage.exe") != 0) {
		MessageBox(0, "Failed to replace the old patcher. Could it be it's still running?", "Status", MB_OK);
		return 1;
	}

	SetCurrentDirectory("..");
	ShellExecute(NULL, NULL, "pwmirage.exe", NULL, NULL, SW_SHOW);
	return 0;
}
