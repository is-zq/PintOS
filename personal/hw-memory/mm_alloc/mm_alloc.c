/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define true 1
#define false 0

typedef struct MetaData
{
	size_t size;
	int free;
	struct MetaData* next;
	struct MetaData* prev;
}MetaData,*MemList;

MemList mem_list = NULL;

/* Insert MetaData Node of size SIZE after P */
MetaData* metadata_insert(MetaData* p,size_t size,int free)
{
	char* q = (char*)p;
	q += sizeof(MetaData) + p->size;
	MetaData* s = (MetaData*)q;
	s->next = p->next;
	s->prev = p;
	p->next = s;
	if(s->next != NULL)
		s->next->prev = s;
	
	s->size = size;
	s->free = free;
	return s;
}

void* mm_malloc(size_t size) {
	if(size == 0)
		return NULL;

	if(mem_list == NULL)	//Empty memory list
	{
		void* ret_p = sbrk(sizeof(MetaData) + size);
		if(ret_p == (void*)-1)
			return NULL;
		mem_list = (MemList)ret_p;
		mem_list->next = NULL;
		mem_list->prev = NULL;
		mem_list->size = size;
		mem_list->free = false;
		memset(mem_list+1,0,size);
		return mem_list + 1;
	}

	MetaData* p = mem_list;
	MetaData* tail = p;
	while(p != NULL)
	{
		if(p->free && p->size >= size)
		{
			p->free = false;
			if(p->size - size > sizeof(MetaData))	//Sufficient remaining space
			{
				size_t remain_size = p->size - size - sizeof(MetaData);
				p->size = size;
				metadata_insert(p,remain_size,true);
			}
			memset(p+1,0,p->size);
			return p + 1;
		}
		else
		{
			tail = p;
			p = p->next;
		}
	}

	/* No sufficient block */
	if(sbrk(sizeof(MetaData) + size) == (void*)-1)
		return NULL;
	MetaData* ret_p = metadata_insert(tail,size,false);
	memset(ret_p+1,0,size);
	return ret_p + 1;
}

void* mm_realloc(void* ptr, size_t size) {
	if(ptr != NULL && size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	else if(ptr == NULL)
		return mm_malloc(size);

	void* new_p = mm_malloc(size);
	MetaData* metadata = (MetaData*)ptr;
	metadata--;
	memcpy(new_p,ptr,metadata->size);
	mm_free(ptr);

	return new_p;
}

void mm_free(void* ptr) {
	if(mem_list == NULL || ptr == NULL)
		return;

	MetaData* p = mem_list;
	while(p != NULL)
	{
		if(p+1 == ptr)
		{
			p->free = true;
			MetaData* pre_p = p->prev;
			MetaData* next_p = p->next;
			if(next_p != NULL && next_p->free == true)
			{
				p->size += sizeof(MetaData) + next_p->size;
				p->next = next_p->next;
				if(next_p->next != NULL)
					next_p->next->prev = p;
			}
			if(pre_p != NULL && pre_p->free == true)
			{
				pre_p->size += sizeof(MetaData) + p->size;
				pre_p->next = p->next;
				if(p->next != NULL)
					p->next->prev = pre_p;
			}
			break;
		}
		else
			p = p->next;
	}
}
