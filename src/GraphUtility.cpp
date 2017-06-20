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

#include "GraphUtility.h"
#include "graph_types.h"
#include "PrivateTypes.h"
#include "Bit.h"

#include <memory.h>
#include <stdio.h>

namespace Graph
{
	void PrintGraphMatrix(Ext::Array<Vertex> _graph, TextStream textStream)
	{
		if (textStream == nullptr)
			return;

		char* sz = new char[_graph.size() + 3];

		for (size_t j, i = 0; (i < _graph.size()); i++)
		{
			auto neighbours = _graph[i].Neighbours;
			for (j = 0; (j < _graph.size()); j++)
				sz[j] = BitTest(neighbours, j) ? '*' : ' ';

			sz[j++] = '\r';
			sz[j++] = '\n';
			sz[j++] = 0;

			textStream(sz);
		}

		delete sz;

		textStream("\r\n");
	}


	Ext::Array<Vertex> CreateGraph(size_t _count, void* _ptr)
	{
		if (_count <= 0)
			return Ext::Array<Vertex>((Vertex*)_ptr, 0);

		ZeroMemoryPack8(_ptr, GetGraphAllocationSize(_count));

		size_t size = GetQWordAlignedSizeForBits(_count);
		byte* ptr = (byte*)_ptr;
		Ext::Array<Vertex> graph = Ext::Array<Vertex>((Vertex*)ptr, _count);

		ptr += GetQWordAlignedSize(sizeof(Vertex) * _count);
		for (size_t i = 0; i < graph.size(); i++, ptr += size)
		{
			graph[i].Id = (ID)i;
			graph[i].Count = 0;
			graph[i].Neighbours = (byte*)ptr;
		}

		return graph;
	}

	Ext::Array<Vertex> CreateGraph(size_t _count)
	{
		return CreateGraph(_count, (_count <= 0) ? nullptr : new byte[GetGraphAllocationSize(_count)]);
	}

	void FreeGraph(Ext::Array<Vertex>& _graph)
	{
		if (_graph.ptr() != nullptr)
			delete[] (byte*)_graph.ptr();

		_graph = Ext::Array<Vertex>();
	}

	bool IsCorrupt(Ext::Array<Vertex> _graph, bool _createdThroughCreateGraph)
	{
		if (_graph.size() == 0)
			return false;

		if (_graph.ptr() == nullptr)
			return true;

		size_t size = GetQWordAlignedSizeForBits(_graph.size());
		byte* ptr = (byte*)_graph.ptr();

		ptr += GetQWordAlignedSize(sizeof(Vertex) * _graph.size());
		for (size_t i = 0; i < _graph.size(); i++, ptr += size)
		{
			if ((_graph[i].Id != i) || (_graph[i].Neighbours == nullptr) || (_graph[i].Count != PopCount((UInt64*)_graph[i].Neighbours, size >> 3)))
				return true;

			if (_createdThroughCreateGraph && (_graph[i].Neighbours != ptr))
				return true;
		}

		return false;
	}


	Ext::Array<Vertex> ResizeGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, void* _ptr)
	{
		Ext::Array<Vertex> graph = (_ptr == nullptr) ? CreateGraph(_newSize) : CreateGraph(_newSize, _ptr);

		if (_graph.size() <= _newSize)
		{
			size_t bytes = (_graph.size() + 7) / 8;
			for (size_t i = 0; i < _graph.size(); i++)
			{
				graph[i].Count = _graph[i].Count;
				memcpy(graph[i].Neighbours, _graph[i].Neighbours, bytes);
			}
		}
		else if (_newSize > 0)
		{
			size_t bytes = _newSize / 8;
			byte mask = (byte)((0x01 << (_newSize % 8)) - 1);

			for (size_t i = 0; i < _newSize; i++)
			{
				size_t count = 0;
				auto dest = graph[i].Neighbours;
				auto src = _graph[i].Neighbours;

				for (size_t j = 0; j < bytes; j++)
				{
					dest[j] = src[j];
					count += PopCount32(dest[j]);
				}

				if (mask > 0)
				{
					dest[bytes] = src[bytes] & mask;
					count += PopCount32(dest[bytes]);
				}

				graph[i].Count = (decltype(Vertex::Id))count;
			}
		}

		return graph;
	}

	Ext::Array<Vertex> TransformGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, Ext::Array<decltype(Vertex::Id)> _idMap)
	{
		if (_graph.size() < _idMap.size())
			throw "idMap::size is > graph::size.";

		for (size_t i = 0; i < _idMap.size(); i++)
		{
			if ((_idMap[i] < 0) || ((_idMap[i] >= _newSize) && (_idMap[i] != INVALID_ID)))
				throw "Invalid idMap.";
		}

		Ext::Array<Vertex> graph = CreateGraph(_newSize);

		for (size_t i = 0; i < _idMap.size(); i++)
		{
			if (_idMap[i] == INVALID_ID)
				continue;

			auto src = _graph[i].Neighbours;
			graph[_idMap[i]].Count = 1;

			for (size_t j = 0; j < _idMap.size(); j++)
			{
				if ((_idMap[j] == INVALID_ID) || !BitTest(src, (decltype(Vertex::Id))j))
					continue;

				BitSet(graph[_idMap[i]].Neighbours, _idMap[j]);
				BitSet(graph[_idMap[j]].Neighbours, _idMap[i]);

				graph[_idMap[j]].Count = 1;
			}
		}

		size_t bytes = (graph.size() + 7) / 8;
		for (size_t i = 0; i < graph.size(); i++)
		{
			if (graph[i].Count == 0)
				continue;

			auto dest = graph[i].Neighbours;
			size_t count = 0;

			for (size_t j = 0; j < bytes; j++)
				count += PopCount32(dest[j]);

			graph[i].Count = (decltype(Vertex::Id))count;
		}

		return graph;
	}

	Ext::Array<Vertex> TransformGraph(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _newSize, Ext::Array<Ext::pair<decltype(Vertex::Id), decltype(Vertex::Id)>> _idMap)
	{
		for (size_t i = 0; i < _idMap.size(); i++)
		{
			if ((_idMap[i].first < 0) || (_idMap[i].first >= _graph.size()) || 
				 (_idMap[i].second < 0) || (_idMap[i].second >= _newSize) ||
				 ((0 < i) && (_idMap[i].first <= _idMap[i - 1].first)))
				throw "Invalid idMap.";
		}

		Ext::Array<Vertex> graph = CreateGraph(_newSize);

		for (size_t i = 0; i < _idMap.size(); i++)
		{
			auto ii = _idMap[i].second;

			auto src = _graph[_idMap[i].first].Neighbours;
			graph[ii].Count = 1;

			for (size_t j = 0; j < _idMap.size(); j++)
			{
				if (!BitTest(src, (decltype(Vertex::Id))_idMap[j].first))
					continue;

				auto jj = _idMap[j].second;
				BitSet(graph[ii].Neighbours, jj);
				BitSet(graph[jj].Neighbours, ii);

				graph[jj].Count = 1;
			}
		}

		size_t bytes = (graph.size() + 7) / 8;
		for (size_t i = 0; i < graph.size(); i++)
		{
			if (graph[i].Count == 0)
				continue;

			auto dest = graph[i].Neighbours;
			size_t count = 0;

			for (size_t j = 0; j < bytes; j++)
				count += PopCount32(dest[j]);

			graph[i].Count = (decltype(Vertex::Id))count;
		}

		return graph;
	}


	void ClearVertexEdges(Ext::Array<Vertex> _graph, byte* _vertices)
	{
		if (_graph.size() == 0)
			return;

		if (_vertices == nullptr)
			throw "InvalidArgument: _vertices.";

		size_t size = GetQWordAlignedSizeForBits(_graph.size());
		for (size_t i = 0; i < _graph.size(); i++)
		{
			auto neighbours = _graph[i].Neighbours;

			if (BitTest(_vertices, i))
			{
				ZeroMemoryPack8(neighbours, size);
				_graph[i].Count = 0;
			}
			else
			{
				for (size_t j = 0; j < size; j++)
					neighbours[j] &= ~_vertices[j];

				_graph[i].Count = (decltype(Vertex::Id))PopCount((UInt64*)neighbours, size >> 3);
			}
		}
	}

	void ComplementGraph(Ext::Array<Vertex> _src, Ext::Array<Vertex> _complement)
	{
		if (_src.size() != _complement.size())
			throw "Both graph should be of equal size.";

		if (_src.size() == 0)
			return;

		size_t size = GetQWordAlignedSizeForBits(_src.size());
		for (size_t j = 0, i = 0; i < _src.size(); i++)
		{
			auto neighbours = _complement[i].Neighbours;
			auto srcNeighbours = _src[i].Neighbours;
			bool isSelfEdgeExist = BitTest(srcNeighbours, (decltype(Vertex::Id))i);

			for (j = 0; j < (_complement.size() / 8); j++)
				neighbours[j] = ~srcNeighbours[j];
			if ((_complement.size() % 8) != 0)
				neighbours[j++] = srcNeighbours[j] ^ ((byte)((1 << (_complement.size() % 8)) - 1));
			for (; j < size; j++)
				((byte*)neighbours)[j] = 0;

			if (isSelfEdgeExist)
				BitSet(neighbours, (decltype(Vertex::Id))i);
			else
				BitReset(neighbours, (decltype(Vertex::Id))i);

			_complement[i].Count = (decltype(Vertex::Id))PopCount((UInt64*)neighbours, size >> 3);
		}
	}

	Ext::Array<Vertex> CombineGraph(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _graph2)
	{
		Ext::Array<Vertex> graph = ResizeGraph(_graph, (decltype(Vertex::Id))(_graph.size() + _graph2.size()));

		size_t bytes = (_graph2.size() + 7) / 8;
		int leftShift = (_graph.size() % 8);
		int rightShift = (8 - leftShift);

		for (size_t i = 0; i < _graph2.size(); i++)
		{
			auto idx = _graph.size() + i;
			auto dest = graph[idx].Neighbours;
			auto src = _graph2[i].Neighbours;

			if (leftShift == 0)
				memcpy(dest + (_graph.size() / 8), src, bytes);
			else
			{
				for (size_t j = 0, k = _graph.size() / 8; j < bytes; j++)
				{
					dest[k] |= (src[j] << leftShift) && 0xFF;
					dest[++k] |= src[j] >> rightShift;
				}
			}

			graph[idx].Count = _graph2[i].Count;
		}

		return graph;
	}

	Ext::Array<Vertex> MergeGraphEdges(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _graph2)
	{
		Ext::Array<Vertex> graph, graph2;

		if (_graph.size() >= _graph2.size())
		{
			graph = ResizeGraph(_graph, (decltype(Vertex::Id))_graph.size());
			graph2 = _graph2;
		}
		else
		{
			graph = ResizeGraph(_graph2, (decltype(Vertex::Id))_graph2.size());
			graph2 = _graph;
		}

		size_t bytes = (graph2.size() + 7) / 8;
		size_t bytes2 = (graph.size() + 7) / 8;

		for (size_t i = 0; i < graph2.size(); i++)
		{
			auto dest = graph[i].Neighbours;
			auto src = graph2[i].Neighbours;

			for (size_t j = 0; j < bytes; j++)
				dest[j] |= src[j];

			size_t count = 0;
			for (size_t j = 0; j < bytes2; j++)
				count += PopCount32(dest[j]);

			graph[i].Count = (decltype(Vertex::Id))count;
		}

		return graph;
	}

	Ext::Array<Vertex> ExtractGraph(Ext::Array<Vertex> _graph, byte* _bitset, void* _ptr)
	{
		auto size = (_bitset == nullptr) ? 0 : (decltype(Vertex::Id))PopCount((UInt64*)_bitset, GetQWordSizeForBits(_graph.size()));

		if (size == 0)
			return Ext::Array<Vertex>();
		else if (size == _graph.size())
			return CloneGraph(_graph, _ptr);

		Ext::Array<Vertex> graph = CreateGraph(size, _ptr);
		byte* _sizeOfBitset = new byte[GetSizeForBits(_graph.size())];

		ExtractGraph(_graph, graph, _bitset, _sizeOfBitset);

		delete[] _sizeOfBitset;

		return graph;
	}


	static UInt32 EndianOrder = 0x01020304UL;
	inline bool IsLittleEndian()
	{
		return (*((byte*)&EndianOrder) == 4);
	}

	inline bool IsBigEndian()
	{
		return (*((byte*)&EndianOrder) == 1);
	}

	bool ExtractGraph(Ext::Array<Vertex> _from, Ext::Array<Vertex> _to, byte* _mask, byte* _sizeOfBitset)
	{
		auto size = (decltype(Vertex::Id))PopCount((UInt64*)_mask, GetQWordSizeForBits(_from.size()));
		if (_to.size() < size)
			return false;

		decltype(Vertex::Id) i, j, k, l;
		size_t chuncks;

		if (IsLittleEndian())
		{
			auto mask = (UInt64*)_mask;
			chuncks = GetQWordSizeForBits(_from.size());

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount64(mask[k]);

			for (j = 0, i = 0; i < _from.size(); i++)
			{
				if (!BitTest(_mask, i))
					continue;

				auto dest = (decltype(mask))_to[j].Neighbours;
				auto src = (decltype(mask))_from[i].Neighbours;
				size_t count = 0, count2 = 0;

				for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
				{
					if (mask[k] == 0)
						continue;

					byte bitsCount = (byte)(count & 0x3F);
					auto bits = _pext_u64(src[k], mask[k]);

					dest[l] |= (bits << bitsCount);
					count += _sizeOfBitset[k];

					if ((bitsCount + _sizeOfBitset[k]) < 64)
						continue;

					count2 += PopCount64(dest[l]);
					l++;

					if (_sizeOfBitset[k] > (64 - bitsCount))
						dest[l] = (bits >> (64 - bitsCount));
					else if (count < size)
						dest[l] = 0;
				}

				if ((count & 0x3F) > 0)
					count2 += PopCount64(dest[l]);

				_to[j].Count = (decltype(Vertex::Count))count2;

				j++;
			}
		}
		else if (IsBigEndian())
		{
			auto mask = _mask;
			chuncks = GetSizeForBits(_from.size());

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount8(mask[k]);

			for (j = 0, i = 0; i < _from.size(); i++)
			{
				if (!BitTest(_mask, i))
					continue;

				auto dest = (decltype(mask))_to[j].Neighbours;
				auto src = (decltype(mask))_from[i].Neighbours;
				size_t count = 0, count2 = 0;
				for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
				{
					if (mask[k] == 0)
						continue;

					byte bitsCount = (byte)(count & 0x07);
					auto bits = _pext_u32(src[k], mask[k]);

					dest[l] |= (bits << bitsCount);
					count += _sizeOfBitset[k];

					if ((bitsCount + _sizeOfBitset[k]) < 8)
						continue;

					count2 += PopCount8(dest[l]);
					l++;

					if (_sizeOfBitset[k] > (8 - bitsCount))
						dest[l] = (bits >> (8 - bitsCount));
					else if (count < size)
						dest[l] = 0;
				}

				if ((count & 0x07) > 0)
					count2 += PopCount8(dest[l]);

				_to[j].Count = (decltype(Vertex::Count))count2;

				j++;
			}
		}
		else
			throw "Not Implemented.";


		for (i = size; i < _to.size(); i++)
		{
			_to[i].Count = 0;
			ZeroMemoryPack8(_to[i].Neighbours, _to.size() + 7);
		}

		return true;
	}

	size_t ExtractBits(byte* _from, byte* _to, byte* _mask, byte* _sizeOfBitset, size_t _size)
	{
		auto size = (decltype(Vertex::Id))PopCount((UInt64*)_mask, GetQWordSizeForBits(_size));
		decltype(Vertex::Id) k, l;
		size_t chuncks;

		if (IsLittleEndian())
		{
			auto mask = (UInt64*)_mask;
			chuncks = GetQWordSizeForBits(_size);

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount64(mask[k]);

			auto dest = (decltype(mask))_to;
			auto src = (decltype(mask))_from;
			size_t count = 0, count2 = 0;

			for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
			{
				if (mask[k] == 0)
					continue;

				byte bitsCount = (byte)(count & 0x3F);
				auto bits = _pext_u64(src[k], mask[k]);

				dest[l] |= (bits << bitsCount);
				count += _sizeOfBitset[k];

				if ((bitsCount + _sizeOfBitset[k]) < 64)
					continue;

				count2 += PopCount64(dest[l]);
				l++;

				if (_sizeOfBitset[k] > (64 - bitsCount))
					dest[l] = (bits >> (64 - bitsCount));
				else if (count < size)
					dest[l] = 0;
			}

			if ((count & 0x3F) > 0)
				count2 += PopCount64(dest[l]);

			return count2;
		}
		else if (IsBigEndian())
		{
			auto mask = _mask;
			chuncks = GetSizeForBits(_size);

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount8(mask[k]);

			auto dest = (decltype(mask))_to;
			auto src = (decltype(mask))_from;
			size_t count = 0, count2 = 0;
			for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
			{
				if (mask[k] == 0)
					continue;

				byte bitsCount = (byte)(count & 0x07);
				auto bits = _pext_u32(src[k], mask[k]);

				dest[l] |= (bits << bitsCount);
				count += _sizeOfBitset[k];

				if ((bitsCount + _sizeOfBitset[k]) < 8)
					continue;

				count2 += PopCount8(dest[l]);
				l++;

				if (_sizeOfBitset[k] > (8 - bitsCount))
					dest[l] = (bits >> (8 - bitsCount));
				else if (count < size)
					dest[l] = 0;
			}

			if ((count & 0x07) > 0)
				count2 += PopCount8(dest[l]);

			return count2;
		}
		else
			throw "Not Implemented.";

		return 0;
	}

	size_t ExtractBitsFromComplement(byte* _from, byte* _to, byte* _mask, byte* _sizeOfBitset, size_t _size)
	{
		auto size = (decltype(Vertex::Id))PopCount((UInt64*)_mask, GetQWordSizeForBits(_size));
		decltype(Vertex::Id) k, l;
		size_t chuncks;

		if (IsLittleEndian())
		{
			auto mask = (UInt64*)_mask;
			chuncks = GetQWordSizeForBits(_size);

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount64(mask[k]);

			auto dest = (decltype(mask))_to;
			auto src = (decltype(mask))_from;
			size_t count = 0, count2 = 0;

			for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
			{
				if (mask[k] == 0)
					continue;

				byte bitsCount = (byte)(count & 0x3F);
				auto bits = _pext_u64(~src[k], mask[k]);

				dest[l] |= (bits << bitsCount);
				count += _sizeOfBitset[k];

				if ((bitsCount + _sizeOfBitset[k]) < 64)
					continue;

				count2 += PopCount64(dest[l]);
				l++;

				if (_sizeOfBitset[k] > (64 - bitsCount))
					dest[l] = (bits >> (64 - bitsCount));
				else if (count < size)
					dest[l] = 0;
			}

			if ((count & 0x3F) > 0)
				count2 += PopCount64(dest[l]);

			return count2;
		}
		else if (IsBigEndian())
		{
			auto mask = _mask;
			chuncks = GetSizeForBits(_size);

			for (k = 0; k < chuncks; k++)
				_sizeOfBitset[k] = (byte)PopCount8(mask[k]);

			auto dest = (decltype(mask))_to;
			auto src = (decltype(mask))_from;
			size_t count = 0, count2 = 0;
			for (l = 0, dest[l] = 0, k = 0; k < chuncks; k++)
			{
				if (mask[k] == 0)
					continue;

				byte bitsCount = (byte)(count & 0x07);
				auto bits = _pext_u32((byte) ~src[k], mask[k]);

				dest[l] |= (bits << bitsCount);
				count += _sizeOfBitset[k];

				if ((bitsCount + _sizeOfBitset[k]) < 8)
					continue;

				count2 += PopCount8(dest[l]);
				l++;

				if (_sizeOfBitset[k] > (8 - bitsCount))
					dest[l] = (bits >> (8 - bitsCount));
				else if (count < size)
					dest[l] = 0;
			}

			if ((count & 0x07) > 0)
				count2 += PopCount8(dest[l]);

			return count2;
		}
		else
			throw "Not Implemented.";

		return 0;
	}


	bool IsSubgraph(Ext::Array<Vertex> _graph, Ext::Array<Vertex> _subGraph)
	{
		if (_subGraph.size() > _graph.size())
			return false;

		for (decltype(Vertex::Id) i = 0; i < _subGraph.size(); i++)
		{
			auto neighbours = _subGraph[i].Neighbours;
			auto neighbours2 = _graph[i].Neighbours;

			for (decltype(Vertex::Id) j = 0; j < _subGraph.size(); j++)
			{
				if (BitTest(neighbours, j) && !BitTest(neighbours2, j))
					return false;
			}
		}


		return true;
	}


	bool GetQualifiedEdges(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _minimumNeighbours, byte *_bitset)
	{
		byte			*neighbours;
		Vertex		*pV;

		auto	bitsetLength = decltype(Vertex::Id)GetQWordAlignedSizeForBits(_graph.size());
		decltype(Vertex::Id)	i, j, k, l, count, eligibleCount, eligibleCountAtStart;
		byte			bit;

		eligibleCount = 0;
		ZeroMemoryPack8(_bitset, bitsetLength);

		for (i = 0; i < _graph.size(); i++)
		{
			// assert(i == _graph[i].Id);
			pV = &_graph[i];
			if (pV->Count >= _minimumNeighbours)
			{
				eligibleCount++;
				BitSet(_bitset, i);
			}
			else
			{
				pV->Count = 0;
#if defined(_DEBUG)
				// ZeroMemoryPack8(pV->Neighbours, bitsetLength);	// comment this code in release mode.
#endif
			}
		}

		do
		{
			eligibleCountAtStart = eligibleCount;
			for (i = 0; (i < _graph.size()) && (eligibleCount >= _minimumNeighbours); i++)
			{
				if (!BitTest(_bitset, i))
					continue;

				pV = &_graph[i];
				neighbours = pV->Neighbours;
				pV->Count = (decltype(Vertex::Count)) PopCountAandB_Set((UInt64*)neighbours, (UInt64*)_bitset, bitsetLength >> 3);

				for (j = ((i + 1) >> 3), k = ((i + 1) & 0x07); (j < bitsetLength) && (pV->Count >= _minimumNeighbours); j++, k = 0)
				{
					for (bit = (byte)(neighbours[j] >> k); (0 < bit) && (pV->Count >= _minimumNeighbours); k++, bit >>= 1)
					{
						if (1 == (bit & 0x01))
						{
							l = (j << 3) + k;

							count = (decltype(Vertex::Count))PopCountAandB((UInt64*)neighbours, (UInt64*)_graph[l].Neighbours, bitsetLength >> 3, _minimumNeighbours);
							if (count < _minimumNeighbours)
							{
								BitReset(_graph[l].Neighbours, i);
								if (--_graph[l].Count < _minimumNeighbours)
								{
									//	ZeroMemoryPack8(_graph[m].Neighbours, bitSetLength);	// comment this code in release mode.
									_graph[l].Count = 0;
									eligibleCount--;
									BitReset(_bitset, l);
								}

								BitReset(neighbours, j, k);
								pV->Count--;
							}
						}
					}
				}

				if (pV->Count < _minimumNeighbours)
				{
					//	ZeroMemoryPack8(pV->Neighbours, bitSetLength);	// comment this code in release mode.
					pV->Count = 0;
					eligibleCount--;
					BitReset(_bitset, i);
				}
			}
		} while ((eligibleCountAtStart > eligibleCount) && (eligibleCount >= _minimumNeighbours));

		return (eligibleCount >= _minimumNeighbours);
	}

	bool GetQualifiedEdges(Ext::Array<Vertex2> _graph, decltype(Vertex2::Id) _minimumNeighbours)
	{
		UInt64		bitset = 0;
		byte			*_bitset = (byte*)&bitset;
		byte			*neighbours;
		Vertex2		*pV;

		decltype(pV->Id)	bitsetLength = (decltype(pV->Id))((_graph.size() + 7) >> 3);
		decltype(pV->Id)	i, j, k, m, eligibleCount, eligibleCountAtStart;
		byte			bit;

		eligibleCount = 0;
		bitset = 0;

		for (i = 0; i < _graph.size(); i++)
		{
			pV = &_graph[i];
			if (pV->Count >= _minimumNeighbours)
			{
				eligibleCount++;
				BitSet(_bitset, pV->Id);	// Assert(i == pV->Id);
			}
			else
			{
				pV->Count = 0;
				pV->lNeighbours = 0;
			}
		}

		do
		{
			eligibleCountAtStart = eligibleCount;
			for (i = 0; (i < _graph.size()) && (eligibleCount >= _minimumNeighbours); i++)
			{
				if (!BitTest(_bitset, i))
					continue;

				pV = &_graph[i];
				pV->lNeighbours &= *((UInt64*)_bitset);
				pV->Count = (decltype(pV->Count))PopCount64(pV->lNeighbours);
				neighbours = pV->Neighbours;

				for (j = ((i + 1) >> 3), k = ((i + 1) & 0x07); (j < bitsetLength) && (pV->Count >= _minimumNeighbours); j++, k = 0)
				{
					for (bit = (byte)(neighbours[j] & ~((1 << k) - 1)); bit > 0; bit &= bit - 1)
					{
						k = _tzcnt_u32(bit);
						m = (j << 3) + k;
						if (PopCount64(pV->lNeighbours & _graph[m].lNeighbours) < _minimumNeighbours)
						{
							BitReset(_graph[m].Neighbours, i);
							if (--_graph[m].Count < _minimumNeighbours)
							{
								_graph[m].Count = 0;
								_graph[m].lNeighbours = 0;

								eligibleCount--;
								BitReset(_bitset, m);
							}

							BitReset(neighbours, j, k);
							pV->Count--;
						}
					}
				}

				if (pV->Count < _minimumNeighbours)
				{
					pV->Count = 0;
					pV->lNeighbours = 0;

					eligibleCount--;
					BitReset(_bitset, i);
				}
			}
		} while ((eligibleCountAtStart > eligibleCount) && (eligibleCount >= _minimumNeighbours));

		return (eligibleCount >= _minimumNeighbours);
	}


	Ext::Array<Vertex> CreateGraph(SATFormula _formula)
	{
		if (_formula.Clauses == 0)
			return Ext::Array<Vertex>();

		Ext::Array<Vertex> graph = CreateGraph(_formula.Nodes);

		try
		{
			for (ID i = 0; i < _formula.Clauses; i++)
			{
				auto clauseSize = _formula.getClauseSize(i);
				auto startNode = _formula.getClauseStartNode(i);
				auto pLiterals = _formula.getClause(i);

				for (ID j = i + 1; j < _formula.Clauses; j++)
				{
					auto clauseSize2 = _formula.getClauseSize(j);
					auto startNode2 = _formula.getClauseStartNode(j);
					auto pLiterals2 = _formula.getClause(j);

					for (ID m = 0; m < clauseSize; m++)
					{
						for (ID n = 0; n < clauseSize2; n++)
						{
							if (pLiterals[m] != -pLiterals2[n])
							{
								BitSet(graph[startNode + m].Neighbours, startNode2 + n);
								BitSet(graph[startNode2 + n].Neighbours, startNode + m);
							}
						}
					}
				}
			}

			return graph;
		}
		catch (...)
		{
			FreeGraph(graph);

			return Ext::Array<Vertex>();
		}
	}

}
