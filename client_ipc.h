/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_CLIENT_IPC_H
#define PW_CLIENT_IPC_H

#include <windows.h>

#define MG_PATCHER_STATUS_MSG (WM_USER + 166)

#define MG_GUI_ID_QUIT 1
#define MG_GUI_ID_PATCH 2
#define MG_GUI_ID_PLAY 3
#define MG_GUI_ID_REPAIR 4

enum mg_patcher_msg_command {
	MGP_MSG_SET_STATUS_LEFT = 1,
	MGP_MSG_SET_STATUS_RIGHT,
	MGP_MSG_ENABLE_BUTTON,
	MGP_MSG_DISABLE_BUTTON,
	MGP_MSG_SET_PROGRESS,
};

struct mg_patcher_msg_status {
	char *fmt;
	unsigned num_params;
};

struct mg_patcher_msg_status mg_patcher_msg_status_left[] = {
#define MGP_LMSG_FETCHING_CHANGES 0
	{ "Fetching changes ..." },
#define MGP_LMSG_DOWNLOADING_CHANGES 1
	{ "Downloading patches ..." },
#define MGP_LMSG_SAVING_FILES 2
	{ "Saving files ..." },
#define MGP_LMSG_LOADING_FILES 3
	{ "Loading files ..." },
#define MGP_LMSG_READY_TO_PLAY 4
	{ "Ready to play" },
};

struct mg_patcher_msg_status mg_patcher_msg_status_right[] = {
#define MGP_RMSG_CANT_FETCH 0
	{ "Can't fetch patch list" },
#define MGP_RMSG_CANT_PARSE 1
	{ "Can't parse patch list" },
#define MGP_RMSG_FAILED_LOADING 2
	{ "Failed to load local version" },
#define MGP_RMSG_ALLOC_FAILED 3
	{ "Failed to allocate memory" },
#define MGP_RMSG_ELEMENTS_PARSING_FAILED 4
	{ "Failed to parse elements file (%d). Please redownload the client" },
#define MGP_RMSG_TASKS_PARSING_FAILED 5
	{ "Failed to parse tasks file (%d). Please redownload the client" },
#define MGP_RMSG_TASK_NPC_PARSING_FAILED 6
	{ "Failed to parse task_npc file (%d). Please redownload the client" },
#define MGP_RMSG_PARSING_FAILED 7
	{ "Failed to parse data files (%d). Please redownload the client" },
#define MGP_RMSG_FETCH_FAILED 8
	{ "Failed to fetch server information" },
#define MGP_RMSG_DOWNLOADING_N_OF_N 9
	{ "Downloading patch %d of %d", 2 },
#define MGP_RMSG_PATCHING_DONE 10
	{ "Patching done" },
#define MGP_RMSG_SAVING_FAILED 11
	{ "Failed (%d:%d). No permission to access game directories?", 1 },
#define MGP_RMSG_ALREADY_UPTODATE 12
	{ "Already up to date", 0 },
#define MGP_RMSG_PATCHING_FAILED 13
	{ "Patching failed! (%d)", 1 },
#define MGP_RMSG_EMPTY 14
	{ "", 0 },
};

static inline unsigned
mg_patcher_msg_pack_params(unsigned type, unsigned char p1, unsigned char p2, unsigned char p3)
{
	unsigned ret;

	ret = type & 0xff;
	ret |= p1 << 8;
	ret |= p2 << 16;
	ret |= p3 << 24;

	return ret;
}

static inline int
mg_patcher_msg_get_status(enum mg_patcher_msg_command type, unsigned param, char *buf, unsigned buflen)
{
	struct mg_patcher_msg_status *status = NULL;

	switch (type) {
		case MGP_MSG_SET_STATUS_LEFT:
			status = &mg_patcher_msg_status_left[param & 0xff];
			break;
		case MGP_MSG_SET_STATUS_RIGHT:
			status = &mg_patcher_msg_status_right[param & 0xff];
			break;
		default:
			break;
	}

	if (!status) {
		return -1;
	}

	switch (status->num_params) {
		case 0:
			snprintf(buf, buflen, status->fmt);
			break;
		case 1:
			snprintf(buf, buflen, status->fmt, (param & 0xff00) >> 8);
			break;
		case 2:
			snprintf(buf, buflen, status->fmt, (param & 0xff00) >> 8, (param & 0xff0000) >> 16);
			break;
		case 3:
			snprintf(buf, buflen, status->fmt, (param & 0xff00) >> 8, (param & 0xff0000) >> 16, (param & 0xff000000) >> 24);
			break;
		default:
			return -1;
	}

	return 0;
}

#endif /* PW_CLIENT_IPC_H */
