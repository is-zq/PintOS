#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "lib/float.h"

struct lock file_lock;

static void syscall_handler(struct intr_frame*);
static void syscall_exit(int status);
static int syscall_practice(int i);
static void syscall_halt(void);
static pid_t syscall_exec(const char *cmd_line);
static int syscall_wait(pid_t pid);
static bool syscall_create(const char* file,unsigned initial_size);
static bool syscall_remove(const char* file);
static int syscall_open(const char* file);
static int syscall_filesize(int fd);
static int syscall_read(int fd,void* buffer,unsigned size);
static int syscall_write(int fd,const void* buffer,unsigned size);
static void syscall_seek(int fd,unsigned position);
static int syscall_tell(int fd);
static void syscall_close(int fd);
static double syscall_compute_e(int n);
static tid_t sys_pthread_create(stub_fun sfun,pthread_fun tfun,const void* arg);
static void sys_pthread_exit(void);
static tid_t sys_pthread_join(tid_t tid);
static bool sys_lock_init(lock_t* lock);
static bool sys_lock_acquire(lock_t* lock);
static bool sys_lock_release(lock_t* lock);
static bool sys_sema_init(sema_t* sema,int val);
static bool sys_sema_down(sema_t* sema);
static bool sys_sema_up(sema_t* sema);
static tid_t sys_get_tid(void);

static bool valid_addr(const char* addr)
{
	return !(addr == NULL || !is_user_vaddr(addr)
				|| pagedir_get_page(thread_current()->pcb->pagedir,addr) == NULL);
}

static void validate_byte(const char* byte)
{
	if(!valid_addr(byte))
	{
		syscall_exit(-1);
	}
}

static void validate_string(char* string)
{
	char* ch = string;
	while(true)
	{
		validate_byte(ch);
		if(*ch == '\0')
			break;
		++ch;
	}
}

static void validate_buffer(const void* buffer,size_t len)
{
	char* addr_b = (char*)buffer;
	for(size_t i=0;i<len;i++)
		validate_byte(addr_b++);
}

static void validate_args(const uint32_t* argv,int count)
{
	validate_buffer(argv,count * sizeof(uint32_t));
}

void syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
	lock_init(&file_lock);
}

static void syscall_handler(struct intr_frame* f) 
{
	uint32_t* args = ((uint32_t*)f->esp);

	/*
	* The following print statement, if uncommented, will print out the syscall
	* number whenever a process enters a system call. You might find it useful
	* when debugging. It will cause tests to fail, however, so you should not
	* include it in your final submission.
	*/

	/* printf("System call number: %d\n", args[0]); */
	validate_args(args,1);

	switch(args[0])
	{
	case SYS_EXIT:
		validate_args(args+1, 1);
		f->eax = (int)args[1];
		syscall_exit((int)args[1]);
		break;

	case SYS_PRACTICE:
		validate_args(args+1, 1);
		f->eax = syscall_practice((int)args[1]);	
		break;

	case SYS_HALT:
		syscall_halt();
		break;

	case SYS_EXEC:
		validate_args(args+1, 1);
		validate_string((char*)args[1]);
		f->eax = syscall_exec((char*)args[1]);
		break;

	case SYS_WAIT:
		validate_args(args+1, 1);
		f->eax = syscall_wait((pid_t)args[1]);
		break;

	case SYS_CREATE:
		validate_args(args+1,2);
		validate_string((char*)args[1]);
		f->eax = syscall_create((char*)args[1],(unsigned)args[2]);
		break;

	case SYS_REMOVE:
		validate_args(args+1,1);
		validate_string((char*)args[1]);
		f->eax = syscall_remove((char*)args[1]);
		break;

	case SYS_OPEN:
		validate_args(args+1,1);
		validate_string((char*)args[1]);
		f->eax = syscall_open((char*)args[1]);
		break;

	case SYS_FILESIZE:
		validate_args(args+1,1);
		f->eax = syscall_filesize((int)args[1]);
		break;

	case SYS_READ: 
		validate_args(args+1,3);
		validate_buffer((void*)args[2],(size_t)args[3]);
		f->eax = syscall_read((int)args[1],(void*)args[2],(unsigned)args[3]);
		break;

	case SYS_WRITE:
		validate_args(args+1,3);
		validate_buffer((void*)args[2],(size_t)args[3]);
		f->eax = syscall_write((int)args[1],(void*)args[2],(unsigned)args[3]);
		break;

	case SYS_SEEK:
		validate_args(args+1,2);
		syscall_seek((int)args[1],(unsigned)args[2]);
		break;

	case SYS_TELL:
		validate_args(args+1,1);
		f->eax = syscall_tell((int)args[1]);
		break;

	case SYS_CLOSE:
		validate_args(args+1,1);
		syscall_close((int)args[1]);
		break;

	case SYS_COMPUTE_E:
		validate_args(args+1,1);
		f->eax = syscall_compute_e((int)args[1]);
		break;

	case SYS_PT_CREATE:
		validate_args(args+1,3);
		f->eax = sys_pthread_create((stub_fun)args[1],(pthread_fun)args[2],(const void*)args[3]);
		break;

	case SYS_PT_EXIT:
		sys_pthread_exit();
		break;

	case SYS_PT_JOIN:
		validate_args(args+1,1);
		f->eax = sys_pthread_join((tid_t)args[1]);
		break;

	case SYS_LOCK_INIT:
		validate_args(args+1,1);
		f->eax = sys_lock_init((lock_t*)args[1]);
		break;

	case SYS_LOCK_ACQUIRE:
		validate_args(args+1,1);
		f->eax = sys_lock_acquire((lock_t*)args[1]);
		break;

	case SYS_LOCK_RELEASE:
		validate_args(args+1,1);
		f->eax = sys_lock_release((lock_t*)args[1]);
		break;

	case SYS_SEMA_INIT:
		validate_args(args+1,2);
		f->eax = sys_sema_init((sema_t*)args[1],(int)args[2]);
		break;

	case SYS_SEMA_DOWN:
		validate_args(args+1,1);
		f->eax = sys_sema_down((sema_t*)args[1]);
		break;

	case SYS_SEMA_UP:
		validate_args(args+1,1);
		f->eax = sys_sema_up((sema_t*)args[1]);
		break;

	case SYS_GET_TID:
		f->eax = sys_get_tid();
		break;

	default:
		break;
	}
}

static void syscall_exit(int status)
{
	struct thread* t = thread_current();
	if(t->pcb->ppcb != NULL)
		childList_get(&t->pcb->ppcb->child_list,t->pcb->main_thread->tid)->exit_status = status;
	printf("%s: exit(%d)\n", thread_current()->pcb->process_name, status);
	process_exit();
}

static int syscall_practice(int i)
{
	return i+1;
}

static void syscall_halt(void)
{
	shutdown_power_off();
}

static pid_t syscall_exec(const char *cmd_line)
{
	return process_execute(cmd_line);
}

static int syscall_wait(pid_t pid)
{
	return process_wait(pid);
}

static bool syscall_create(const char* file,unsigned initial_size)
{
	lock_acquire(&file_lock);
	bool ret = filesys_create(file,initial_size);
	lock_release(&file_lock);
	return ret;
}

static bool syscall_remove(const char* file)
{
	lock_acquire(&file_lock);
	bool ret = filesys_remove(file);
	lock_release(&file_lock);
	return ret;
}

static int syscall_open(const char* file)
{
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	struct file* new_file = filesys_open(file);
	if(new_file == NULL)
	{
		lock_release(&file_lock);
		return -1;
	}
	for(int i=3;i<MAX_FD;i++)
	{
		if(pcb->fd_table[i] == NULL)
		{
			pcb->fd_table[i] = new_file;
			lock_release(&file_lock);
			return i;
		}
	}
	/* Full */
	file_close(new_file);
	lock_release(&file_lock);
	return -1;
}

static int syscall_filesize(int fd)
{
	if(fd < 0 || fd >= MAX_FD)
		return -1;
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	if(pcb->fd_table[fd] == NULL)
	{
		lock_release(&file_lock);
		return -1;
	}
	int ret = file_length(pcb->fd_table[fd]);
	lock_release(&file_lock);
	return ret;
}

static int syscall_read(int fd,void* buffer,unsigned size)
{
	if(fd < 0 || fd >= MAX_FD)
		return -1;
	if(fd == STDIN_FILENO)
	{
		char* p = (char*)buffer;
		while(size--)
			*p++ = input_getc();
		return size;
	}
	else
	{
		struct process* pcb = thread_current()->pcb;
		lock_acquire(&file_lock);
		if(pcb->fd_table[fd] == NULL)
		{
			lock_release(&file_lock);
			return -1;
		}
		int ret = file_read(pcb->fd_table[fd],buffer,size);
		lock_release(&file_lock);
		return ret;
	}
}

static int syscall_write(int fd,const void *buffer,unsigned size)
{
	if(fd < 0 || fd >= MAX_FD)
		return -1;
	if(fd == STDOUT_FILENO)
	{
		putbuf(buffer,size);
		return size;
	}
	else
	{
		struct process* pcb = thread_current()->pcb;
		lock_acquire(&file_lock);
		if(pcb->fd_table[fd] == NULL)
		{
			lock_release(&file_lock);
			return -1;
		}
		int ret = file_write(pcb->fd_table[fd],buffer,size);
		lock_release(&file_lock);
		return ret;
	}
}

static void syscall_seek(int fd,unsigned position)
{
	if(fd < 0 || fd >= MAX_FD)
		return;
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	if(pcb->fd_table[fd] == NULL)
	{
		lock_release(&file_lock);
		return;
	}
	file_seek(pcb->fd_table[fd],position);
	lock_release(&file_lock);
}

static int syscall_tell(int fd)
{
	if(fd < 0 || fd >= MAX_FD)
		return -1;
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	if(pcb->fd_table[fd] == NULL)
	{
		lock_release(&file_lock);
		return -1;
	}
	int ret = file_tell(pcb->fd_table[fd]);
	lock_release(&file_lock);
	return ret;
}

static void syscall_close(int fd)
{
	if(fd < 0 || fd >= MAX_FD)
		return;
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	if(pcb->fd_table[fd] == NULL)
	{
		lock_release(&file_lock);
		return;
	}
	file_close(pcb->fd_table[fd]);
	pcb->fd_table[fd] = NULL;
	lock_release(&file_lock);
}

static double syscall_compute_e(int n)
{
	return sys_sum_to_e(n);
}

static tid_t sys_pthread_create(stub_fun sfun,pthread_fun tfun,const void* arg)
{
	return pthread_execute(sfun,tfun,arg);
}

static void sys_pthread_exit(void)
{
	pthread_exit();
}

static tid_t sys_pthread_join(tid_t tid)
{
	return pthread_join(tid);
}

static bool sys_lock_init(lock_t* lock)
{
	if(!valid_addr(lock))
		return false;
	lock_t n_lock = new_lock();
	if(n_lock == -128)
		return false;
	*lock = n_lock;
	return true;
}

static bool sys_lock_acquire(lock_t* lock)
{
	if(!valid_addr(lock))
		return false;
	struct lock* ls = get_lock(*lock);
	if(ls == NULL || lock_held_by_current_thread(ls))
		return false;
	
	lock_acquire(ls);
	return true;
}

static bool sys_lock_release(lock_t* lock)
{
	if(!valid_addr(lock))
		return false;
	struct lock* ls = get_lock(*lock);
	if(ls == NULL || lock_held_by_current_thread(ls))
		return false;
	
	lock_release(ls);
	return true;
}

static bool sys_sema_init(sema_t* sema,int val)
{
	if(!valid_addr(sema) || val < 0)
		return false;
	sema_t n_sema = new_sema(val);
	if(n_sema == -128)
		return false;
	*sema = n_sema;
	return true;
}

static bool sys_sema_down(sema_t* sema)
{
	if(!valid_addr(sema))
		return false;
	struct semaphore* ss = get_sema(*sema);
	if(ss == NULL)
		return false;
	
	sema_down(ss);
	return true;
}

static bool sys_sema_up(sema_t* sema)
{
	if(!valid_addr(sema))
		return false;
	struct semaphore* ss = get_sema(*sema);
	if(ss == NULL)
		return false;
	
	sema_up(ss);
	return true;
}

static tid_t sys_get_tid(void)
{
	return thread_tid();
}
