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

#include "PrivateTypes.h"
#include "Types.h"
#include "MemoryAllocation.h"
#include <memory.h>
#include <cassert>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////
//

inline size_t GetAlignedSize(size_t size, size_t alignment)
{
	// assert((alignment & (alignment - 1)) == 0); // power of 2.

	return ((size + alignment - 1) & ~(alignment - 1));
}

inline bool IsAligned(void* ptr, size_t alignment)
{
	return (((PtrInt)ptr & (alignment - 1)) == 0);
}

inline bool IsAligned(size_t size, size_t alignment)
{
	return ((size & (alignment - 1)) == 0);
}

namespace Graph
{
	template < class SizeT = size_t >
	struct HeapBlock
	{
		struct Entry
		{
			SizeT Size;
			SizeT Next;
		};

		HeapBlock() : Ptr(nullptr), Available(0), Capacity(0), FreeBlockOffset(0), FirstFreeBlockSize(0), AlignmentGap(0) { };
		HeapBlock(void* ptr, SizeT size, SizeT alignmentGap = 0) : Ptr(ptr), Available(size), Capacity(size), FreeBlockOffset(size), FirstFreeBlockSize(size), AlignmentGap(alignmentGap) { };

		Entry* getFreeBlock() const { return /*(FreeBlockOffset == Capacity) ? nullptr : */(Entry*)(((byte*)Ptr) + FreeBlockOffset); }

		void*	Ptr;
		SizeT	Available;
		SizeT	Capacity;
		SizeT FreeBlockOffset;
		SizeT FirstFreeBlockSize;
		SizeT AlignmentGap;
	};

	typedef HeapBlock<StackMemoryPool::SizeT> _HeapBlock;

	///
	///	BlockForNextAlloc: This will track the block where last alloc or free op took place.
	///

#define AlignmentSize 16

	StackMemoryPool::StackMemoryPool(SizeT _blockSize, SizeT _maxPoolSize)
		: Pool(nullptr, 0, 0), BlockSize(_blockSize), 
		  MaxPoolSize((_blockSize <= _maxPoolSize) ? _maxPoolSize : _blockSize),
		 BlockForNextAlloc(0)
	{
		if (_blockSize == 0)
			throw std::invalid_argument("_blockSize");

		BlockSize = (decltype(BlockSize))GetAlignedSize(_blockSize, AlignmentSize);
		MaxPoolSize = (BlockSize <= _maxPoolSize) ? _maxPoolSize : BlockSize;
	}

	StackMemoryPool::~StackMemoryPool()
	{
		auto pool = Pool.ptr<_HeapBlock>();

		if ((Pool.size() < Pool.capacity()) && (pool[Pool.size()].Ptr != nullptr))
			FreeMemory(((byte*)pool[Pool.size()].Ptr) - pool[Pool.size()].AlignmentGap);

		for (size_t i = 0; i < Pool.size(); i++)
			FreeMemory(((byte*)pool[i].Ptr) - pool[i].AlignmentGap);

		delete[]	pool;
		Pool = Ext::VList<SizeT>(nullptr, 0, 0);
	}

	/*
	*	_size should be
	*		allways less than or equal to _primaryPoolSize.
	*	Note: All free blocks are not checked for possible allocation.
	*/
	void* StackMemoryPool::Allocate(size_t _size)
	{
		_size = GetAlignedSize(_size, AlignmentSize);
		if ((_size == 0) || (_size > BlockSize))
			return nullptr;

		auto pool = Pool.ptr<_HeapBlock>();
		auto idx = BlockForNextAlloc;

		_size += AlignmentSize;
		if ((0 == Pool.size()) || (pool[idx].FreeBlockOffset == pool[idx].Capacity) || (pool[idx].FirstFreeBlockSize < _size))
		{
			decltype(idx) i;
			for (i = 0; (i < Pool.size()) && (pool[++idx % Pool.size()].FirstFreeBlockSize < _size); i++);
			if (i == Pool.size())
			{
				if (Pool.size() < (MaxPoolSize / BlockSize))
				{
					pool = (decltype(pool))AddBlock(BlockSize + AlignmentSize * 8 /*for sub-block size header*/);
					idx = BlockForNextAlloc;
				}
				else
					return nullptr;
			}
			else
				idx %= Pool.size();
		}

		_HeapBlock& heap = pool[idx];
		auto freeBlock = *heap.getFreeBlock();
		void*	ptr = (byte*)heap.Ptr + heap.FreeBlockOffset + AlignmentSize;
		heap.Available -= (decltype(_HeapBlock::Available))_size;
		freeBlock.Size -= (decltype(_HeapBlock::Entry::Size))_size;
		heap.FirstFreeBlockSize = freeBlock.Size;

		if (freeBlock.Size == 0)
		{
			heap.FreeBlockOffset = freeBlock.Next;

			if ((heap.FreeBlockOffset < heap.Capacity))
			{
				freeBlock = *heap.getFreeBlock();
				heap.FirstFreeBlockSize = freeBlock.Size;
			}
		}
		else
		{
			heap.FreeBlockOffset += (decltype(_HeapBlock::Available))_size;
			*heap.getFreeBlock() = freeBlock;
		}

		// set allocated size in allocation header
		((size_t*)ptr)[-1] = _size - AlignmentSize;

		return ptr;
	}

	void StackMemoryPool::Free(void* _ptr)
	{
		if ((_ptr == nullptr) || (Pool.size() == 0) || !IsAligned(_ptr, AlignmentSize))
			return;

		auto pool = Pool.ptr<_HeapBlock>();
		auto idx = (BlockForNextAlloc < Pool.size()) ? BlockForNextAlloc : Pool.size() - 1;
		if ((_ptr < pool[idx].Ptr) || (((byte*)pool[idx].Ptr + pool[idx].Capacity) <= _ptr))
		{
			idx = Pool.size();
			while ((idx-- > 0) && ((_ptr < pool[idx].Ptr) || (((byte*)pool[idx].Ptr + pool[idx].Capacity) <= _ptr)));
			if (idx == 0)
				return; // block is outside of this pool.
		}

		// read allocated size from allocation header
		_HeapBlock& heap = pool[idx];
		SizeT size = (SizeT)((size_t*)_ptr)[-1];
		assert(IsAligned(size, AlignmentSize) && ((((byte*)_ptr) - (byte*)heap.Ptr + size) <= heap.Capacity));

		BlockForNextAlloc = idx;

		size += AlignmentSize;
		heap.Available += size;
		if (((idx + 1) == Pool.size()) && (heap.Available == heap.Capacity))
		{
			do
			{
				if (((idx + 1) < Pool.capacity()) && (pool[idx + 1].Ptr != nullptr))
				{
					FreeMemory(((byte*)pool[idx + 1].Ptr) - pool[idx + 1].AlignmentGap);
					pool[idx + 1] = _HeapBlock(nullptr, 0);
				}

				Pool.size(Pool.size() - 1);
			} while ((idx-- > 0) && (pool[idx].Available == pool[idx].Capacity));

			BlockForNextAlloc = idx;

			return;
		}

		SizeT freeBlockOffset = (SizeT)(((byte*)_ptr) - (byte*)heap.Ptr - AlignmentSize);
		_HeapBlock::Entry *pFreeBlock, freeBlock;
		SizeT prevBlockOffset = heap.Capacity, currentBlockOffset = heap.FreeBlockOffset;

		while (currentBlockOffset < heap.Capacity)
		{
			pFreeBlock = (decltype(pFreeBlock))((byte*)heap.Ptr + currentBlockOffset);
			if ((freeBlockOffset + size) < currentBlockOffset)
			{
				pFreeBlock = (decltype(pFreeBlock))((byte*)heap.Ptr + freeBlockOffset);
				pFreeBlock->Next = currentBlockOffset;
				pFreeBlock->Size = size;

				if (currentBlockOffset == heap.FreeBlockOffset)
				{
					heap.FreeBlockOffset = freeBlockOffset;
					heap.FirstFreeBlockSize = pFreeBlock->Size;
				}
				else
					((_HeapBlock::Entry*)((byte*)heap.Ptr + prevBlockOffset))->Next = freeBlockOffset;

				break;
			}
			else if ((freeBlockOffset + size) == currentBlockOffset)
			{
				freeBlock = *pFreeBlock;

				pFreeBlock = (decltype(pFreeBlock))((byte*)heap.Ptr + freeBlockOffset);
				pFreeBlock->Next = freeBlock.Next;
				pFreeBlock->Size = size + freeBlock.Size;

				if (currentBlockOffset == heap.FreeBlockOffset)
				{
					heap.FreeBlockOffset = freeBlockOffset;
					heap.FirstFreeBlockSize = pFreeBlock->Size;
				}

				break;
			}
			else if ((currentBlockOffset + pFreeBlock->Size) == freeBlockOffset)
			{
				pFreeBlock->Size += size;
				if ((pFreeBlock->Next != heap.Capacity) && ((currentBlockOffset + pFreeBlock->Size) == pFreeBlock->Next))
				{
					freeBlock = *((_HeapBlock::Entry*)((byte*)heap.Ptr + pFreeBlock->Next));

					pFreeBlock->Next = freeBlock.Next;
					pFreeBlock->Size += freeBlock.Size;
				}

				if (currentBlockOffset == heap.FreeBlockOffset)
					heap.FirstFreeBlockSize = pFreeBlock->Size;

				break;
			}

			prevBlockOffset = currentBlockOffset;
			currentBlockOffset = pFreeBlock->Next;
		}

		if (currentBlockOffset == heap.Capacity)
		{
			pFreeBlock = (decltype(pFreeBlock))((byte*)heap.Ptr + freeBlockOffset);
			pFreeBlock->Next = currentBlockOffset;
			pFreeBlock->Size = size;

			if (currentBlockOffset == heap.FreeBlockOffset)
			{
				heap.FreeBlockOffset = freeBlockOffset;
				heap.FirstFreeBlockSize = pFreeBlock->Size;
			}
			else
				((_HeapBlock::Entry*)((byte*)heap.Ptr + prevBlockOffset))->Next = freeBlockOffset;
		}
	}

	void* StackMemoryPool::AddBlock(SizeT _blockSize)
	{
		auto pool = Pool.ptr<_HeapBlock>();
		auto idx = Pool.size();

		if (idx == Pool.capacity())
		{
			auto poolT = Ext::List<_HeapBlock, SizeT>(new _HeapBlock[idx + 16], idx + 16, idx);
			if (idx > 0)
			{
				for (size_t i = 0; i < idx; i++)
					poolT[i] = pool[i];

				delete[] Pool.ptr();
			}

			Pool = poolT.toVList();
		}

		pool = Pool.ptr<_HeapBlock>();
		if (pool[idx].Ptr == nullptr)
		{
			pool[idx].Capacity = (decltype(_HeapBlock::Capacity))GetAlignedSize(_blockSize, AlignmentSize) + AlignmentSize/*for aligning when needed*/;
			pool[idx].Ptr = (byte*)AllocMemory(pool[idx].Capacity);

			pool[idx].AlignmentGap = (int)(AlignmentSize - (((PtrInt)pool[idx].Ptr) & (AlignmentSize - 1)));
			pool[idx].Capacity -= pool[idx].AlignmentGap;
			pool[idx].Ptr = ((byte*)pool[idx].Ptr) + pool[idx].AlignmentGap;
		}

		pool[idx].Available = pool[idx].Capacity;
		pool[idx].FirstFreeBlockSize = pool[idx].Available;
		pool[idx].FreeBlockOffset = 0;
		Pool.size(Pool.size() + 1);

		auto entry = (_HeapBlock::Entry*)pool[idx].Ptr;
		entry->Size = pool[idx].Available;
		entry->Next = pool[idx].Capacity;

		BlockForNextAlloc = idx;

		return pool;
	}
}
