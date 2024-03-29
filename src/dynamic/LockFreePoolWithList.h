#pragma once

#include <omp.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "common.h"
#include <cassert>
#include <cstring>
#include <sys/mman.h>

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)

template <u64 MAX_THREADS = 64, u64 MAX_SEGMENT_BITS = 32, u64 BLOCK_SIZE = (1UL << 22)>
class LockFreePoolWithList {
	alignas(64) void* __restrict nextFreePtrs[MAX_THREADS][MAX_SEGMENT_BITS];

public:

#ifdef CALC_MEM_PER_EDGE
	u64 totMem = 0;
#endif

	LockFreePoolWithList(){
		memset(nextFreePtrs, 0, MAX_THREADS * MAX_SEGMENT_BITS * sizeof(void*));
	}


	void* allocLog2(u64 log2size){
#ifdef _OPENMP
		void* __restrict * nextPtr = &nextFreePtrs[omp_get_thread_num()][log2size];
#else
		void* __restrict * nextPtr = &nextFreePtrs[0][log2size];
#endif
		if(__builtin_expect(*nextPtr == nullptr, 0)){
			const u64 segSize = 1UL << log2size;
			const u64 blockSize = (segSize <= BLOCK_SIZE) ? BLOCK_SIZE : segSize;

#ifdef USE_HUGEPAGE
			void* blockStart = mmap(NULL, blockSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB |
					MAP_HUGE_2MB, -1, 0);
#else
			void* blockStart = aligned_alloc(64, blockSize);
#endif

#ifdef CALC_MEM_PER_EDGE
			#pragma omp atomic
			totMem += blockSize;
#endif

			*nextPtr = blockStart;
			const u64 iter = blockSize >> log2size;
			for(u64 i = 0; i < (iter - 1); i++){
				void* nextBlock = (u8*)blockStart + segSize;
				*(void**)blockStart = nextBlock;
				blockStart = nextBlock;
			}
			*(void**)blockStart = nullptr;
		}
		void* ret = *nextPtr;
		*nextPtr = *(void**)ret;
		return ret;
	}

	void* allocPow2(u64 size){
		return allocLog2(getPow2Log2(size));
	}

	void* allocate(u64 size){
		return allocLog2(getNextPow2Log2(size));
	}

	void freeLog2(void* ptr, u64 log2size){
#ifdef _OPENMP
		void* __restrict * nextPtr = &nextFreePtrs[omp_get_thread_num()][log2size];
#else
		void* __restrict * nextPtr = &nextFreePtrs[0][log2size];
#endif
		*(void**)ptr = *nextPtr;
		*nextPtr = ptr;
	}

	void freePow2(void* __restrict ptr, u64 size){
		freeLog2(ptr, getPow2Log2(size));
	}

	void deallocate(void* __restrict ptr, u64 size){
		freeLog2(ptr, getNextPow2Log2(size));
	}
};

