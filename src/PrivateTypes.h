/*
 * Copyright (c) 2017 Subramaniyan Neelagandan
 * All Rights Reserved.
 *
 * All information contained herein is, and remains the property of
 * Subramaniyan Neelagandan. The intellectual and technical concepts
 * contained herein are proprietary to Subramaniyan Neelagandan.
 *
 * Limited restrictive permission is hereby granted for educational
 * purpose(s) only and which are learning, understanding, explaining
 * and teaching.
 */

#pragma once

#if (!PrivateTypes_H)
#define PrivateTypes_H

#include <limits>

#include "Types.h"

namespace Graph
{
	class IMemoryPool
	{
	public:
		// _size should be allways <= _primaryPoolSize.
		virtual void* Allocate(size_t _size) = 0;
		virtual void Free(void* _memoryBlock) = 0;

	public:
		virtual ~IMemoryPool() {};
	};

	class StackMemoryPool : public IMemoryPool
	{
	public:
		typedef UInt32 SizeT;

		StackMemoryPool(SizeT _blockSize/*Largest allowed allocation size*/, SizeT _maxPoolSize = UINT_MAX);
		~StackMemoryPool();

		// _size should be allways <= BlockSize.
		void* Allocate(size_t _size);
		void Free(void* _ptr);

	private:
		void*	AddBlock(SizeT _blockSize);

	private:
		Ext::VList<SizeT> Pool;
		SizeT BlockSize, MaxPoolSize;
		SizeT BlockForNextAlloc;

#ifdef TrackAllocations
		Ext::List<void*, SizeT> AllocationPtr;
#endif
	};

	class ReleaseMemoryToPool
	{
	public:
		ReleaseMemoryToPool(StackMemoryPool& _pool, void* _block) : Pool(_pool), Block(_block) {};
		~ReleaseMemoryToPool() { Pool.Free(Block); };

		StackMemoryPool&	pool() { return Pool; };
		void*	ptr() { return Block; };

	private:
		StackMemoryPool&	Pool;
		void*	Block;
	};
}

#endif
