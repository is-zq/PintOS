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
#include "filesys/inode.h"
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
static int syscall_open(const char* name);
static int syscall_filesize(int fd);
static int syscall_read(int fd,void* buffer,unsigned size);
static int syscall_write(int fd,const void* buffer,unsigned size);
static void syscall_seek(int fd,unsigned position);
static int syscall_tell(int fd);
static void syscall_close(int fd);
static double syscall_compute_e(int n);
static bool syscall_chdir(const char* dir);
static bool syscall_mkdir(const char* dir);
static bool syscall_readdir(int fd,char* name);
static bool syscall_isdir(int fd);
static int syscall_inumber(int fd);

static void validate_byte(const char* byte)
{
	if(byte == NULL || !is_user_vaddr(byte)
			|| pagedir_get_page(thread_current()->pcb->pagedir,byte) == NULL)
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

	case SYS_CHDIR:
		validate_args(args+1,1);
		f->eax = syscall_chdir((const char*)args[1]);
		break;

	case SYS_MKDIR:
		validate_args(args+1,1);
		f->eax = syscall_mkdir((const char*)args[1]);
		break;

	case SYS_READDIR:
		validate_args(args+1,2);
		f->eax = syscall_readdir((int)args[1],(char*)args[2]);
		break;

	case SYS_ISDIR:
		validate_args(args+1,1);
		f->eax = syscall_isdir((int)args[1]);
		break;

	case SYS_INUMBER:
		validate_args(args+1,1);
		f->eax = syscall_inumber((int)args[1]);
		break;

	default:
		break;
	}
}

static void syscall_exit(int status)
{
	struct thread* t = thread_current();
	if(t->pcb->ppcb != NULL)
		childList_get(&t->pcb->ppcb->child_list,t->tid)->exit_status = status;
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
	bool ret = filesys_create(file,initial_size,thread_current()->pcb->pwd);
	lock_release(&file_lock);
	return ret;
}

static bool syscall_remove(const char* file)
{
	lock_acquire(&file_lock);
	bool ret = filesys_remove(file,thread_current()->pcb->pwd);
	lock_release(&file_lock);
	return ret;
}

static int syscall_open(const char* name)
{
	struct process* pcb = thread_current()->pcb;
	lock_acquire(&file_lock);
	struct file* file = filesys_open(name,thread_current()->pcb->pwd);
	if(file == NULL)
	{
		lock_release(&file_lock);
		return -1;
	}
	for(int i=3;i<MAX_FD;i++)
	{
		if(pcb->fd_table[i] == NULL)
		{
			if(inode_isdir(file_get_inode(file)))
			{
				struct dir* dir = dir_open(inode_reopen(file_get_inode(file)));
				file_close(file);
				pcb->isdir_table[i] = true;
				pcb->fd_table[i] = dir;
			}
			else
			{
				pcb->isdir_table[i] = false;
				pcb->fd_table[i] = file;
			}
			lock_release(&file_lock);
			return i;
		}
	}
	/* Full */
	file_close(file);
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
		return input_getc();
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
		if(pcb->isdir_table[fd])
			return -1;
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
	if(pcb->isdir_table[fd])
		dir_close(pcb->fd_table[fd]);
	else
		file_close(pcb->fd_table[fd]);
	pcb->fd_table[fd] = NULL;
	lock_release(&file_lock);
}

static double syscall_compute_e(int n)
{
	return sys_sum_to_e(n);
}

static bool syscall_chdir(const char* dir)
{
	lock_acquire(&file_lock);
	bool ret = filesys_chdir(dir,&thread_current()->pcb->pwd);
	lock_release(&file_lock);
	return ret;
}

static bool syscall_mkdir(const char* dir)
{
	lock_acquire(&file_lock);
	bool ret = filesys_mkdir(dir,thread_current()->pcb->pwd);
	lock_release(&file_lock);
	return ret;
}

static bool syscall_readdir(int fd,char* name)
{
	struct process* pcb = thread_current()->pcb;
	if(!pcb->isdir_table[fd])
		return false;
	lock_acquire(&file_lock);
	bool ret = dir_readdir((struct dir*)pcb->fd_table[fd],name);
	lock_release(&file_lock);
	return ret;
}

static bool syscall_isdir(int fd)
{
	struct process* pcb = thread_current()->pcb;
	return pcb->isdir_table[fd];
}

static int syscall_inumber(int fd)
{
	int ret = 0;
	struct process* pcb = thread_current()->pcb;

	lock_acquire(&file_lock);
	if(pcb->isdir_table[fd])
	{
		struct dir* dir = (struct dir*)pcb->fd_table[fd];
		ret = inode_get_inumber(dir_get_inode(dir));
	}
	else
	{
		struct file* file = (struct file*)pcb->fd_table[fd];
		ret = inode_get_inumber(file_get_inode(file));
	}
	lock_release(&file_lock);

	return ret;
}
