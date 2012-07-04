// ID_MM: The Memory Manager
// The Memory Manager provides a system for allocating and deallocating
// memory. Memory blocks can be moved and removed when not in use to free
// up memory needed.
//
// Memory blocks are allocated returned double-indirect pointers, and these
// pointers are updated when a block's address changes.
//

#include "id_mm.h"
#include "id_us.h"

#include <stdlib.h>

#define MM_MAXBLOCKS	2048 	//1200 in Keen5

typedef struct ID_MM_MemBlock
{
	void *ptr;
	int length;
	mm_ptr_t *userptr;
	bool locked;
	int purgelevel;
	struct ID_MM_MemBlock *next;
} ID_MM_MemBlock;

static ID_MM_MemBlock mm_blocks[MM_MAXBLOCKS];

static ID_MM_MemBlock *mm_free, *mm_purgeable;

static int mm_blocksused;
static int mm_numpurgeable;
static int mm_memused;


static void MML_ClearBlock()
{
	ID_MM_MemBlock *bestBlock = 0;
	for (int i = 0; i < MM_MAXBLOCKS; ++i)
	{
		//Is the block free?
		if (!mm_blocks[i].ptr) continue;
		//Can we purge it?
		if (!mm_blocks[i].purgelevel) continue;
		//Is it locked?
		if (mm_blocks[i].locked) continue;

		if (!bestBlock || mm_blocks[i].purgelevel > bestBlock->purgelevel)
		{
			bestBlock = &(mm_blocks[i]);
			continue;
		}
	}

	//Did we find a purgable block?
	if (!bestBlock)
		Quit("MML_ClearBlock(): No purgable blocks!");

	//Free the sucker.
	MM_FreePtr(bestBlock->userptr);
}

static ID_MM_MemBlock *MML_GetNewBlock()
{
	ID_MM_MemBlock *newBlock;
	//If there aren't any free blocks, kick out some purgables.
	if (!mm_free)
		MML_ClearBlock();
	newBlock = mm_free;
	mm_free = mm_free->next;
	return newBlock;
}

static void MML_UpdateUserPointer(ID_MM_MemBlock *blk)
{
	(*blk->userptr) = blk->ptr;
}

static ID_MM_MemBlock *MML_BlockFromUserPointer(mm_ptr_t *ptr)
{
	for (int i =0; i < MM_MAXBLOCKS; ++i)
		if (mm_blocks[i].userptr == ptr)
			return &(mm_blocks[i]);

	return (ID_MM_MemBlock*)(0);
}

void MM_Startup(void)
{
	//NOP
	for (int i = MM_MAXBLOCKS-1; i >= 0; --i)
	{
		mm_blocks[i].ptr = 0;
		mm_blocks[i].next = (i==MM_MAXBLOCKS-1)?0:&(mm_blocks[i+1]);
	}
	mm_free = &(mm_blocks[0]);
	mm_purgeable = 0;
}

void MM_Shutdown(void)
{
	for (int i = 0; i < MM_MAXBLOCKS; ++i)
	{
		if (mm_blocks[i].ptr)	
			free(mm_blocks[i].ptr);
	}
}

void MM_GetPtr(mm_ptr_t *ptr, unsigned long size)
{
	ID_MM_MemBlock *blk = MML_GetNewBlock();
	//Try to allocate memory, freeing if we can't.
	do {
		blk->ptr = malloc(size);
		if (!blk->ptr)
		{
			if (mm_numpurgeable)
				MML_ClearBlock();
			else
				Quit("MM_GetPtr: Out of Memory!");
		}
	} while(!blk->ptr);

	//Setup the block details (unlocked, non-purgable)
	blk->length = size;
	blk->userptr = ptr;
	blk->purgelevel = 0;
	blk->locked = false;

	//Update the stats
	mm_blocksused++;
	mm_memused += size;

	MML_UpdateUserPointer(blk);
}

void MM_FreePtr(mm_ptr_t *ptr)
{
	//Lookup the block
	ID_MM_MemBlock *blk = MML_BlockFromUserPointer(ptr);
	if (!blk)
		Quit("MM_FreePtr: Block not Found!");

	//Update the purgeable count.
	if (blk->purgelevel)
		mm_numpurgeable--;

	//Update the used memory counts.
	mm_blocksused--;
	mm_memused -= blk->length;

	//Add it to the free list
	blk->next = mm_free;
	mm_free = blk;

	//Free its memory.
	free(blk->ptr);
	blk->length = 0;
	blk->ptr = 0;
	blk->userptr = 0;
}

void MM_SetPurge(mm_ptr_t *ptr, int level)
{
	//Lookup the block
	ID_MM_MemBlock *blk = MML_BlockFromUserPointer(ptr);
	if (!blk)
		Quit("MM_SetPurge: Block not Found!");

	//Keep track of the purgeable block count.
	if (!blk->purgelevel && level)
		mm_numpurgeable++;
	else if (blk->purgelevel && !level)
		mm_numpurgeable--;

	//Set the purge level
	blk->purgelevel = level;
}

void MM_SetLock(mm_ptr_t *ptr, bool lock)
{
	//Lookup the block
	ID_MM_MemBlock *blk = MML_BlockFromUserPointer(ptr);
	if (!blk)
		Quit("MM_SetLock: Block not Found!");

	//Lock/Unlock the block
	blk->locked = lock;
}

int MM_UsedMemory()
{
	return mm_memused;
}

int MM_UsedBlocks()
{
	return mm_blocksused;
}

int MM_PurgableBlocks()
{
	return mm_numpurgeable;
}

void MM_SortMem()
{
	//We're not actually sorting memory, as we just handball
	//allocation over to the system at the moment.
	//All we'll do is purge _all_ purgable blocks.

	//NOTE: Keen locks the currently playing music at this point.
	//We will ignore this, as no sound is implemented.
	for (int i = 0; i < MM_MAXBLOCKS; ++i)
	{
		if (mm_blocks[i].purgelevel && !mm_blocks[i].locked)
			MM_FreePtr(mm_blocks[i].userptr);
	}
}

void MM_ShowMemory()
{
	//TODO: This is a stub. I should at least add a stats dump here.
}

void MM_BombOnError(bool bomb)
{
	//TODO: Add support for this here.
}

//NOTE: Keen/Wolf3d have MML_UseSpace. This is incompatible with our use of the
//system allocator, so it is unused for now.