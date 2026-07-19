// Include libraries
#include "job.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define jobs table
#define MAX_JOBS 64
static Job jobs[MAX_JOBS];
static int next_job_id = 1;

// PID to PGID mapping table (for tracking pgid after process exits)
#define MAX_PID_MAP 256
typedef struct {
  pid_t pid;
  pid_t pgid;
} PidPgidMap;
static PidPgidMap pid_pgid_map[MAX_PID_MAP];

void jobs_init(void)
{
  memset(jobs, 0, sizeof(jobs));
  memset(pid_pgid_map, 0, sizeof(pid_pgid_map));
  next_job_id = 1;
}

void pid_pgid_map_add(pid_t pid, pid_t pgid)
{
  for (int i = 0; i < MAX_PID_MAP; i++) {
    if (pid_pgid_map[i].pid == 0) {
      pid_pgid_map[i].pid = pid;
      pid_pgid_map[i].pgid = pgid;
      return;
    }
  }
}

pid_t pid_pgid_map_get(pid_t pid)
{
  for (int i = 0; i < MAX_PID_MAP; i++) {
    if (pid_pgid_map[i].pid == pid) {
      return pid_pgid_map[i].pgid;
    }
  }
  return -1;
}

void pid_pgid_map_remove(pid_t pid)
{
  for (int i = 0; i < MAX_PID_MAP; i++) {
    if (pid_pgid_map[i].pid == pid) {
      pid_pgid_map[i].pid = 0;
      pid_pgid_map[i].pgid = 0;
      return;
    }
  }
}

int jobs_add(pid_t pgid, const char *command, bool background)
{
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == 0) {
      jobs[i].job_id = next_job_id++;
      jobs[i].pgid = pgid;
      jobs[i].state = JOB_RUNNING;
      jobs[i].background = background;
      jobs[i].command = malloc(strlen(command)+1);

      if (jobs[i].command == NULL){
        return -1;
      }
      strcpy(jobs[i].command, command);

      return jobs[i].job_id;
    }
  }
  return -1;
}

void jobs_remove(pid_t pgid) {
  Job *job = jobs_find_by_pgid(pgid);
  if (job){
    free(job->command);
    memset(job, 0, sizeof(Job));
  }
}   

Job *jobs_find_by_pgid(pid_t pgid)
{
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].pgid == pgid){
      return &jobs[i];
    }
  }
  return NULL;
}

Job *jobs_find_by_id(int id)
{
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == id) {
      return &jobs[i];
    }
  }
  return NULL;
}

void jobs_update_state(pid_t pgid, job_state state)
{
  Job *job = jobs_find_by_pgid(pgid);
  if (job){
    job->state = state;
  }
}

void jobs_print(void)
{
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == 0) {
      continue;
    }
    printf("[%d] %d ", jobs[i].job_id, jobs[i].pgid);
    
    switch (jobs[i].state) {
      case JOB_RUNNING:
        printf("Running");
        break;
      
      case JOB_STOPPED:
        printf("Stopped");
        break;

      case JOB_DONE:
        printf("Done");

        break;
    }
    printf(" %s\n", jobs[i].command);
    if (jobs[i].state == JOB_DONE) {
      jobs_remove(jobs[i].pgid);
    }
  }
}