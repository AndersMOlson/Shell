#ifndef JOB_H
#define JOB_H

#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>

typedef enum {
  JOB_RUNNING,
  JOB_STOPPED,
  JOB_DONE
} job_state;

typedef struct {
  int job_id;
  pid_t pgid;
  job_state state;
  char *command;
  bool background;
} Job;

void jobs_init(void);
int jobs_add(pid_t pgid, const char *command, bool background);
void jobs_remove(pid_t pgid);
Job *jobs_find_by_pgid(pid_t pgid);
Job *jobs_find_by_id(int id);
void jobs_update_state(pid_t pgid, job_state state);
void jobs_print(void);

// PID to PGID mapping functions (for tracking process groups after process exits)
void pid_pgid_map_add(pid_t pid, pid_t pgid);
pid_t pid_pgid_map_get(pid_t pid);
void pid_pgid_map_remove(pid_t pid);

#endif