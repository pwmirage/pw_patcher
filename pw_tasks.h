/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_TASKS_H
#define PW_TASKS_H

struct pw_task_file {
	uint32_t magic;
	uint32_t version;
	struct pw_idmap *idmap;
	struct pw_chain_table *tasks;
	unsigned max_arr_idx;
	unsigned max_dialogue_id;
};

int pw_tasks_load(struct pw_task_file *taskf, const char *path, const char *idmap_path);
int pw_tasks_serialize(struct pw_task_file *taskf, const char *filename);
int pw_tasks_patch_obj(struct pw_task_file *taskf, struct cjson *obj);
int pw_tasks_save(struct pw_task_file *taskf, const char *path, bool is_server);
int pw_tasks_idmap_save(struct pw_task_file *taskf, const char *filename);
void pw_tasks_adjust_rates(struct pw_task_file *taskf, struct cjson *rates);

#endif /* PW_TASKS_H */
