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
#if (!Graph_Utility_H)
#define Graph_Utility_H

#include "graph_types.h"
#include "Types.h"
#include "Bit.h"

#include <time.h>

namespace Graph
{

#define Swap(a, b, tempStorage) tempStorage = a; a = b; b = tempStorage;

#define BitSet(bits, idx)		bits[(idx) >> 3] |= Bit::BIT[(idx) & 0x07]

	//#define BitTest(bits, idx)		((bits[(idx) >> 3] & Bit::BIT[(idx) & 0x07]) != 0)

	//#define BitReset(bits, idx)	bits[(idx) >> 3] &= Bit::BitMask[(idx) & 0x07]

	//#define BitReset1(bits, i, j) bits[(i)] &= Bit::BitMask[(j)/* & 0x07*/]

	inline bool BitTest(byte* bits, ID idx)
	{
		return (bits[idx >> 3] & Bit::BIT[idx & 0x07]) != 0;
	};

	inline bool BitTest(byte* bits, size_t idx)
	{
		return (bits[idx >> 3] & Bit::BIT[idx & 0x07]) != 0;
	};

	//inline void BitSet(byte* bits, ID idx)
	//{
	//	bits[idx >> 3] |= Bit::BIT[idx & 0x07];
	//};

	inline void BitReset(byte* bits, ID idx)
	{
		bits[idx >> 3] &= Bit::BitMask[idx & 0x07];
	};

	inline void BitReset(byte* bits, int i, int j)
	{
		bits[i] &= Bit::BitMask[j/* & 0x07*/];
	};




#include "Templates.h"

#if (_MSC_VER)
#define forceinline	__forceinline
#else
#define forceinline	__attribute__((always_inline))
#endif

	class GraphDtor
	{
	public:
		GraphDtor(Ext::Array<Vertex> _graph) : ptr((byte*)_graph.ptr()) { };
		~GraphDtor() { delete[]ptr; };

	private:
		byte* ptr;
	};

	typedef void(*TextStream)(const char* message);

	void SetTraceMessageHandler(TextStream textStream);

	void PrintGraphMatrix(Ext::Array<Vertex> _graph, TextStream textStream);

#ifdef _WIN32
	#pragma comment(lib, "Kernel32.lib")
	extern "C"
	{
		extern UInt64 __stdcall GetTickCount64(void);
	}

#define GetCurrentTick		GetTickCount64
#else
	UInt64 forceinline GetCurrentTick()
	{
		struct timespec ts;
		// Linux 2.6.32 and above
		clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
		return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
	}
#endif


	size_t inline GetGraphAllocationSize(size_t _count)
	{
		return GetQWordAlignedSize(sizeof(Vertex) * _count) + GetQWordAlignedSizeForBits(_count) * _count;
	};

	Ext::Array<Vertex> CreateGraph(size_t _count);

	Ext::Array<Vertex> CreateGraph(size_t _count, void* _ptr);

	void FreeGraph(Ext::Array<Vertex>& _graph);

	bool IsCorrupt(Ext::Array<Vertex> _graph, bool _createdThroughCreateGraph = false);


	void forceinline ZeroMemoryPack8(void* _ptr, size_t count)
	{
		for (UInt64 *ptr = (UInt64 *)_ptr, *pEnd = ptr + (count >> 3); (ptr < pEnd); ptr++)
			*ptr = 0;
	};

	void forceinline CopyMemoryPack8(void* _dest, void* _src, size_t count)
	{
		for (UInt64 *dest = (UInt64 *)_dest, *src = (UInt64 *)_src, *pEnd = dest + (count >> 3); (dest < pEnd); dest++, src++)
			*dest = *src;
	};

	void forceinline SetNBits(byte* _ptr, size_t _n)
	{
		size_t i;
		for (i = 0; i < (_n / 8); i++)
			_ptr[i] = 0xFF;
		if ((_n % 8) != 0)
			_ptr[i] = (byte)((1 << (_n % 8)) - 1);
	}

	void forceinline SetNBits(UInt64* _ptr, size_t _n)
	{
		size_t i;
		for (i = 0; i < (_n / 64); i++)
			_ptr[i] = ~0;
		if ((_n % 64) != 0)
		{
			_ptr[i] = 0;
			for (i <<= 3; i < (_n / 8); i++)
				((byte*)_ptr)[i] = 0xFF;
			if ((_n % 8) != 0)
				((byte*)_ptr)[i] = (byte)((1 << (_n % 8)) - 1);
		}
	}

	size_t forceinline FindNextBit(byte* _ptr, size_t count)
	{
		byte *ptr = _ptr, *end = _ptr + count;
		while ((ptr < end) && (*ptr == 0))
			ptr++;

		return (ptr < end) ? (((ptr - _ptr) << 3) + _tzcnt_u32(*ptr)) : (count << 3);
	}

	size_t forceinline FindNextBitZero(byte* _ptr, size_t count)
	{
		byte *ptr = _ptr, *end = _ptr + count;
		while ((ptr < end) && (*ptr == 0xFF))
			ptr++;

		return (ptr < end) ? (((ptr - _ptr) << 3) + _tzcnt_u32(~*ptr)) : (count << 3);
	}

	size_t forceinline FindNextBitZero(byte* _ptr, byte* _activeBits, size_t count)
	{
		byte *ptr = _ptr, *end = _ptr + count;
		while ((ptr < end) && ((*ptr & *_activeBits) == *_activeBits))
		{
			ptr++;
			_activeBits++;
		}

		return (ptr < end) ? (((ptr - _ptr) << 3) + _tzcnt_u32((*ptr & *_activeBits) ^ *_activeBits)) : (count << 3);
	}

	size_t forceinline PopCount(UInt64* _p, size_t _size)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++)
			count += PopCount64(*_p);

		return count;
	};

	size_t forceinline PopCountAandB(UInt64* _p, UInt64* _q, size_t _size)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++)
			count += PopCount64(*_p & *_q);

		return count;
	};

	size_t forceinline PopCountAandB(UInt64* _p, UInt64* _q, size_t _size, size_t _minimum)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd) && (count < _minimum); _p++, _q++)
			count += PopCount64(*_p & *_q);

		return count;
	};

	size_t forceinline PopCountAandB(UInt64* _p, UInt64* _q, UInt64* _r, size_t _size, size_t _minimum)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd) && (count < _minimum); _p++, _q++, _r++)
			count += PopCount64(*_p & *_q & *_r);

		return count;
	};

	size_t forceinline PopCountAandB_Set(UInt64* _p, UInt64* _q, size_t _size)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++)
		{
			*_p &= *_q;
			count += PopCount64(*_p);
		}

		return count;
	};

	size_t forceinline PopCountAminusB_Set(UInt64* _p, UInt64* _q, size_t _size)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++)
		{
			count += PopCount64((*_p ^ *_q) & *_p);
			*_p &= *_q;
		}

		return count;
	};

	size_t forceinline PopCountAandB_Set(UInt64* _p, UInt64* _q, UInt64* _r, size_t _size)
	{
		size_t count = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++, _r++)
		{
			*_r = *_p & *_q;
			count += PopCount64(*_r);
		}

		return count;
	};

	void forceinline AandB(UInt64* _p, UInt64* _q, UInt64* _r, size_t _size)
	{
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++, _r++)
			*_r = *_p & *_q;
	};

	void forceinline AorBofC(UInt64* _p, UInt64* _q, UInt64* _r, size_t _size)
	{
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++, _r++)
			*_p = (*_p | *_q) & *_r;
	};

	void forceinline AminusB(UInt64* _p, UInt64* _q, UInt64* _r, size_t _size)
	{
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++, _r++)
			*_r = (*_p ^ *_q) & *_p;
	};

	void forceinline AminusB(UInt64* _p, UInt64* _p2, UInt64* _q, UInt64* _r, size_t _size)
	{
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _p2++, _q++, _r++)
		{
			auto r = (*_p & *_p2);
			*_r = (r ^ *_q) & r;
		}
	};

	void forceinline Negate(UInt64* _p, UInt64* _q, size_t _size)
	{
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _q++)
			*_q = ~(*_p);
	};

#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#define _IS_BIG_ENDIAN_ 1
#define _byteswap_uint64 __builtin_bswap64
#elif (defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__) || defined(__x86_64))
#define _IS_LITTLE_ENDIAN_ 1
#else
#undef _IS_LITTLE_ENDIAN_
#undef _IS_BIG_ENDIAN_
#endif

	ID forceinline GetMembers(UInt64* _p, UInt64* _p2, UInt64* _q, ID* _list, size_t _size)
	{
		ID idx = 0, offset = 0;
		for (UInt64 *pEnd = _p + _size; (_p < pEnd); _p++, _p2++, _q++, offset += 64)
		{
			auto r = (*_p & *_p2);
			UInt64 bits = (r ^ *_q) & r;
#if defined(_IS_BIG_ENDIAN_)
			bits = _byteswap_uint64(bits);
#endif

			while (bits != 0)
			{
				unsigned long pos;
				// bits Bn.....B0
				_BitScanForward64(&pos, bits);
				_bittestandreset64((Int64*)&bits, pos);
				_list[idx++] = offset + pos;
			}
		}

		return idx;
	}


	Ext::Array<Vertex> ReadDIMACSGraph(const char * _binGraphFile);

	SATFormula	ReadDIMACSSATFormula(const char * _satFormula);

	Ext::Array<Vertex> CreateGraph(SATFormula _formula);


	Ext::Array<Vertex> ResizeGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, void* _ptr = nullptr);

	inline Ext::Array<Vertex> CloneGraph(Ext::Array<Vertex> _graph, void* _ptr = nullptr)
	{
		return ResizeGraph(_graph, (decltype(Vertex::Id))_graph.size(), _ptr);
	};

	Ext::Array<Vertex> TransformGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, Ext::Array<decltype(Vertex::Id)> _idMap);

	// _idMap.first should sorted in ASC order.
	Ext::Array<Vertex> TransformGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, Ext::Array<Ext::pair<decltype(Vertex::Id), decltype(Vertex::Id)>> _idMap);

	void ClearVertexEdges(Ext::Array<Vertex> _graph, byte* _vertices);

	void ComplementGraph(Ext::Array<Vertex> _src, Ext::Array<Vertex> _complement);

	Ext::Array<Vertex> CombineGraph(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _graph2);

	Ext::Array<Vertex> MergeGraphEdges(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _graph2);

	Ext::Array<Vertex> ExtractGraph(Ext::Array<Vertex> _graph, byte* _bitset, void* _ptr = nullptr);

	bool ExtractGraph(Ext::Array<Vertex> _from, Ext::Array<Vertex> _to, byte* _mask, byte* _qwSizeOfBitset);

	size_t ExtractBits(byte* _from, byte* _to, byte* _mask, byte* _sizeOfBitset, size_t _size);

	size_t ExtractBitsFromComplement(byte* _complementFrom, byte* _to, byte* _mask, byte* _sizeOfBitset, size_t _size);


	bool IsSubgraph(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _subGraph);

	bool IsClique(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_cliqueMembers, decltype(Vertex::Id) _cliqueSize, byte* _bitset);

	bool IsIndependantSet(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_vertices, decltype(Vertex::Id) _verticesSize, byte* _bitset);

	bool IsProperColor(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_vertexColor);


	bool GetQualifiedEdges(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _minimumNeighbours, byte *_bitset);

	bool GetQualifiedEdges(Ext::Array<Vertex2> _graph, decltype(Vertex2::Id) _minimumNeighbours);

	namespace Clique
	{
		enum struct FindOperation : byte
		{
			ExactSearch = 1,
			MaximumClique = 2,
			EnumerateCliques = 3,
		};
	}

	Ext::Array<Vertex> CreateHardPartitionClique(decltype(Vertex::Id) _graphSize, decltype(Vertex::Id) _cliqueSize);

	decltype(Vertex::Id) FindClique(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _cliqueSize = INVALID_ID, Clique::FindOperation _op = Clique::FindOperation::MaximumClique);

	decltype(Vertex::Id) FindVertextColor(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _cliqueSize);
}
#endif
