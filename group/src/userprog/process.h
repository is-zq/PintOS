#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/directory.h"
#include <stdint.h>

// At most 8MB can be allocated to the stack
// These defines will be used in Project 2: Multithreading
#define MAX_STACK_PAGES (1 << 11)
#define MAX_THREADS 127
#define MAX_FD (128+3)

/* PIDs and TIDs are the same type. PID should be
   the TID of the main thread of the process */
typedef tid_t pid_t;

/* Thread functions (Project 2: Multithreading) */
typedef void (*pthread_fun)(void*);
typedef void (*stub_fun)(pthread_fun, void*);

/* Node used to record process being loaded */
typedef struct LoadNode
{
	pid_t pid;
	bool loaded;
	struct process* pcb;
	struct semaphore k_sema;	/* Blocking kernel thread to ensure successful loading */
	struct semaphore ch_sema;/* Block child process until kernel thread have completed processing */
	struct list_elem elem;
}LoadNode;

/* Node used to record child processes */
typedef struct ChildNode
{
	pid_t pid;
	int exit_status;
	struct process* pcb;
	struct semaphore sema;	/* Block parent process which called wait() */
	struct list_elem elem;
}ChildNode;

/* The process control block for a given process. Since
   there can be multiple threads per process, we need a separate
   PCB from the TCB. All TCBs in a process will have a pointer
   to the PCB, and the PCB will have a pointer to the main thread
   of the process, which is `special`. */
struct process {
  /* Owned by process.c. */
  uint32_t* pagedir;			/* Page directory. */
  struct dir* pwd;				/* Work directory */
  char process_name[16];		/* Name of the main thread */
  struct thread* main_thread;	/* Pointer to main thread */
  struct file* exec_file;		/* Executable file */
  void* fd_table[MAX_FD];		/* File despcriptor table */
  bool isdir_table[MAX_FD];		/* Is fd a directory */
  struct process* ppcb;			/* Parent PCB */
  struct list child_list;		/* List for child processes */
};

void userprog_init(void);
ChildNode* childList_get(struct list* child_list,pid_t pid);
pid_t process_execute(const char* file_name);
int process_wait(pid_t);
void process_exit(void);
void process_activate(void);

bool is_main_thread(struct thread*, struct process*);
pid_t get_pid(struct process*);

tid_t pthread_execute(stub_fun, pthread_fun, void*);
tid_t pthread_join(tid_t);
void pthread_exit(void);
void pthread_exit_main(void);

#endif /* userprog/process.h */
