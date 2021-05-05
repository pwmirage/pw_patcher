/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include <windows.h>
#include <stdio.h>
#include <io.h>

#include "pw_pck.h"

static void
print_help(char *argv[0])
{
	printf("mgpck -x <path\\to\\archive.pck>\n");
	printf("mgpck -u <path\\to\\archive.pck>\n");
	printf("mgpck -g <path\\to\\archive.pck> <path\\for\\file.pck.patch>\n");
	printf("mgpck -a <path\\to\\archive.pck> <path\\to\\file.pck.patch>\n");
	printf("\n");
	printf("Actions:\n");
	printf("  -x, --extract        extract *.pck archive into *.pck.files dir in the\n");
	printf("                       current dir. If the dir already exists a popup will ask\n");
	printf("                       if you want to override it\n");
	printf("  -u, --update         update *.pck archive with modified files from the\n");
	printf("                       *.pck.files dir. Only files after the last extract/\n");
	printf("                       update/apply will be updated. This can be used to\n");
	printf("                       quickly test changes without creating a .patch for them.\n");
	printf("                       NOTE: The first update might take a long while to finish\n");
	printf("                       because the whole .pck needs to be rebuilt into a more\n");
	printf("                       mgpck-aware version.\n");
	printf("  -g, --gen-patch      create a *.pck.patch file that will include any changes\n");
	printf("                       made in the *.pck.files after the last --apply-patch.\n");
	printf("                       The patch will include any changes that were already\n");
	printf("                       applied to the *.pck file with -u or --update\n");
	printf("  -a, --apply-patch    apply a *.pck.patch file on the *.pck. If there were\n");
	printf("                       updates or just local changes to files which are also\n");
	printf("                       modified here, those changes will be lost. (no files in\n");
	printf("                       *.pck.files dir will be modified, but they won't be\n");
	printf("                       picked up by -u or -g until they're modified again.\n");
	printf("\n");
	printf("The extracted *.pck archive contains extra files *_aliases.cfg and *_mgpck.log\n");
	printf("that can be opened and edited with any text editor.\n");
}

static void
enable_console(void)
{
	AttachConsole(ATTACH_PARENT_PROCESS);
	g_stdoutH = GetStdHandle(STD_OUTPUT_HANDLE);
}

int
main(int argc, char *argv[])
{
	struct pw_pck pck = {0};
	int rc = -1;

	setlocale(LC_ALL, "en_US.UTF-8");
	enable_console();

	if (argc < 2) {
		print_help(argv);
		goto out;
	}

	int action = -1;
	const char *pck_path = NULL;
	const char *patch_path = NULL;

	char **a = argv;
	while (argc > 0) {
		if (argc >= 2 && (strcmp(*a, "-x") == 0 || strcmp(*a, "--extract") == 0)) {
			action = PW_PCK_ACTION_EXTRACT;
			pck_path = *(a + 1);
			a++;
			argc--;
		} else if (argc >= 2 && (strcmp(*a, "-u") == 0 || strcmp(*a, "--update") == 0)) {
			action = PW_PCK_ACTION_UPDATE;
			pck_path = *(a + 1);
			a++;
			argc--;
		} else if (argc >= 3 && (strcmp(*a, "-g") == 0 || strcmp(*a, "--gen-patch") == 0)) {
			action = PW_PCK_ACTION_GEN_PATCH;
			pck_path = *(a + 1);
			patch_path = *(a + 2);
			a += 2;
			argc -= 2;
		} else if (argc >= 3 && (strcmp(*a, "-a") == 0 || strcmp(*a, "--apply-patch") == 0)) {
			action = PW_PCK_ACTION_APPLY_PATCH;
			pck_path = *(a + 1);
			patch_path = *(a + 2);
			a += 2;
			argc -= 2;
		}

		a++;
		argc--;
	}

	if (action == -1) {
		print_help(argv);
		goto out;
	}

	rc = pw_pck_open(&pck, pck_path, action);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_pck_open(%s) failed: %d\n", pck_path, rc);
		goto out;
	}

	rc = 0;
out:
	FreeConsole();
	return rc;
}
