#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void syscall_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void validate_buffer_in_user_region(const void* buffer, size_t length) {
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr(buffer) || length > delta)
    syscall_exit(-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void validate_string_in_user_region(const char* string) {
  uintptr_t delta = PHYS_BASE - (const void*)string;
  if (!is_user_vaddr(string) || strnlen(string, delta) == delta)
    syscall_exit(-1);
}

static int syscall_open(const char* filename) {
  struct thread* t = thread_current();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open(filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int syscall_write(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  } else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_write(t->open_file, buffer, size);
}

static int syscall_read(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_read(t->open_file, buffer, size);
}

static void syscall_close(int fd) {
  struct thread* t = thread_current();
  if (fd == 2 && t->open_file != NULL) {
    file_close(t->open_file);
    t->open_file = NULL;
  }
}

static bool install_page(void* upage, void* kpage, bool writable) {
  struct thread* t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL &&
          pagedir_set_page(t->pagedir, upage, kpage, writable));
}

static void malloc_free_multiple(uint8_t* start_bound,uint32_t pg_num)
{
	ASSERT(is_user_vaddr(start_bound));
	ASSERT(pg_ofs(start_bound) == 0);
	struct thread* t = thread_current();
	uint8_t* bound_p = start_bound;
	for(uint32_t i=0;i<pg_num;i++)
	{	
		void* kpage = pagedir_get_page(t->pagedir,bound_p);
		pagedir_clear_page(t->pagedir,bound_p);
		palloc_free_page(kpage);
		bound_p += PGSIZE;
	}
}

static bool malloc_get_multiple(uint8_t* start_bound,uint32_t pg_num)
{
	ASSERT(is_user_vaddr(start_bound));
	ASSERT(pg_ofs(start_bound) == 0);
	uint8_t* bound_p = start_bound;
	bool success = false;
	for(uint32_t i=0;i<pg_num;i++)
	{
		uint8_t* kpage = palloc_get_page(PAL_USER | PAL_ZERO);
		if(kpage != NULL)
		{
			success = install_page(bound_p, kpage, true);
			if(success)
				bound_p += PGSIZE;
			else
			{
				malloc_free_multiple(start_bound,i);
				palloc_free_page(kpage);
				return false;
			}
		}
		else
		{
			malloc_free_multiple(start_bound,i);
			return false;
		}
	}
	return success;
}

static void* syscall_sbrk(intptr_t increment)
{
	struct thread* t = thread_current();
	void* ret = t->seg_break;
	uint8_t* new_break = t->seg_break + increment;
	if(new_break < t->heap_start || new_break > PHYS_BASE)
		return (void*)-1;
	
	uint8_t* up_bound = (uint8_t*)pg_round_up((void*)(t->seg_break));
	uint8_t* low_bound = (uint8_t*)pg_round_down((void*)(t->seg_break));
	if(new_break > up_bound)
	{
		uint32_t pg_num = pg_no((uint8_t*)pg_round_up((void*)new_break) - up_bound);
		bool success = malloc_get_multiple(up_bound,pg_num);
		if(!success)
			return (void*)-1;
	}
	else if(new_break <= low_bound)
	{
		uint32_t pg_num = pg_no(low_bound - (uint8_t*)pg_round_down((void*)new_break));
		if(pg_ofs(new_break) == 0)
			pg_num++;
		malloc_free_multiple(pg_round_up((void*)new_break),pg_num);
	}

	t->seg_break = new_break;
	return ret;
}

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = (uint32_t*)f->esp;
  struct thread* t = thread_current();
  t->in_syscall = true;

  validate_buffer_in_user_region(args, sizeof(uint32_t));
  switch (args[0]) {
    case SYS_EXIT:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_exit((int)args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      validate_string_in_user_region((char*)args[1]);
      f->eax = (uint32_t)syscall_open((char*)args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_read((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_close((int)args[1]);
      break;

	case SYS_SBRK:
	  validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
	  f->eax = (void*)syscall_sbrk((intptr_t)args[1]);
	  break;

    default:
      printf("Unimplemented system call: %d\n", (int)args[0]);
      break;
  }

  t->in_syscall = false;
}
