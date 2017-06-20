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
#include "Templates.h"
#include "PrivateTypes.h"
#include "MemoryAllocation.h"

#include <stdio.h>


namespace Graph
{
	void NullTextStream(const char* message)
	{
	}

	static TextStream TraceMessage = NullTextStream;

	void SetTraceMessageHandler(TextStream textStream)
	{
		TraceMessage = (textStream != nullptr) ? textStream : NullTextStream;
	}

	void PrintGraph(ID* _list, ID _size, TextStream traceMessage = nullptr)
	{
		if (traceMessage == nullptr)
			return;

		char sz[512];

		for (ID i = 0; (i < _size); i++)
		{
			sprintf_s(sz, sizeof(sz), "%s%d", (i > 0) ? " " : "", _list[i] + 1);
			traceMessage(sz);
		}

		traceMessage("\r\n\r\n");
	}

	void PrintGraph(Ext::Array<Vertex>& _graph, TextStream traceMessage = nullptr)
	{
		if (traceMessage == nullptr)
			return;

		char sz[512];

		for (ID i = 0; (i < _graph.size()); i++)
		{
			sprintf_s(sz, sizeof(sz), "%s%d:%d", (i > 0) ? " " : "", _graph[i].Id + 1, _graph[i].Count);
			traceMessage(sz);
		}

		traceMessage("\r\n\r\n");
	}

	void PrintArray(decltype(Vertex::Id) *_list, size_t _size, TextStream traceMessage = nullptr)
	{
		if (traceMessage == nullptr)
			return;

		char sz[32];
		for (size_t i = 0; (i < _size); i++)
		{
			sprintf_s(sz, sizeof(sz), "%5d ", _list[i]);
			traceMessage(sz);
		}

		traceMessage("\r\n");
	}

	bool HasDuplicate(decltype(Vertex::Id) *_list, size_t _size, size_t _max, byte* _bits)
	{
		ZeroMemoryPack8(_bits, GetQWordAlignedSizeForBits(_max));

		for (size_t i = 0; i < _size; i++)
			if (!BitTest(_bits, _list[i]))
				BitSet(_bits, _list[i]);
			else
				return true;

		return false;
	}

	void assert(bool condition)
	{
		if (!condition)
			throw "Condition failed.";
	}



	#define Swap(a, b, tempStorage) tempStorage = a; a = b; b = tempStorage;

	Ext::Array<Vertex> CreateHardPartitionClique(decltype(Vertex::Id) _graphSize, decltype(Vertex::Id) _cliqueSize)
	{
		Ext::Array<Vertex> graph = CreateGraph(_graphSize);
		auto partitionSize = _graphSize / _cliqueSize;
		auto remainder = _graphSize % _cliqueSize;
		decltype(Vertex::Id) start = 0, end;

		for (decltype(Vertex::Id) i = 0; i < _cliqueSize; i++)
		{
			end = start + partitionSize + ((i < remainder) ? 1 : 0);

			for (auto j = start; j < end; j++)
			{
				for (decltype(Vertex::Id) k = 0; k < start; k++)
				{
					BitSet(graph[j].Neighbours, k);
					BitSet(graph[k].Neighbours, j);
				}

				for (decltype(Vertex::Id) k = end; k < _graphSize; k++)
				{
					BitSet(graph[j].Neighbours, k);
					BitSet(graph[k].Neighbours, j);
				}

				graph[j].Count = _graphSize - end + start;
			}

			start = end;
		}

		return graph;
	}

	namespace Clique
	{
		enum struct PartitionVertexStatus : byte
		{
			ConnectedToAll = 0,				// Connected to all vertices from begining
			Connected = 1,						//	Connected to currently active all; not connected to all at the begining
			ConditionallyConnected = 2,	//	Connected to selected vertices under current multi-partite
			ConditionallyInactive = 3,		// not connected to selected vertices under current multi-partite
			PartialUnverified = 4,			//	potential to become eiter Connected or ConditionallyConnected or ConditionallyInactive or Inactive
			Inactive = 5,						// not connected to any of the active vertices and out of scope now.
		};

		struct SetsOfSet
		{
			struct ElementData
			{
				decltype(Vertex::Id)	OriginalVertexId;
				decltype(Vertex::Id)	VertexId;
				decltype(Vertex::Id)	Attribute;
			};

			decltype(Vertex::Id)	*lCount;
			decltype(Vertex::Id)	*lStartIndex;
			ElementData				*lList;
			decltype(Vertex::Id)	Length;

			inline void InitSet(size_t set)
			{
				lStartIndex[set] = (set == 0) ? 0 : (lStartIndex[set - 1] + lCount[set - 1]);
			};

			inline decltype(Vertex::Id) GetSetSize(size_t set)
			{
				return lCount[set];
			};

			inline void SetSetSize(size_t set, decltype(Vertex::Id) size)
			{
				lCount[set] = size;
			};

			inline ElementData& GetValue(size_t set, size_t idx)
			{
				return lList[lStartIndex[set] + idx];
			};

			inline void SetValue(size_t set, size_t idx, decltype(Vertex::Id) vertexId, decltype(Vertex::Id) originalVertexId, byte attribute)
			{
				ElementData& data = lList[lStartIndex[set] + idx];
				data.VertexId = vertexId;
				data.OriginalVertexId = originalVertexId;
				data.Attribute = attribute;
			};

			forceinline void NewSingleValueSet(size_t set, decltype(Vertex::Id) vertexId, decltype(Vertex::Id) originalVertexId)
			{
				lStartIndex[set] = (set == 0) ? 0 : (lStartIndex[set - 1] + lCount[set - 1]);
				lCount[set] = 1;

				ElementData& data = lList[lStartIndex[set]];
				data.VertexId = vertexId;
				data.OriginalVertexId = originalVertexId;
				data.Attribute = (byte)PartitionVertexStatus::ConnectedToAll;
			}
		};

		struct TryFindCliqueCallFrame
		{
			Ext::Array<Vertex> _graph;
			ID *_originalVertexId;
			decltype(Vertex::Id) _cliqueMembersCount;
			decltype(Vertex::Id) _cliqueSize;
			byte *_targettedVertices;
			decltype(Vertex::Id) _targettedVerticesCount;

			byte *activeVertexList, *activeNeighbours;
			decltype(Vertex::Id)	*vertexId, *vertexEdgeCount;
			decltype(Vertex::Id)	activeVertexCount, cliqueSize, cliqueVertexCount, bitSetLength;
			decltype(Vertex::Id)	subGraphSize, cliqueVertexCount2;

			Int64	callCount, startCallCount;
			bool isCliqueExist;
		};

		const int FramesPerBlock = 5;

		class ResourceManager
		{
		public:
			StackMemoryPool		GraphMemoryPool;
			StackMemoryPool		MemoryPool;
			Int32						*Stack;
			byte						*BitSet, *BitSet2;
			decltype(Vertex::Id)	*lId, *lId2, *lId3;
			bool						Verbose;
			TryFindCliqueCallFrame	*CallFrame;
			decltype(Vertex::Id)	TopGraphForMemoryReclaim;
			decltype(Vertex::Id) CliqueSize;
			decltype(Vertex::Id)	*CliqueMembers;

			UInt64	Calls, TwoNHits, SubgraphHits, BtmUpHits, BtmUpHits2, Count8, BtmUpCheck, BtmUpCheck2, Count9, Count10, Count11, Count12, Count13, Count14;

		public:
			ResourceManager(decltype(Vertex::Id) _graphDegree, bool _verbose, UInt32 _blockSize)
				: MemoryPool(_blockSize), Stack(nullptr), GraphMemoryPool((UInt32) (GetGraphAllocationSize(_graphDegree) * FramesPerBlock))
			{
				size_t allocationSize = sizeof(TryFindCliqueCallFrame) * (_graphDegree / 2 + 2);
				CallFrame = (TryFindCliqueCallFrame*)AllocMemory(allocationSize);
				memset(CallFrame, 0, allocationSize);

				void	*ptr = MemoryPool.Allocate((2 * 32 * sizeof(Int32)) + GetQWordAlignedSizeForBits(_graphDegree) * 2 + GetQWordAlignedSize(_graphDegree * sizeof(ID)) * 4);

				// allocate Bitset2 at the top since we may not be using most of the cases.
				BitSet2 = (byte*)ptr;
				Stack = (Int32*)(((byte*)BitSet2) + GetQWordAlignedSizeForBits(_graphDegree));
				BitSet = (byte*)(((byte*)Stack) + (2 * 32 * sizeof(Int32)));
				lId = decltype(this->lId) (((byte*)BitSet) + GetQWordAlignedSizeForBits(_graphDegree));
				lId2 = decltype(this->lId2) (((byte*)lId) + GetQWordAlignedSize(_graphDegree * sizeof(ID)));
				lId3 = decltype(this->lId3) (((byte*)lId2) + GetQWordAlignedSize(_graphDegree * sizeof(ID)));

				CliqueSize = 0;
				CliqueMembers = decltype(this->CliqueMembers) (((byte*)lId3) + GetQWordAlignedSize(_graphDegree * sizeof(ID)));

				TopGraphForMemoryReclaim = 1;
				Verbose = _verbose;

				ClearCounters();
			}

			~ResourceManager()
			{
				FreeMemory(CallFrame);
				CallFrame = nullptr;

				MemoryPool.Free(BitSet2);
			}

			void ClearCounters()
			{
				Calls = TwoNHits = SubgraphHits = BtmUpHits = BtmUpHits2 = Count8 = BtmUpCheck = BtmUpCheck2 = Count9 = Count10 = Count11 = Count12 = Count13 = Count14 = 0;
			}
		};

		template < class T, class size_type = size_t >
		class CircularQueue
		{
		public:
			CircularQueue(T* ptr, size_type capacity)
				: Ptr(ptr), Capacity(capacity), First(0), Last(0)
			{
			};

			size_type size() const { return Last - First; }
			bool isEmpty() { return (Last == First); }
			void push(T value) { Ptr[Last++ % Capacity] = value; };
			T pop() { return Ptr[First++ % Capacity]; };

		private:
			T* Ptr;
			size_type First, Last, Capacity;
		};
	}


	void PrintCallFramDiagnosticsInfo(decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager, TextStream textStream)
	{
		if (textStream == nullptr)
			return;

		char sz[512];

		sprintf_s(sz, sizeof(sz), "%15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", _resourceManager.Calls, _resourceManager.TwoNHits, _resourceManager.SubgraphHits, _resourceManager.BtmUpHits, _resourceManager.BtmUpHits2, _resourceManager.Count8, _resourceManager.BtmUpCheck, _resourceManager.BtmUpCheck2, _resourceManager.Count9, _resourceManager.Count10, _resourceManager.Count11, _resourceManager.Count12, _resourceManager.Count13, _resourceManager.Count14);
		textStream(sz);

		decltype(Vertex::Id) subGraphSize = 0;
		for (size_t i = 0; (_resourceManager.CallFrame[i].callCount > 0); i++)
		{
			Clique::TryFindCliqueCallFrame &frame = _resourceManager.CallFrame[i];
			sprintf_s(sz, sizeof(sz), "%15d Calls:%15I64d GraphSize:%15d ActiveVertices:%15d CliqueSize[In]:%15d CliqueSize:%15d ParentCliqueSize:%15d\n", 
				(int)i + 1, frame.callCount, (i < _depth) ? (decltype(Vertex::Id))frame._graph.size() : subGraphSize,
				frame.activeVertexCount, frame._cliqueSize, frame.cliqueSize, frame._cliqueMembersCount
			);

			textStream(sz);

			subGraphSize = frame.subGraphSize;
		}
	}

	bool TryFindClique(Ext::Array<Vertex> _graph, ID *_originalVertexId,
				Clique::SetsOfSet& _cliqueMembers, decltype(Vertex::Id) _cliqueMembersCount,
				decltype(Vertex::Id)& _cliqueSize, Clique::FindOperation _op,
				byte* _targettedVertices, decltype(Vertex::Id) _targettedVerticesCount,
				decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager);

	decltype(Vertex::Id) FindClique(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _cliqueSize, Clique::FindOperation _op)
	{
		if (IsCorrupt(_graph))
			throw "invalid graph.";

		if (_graph.size() <= 2)
			return 0;

	  /*
		*	_blockSize:
		*			ResourceManager.m_Stack		:		2 * 32 * sizeof(int);
		*			ResourceManager.m_BitSet,2	:		3 * GetQWordAlignedSizeForBits(graph.size());
		*
		*		[
		*			Ext::Array<Vertex>			:		6 * GetQWORDAlignedSize(graph.size() * sizeof(ID));
		*												:		4 * GetQWordAlignedSizeForBits(graph.size());
		*		] * FramesPerBlock * 2
		*/
		UInt32	bitSetLength = (UInt32)GetQWordAlignedSizeForBits(_graph.size());
		auto		resourceManager = Clique::ResourceManager((decltype(Vertex::Id))_graph.size(), true, (UInt32)(2 * 32 * sizeof(int) + bitSetLength * 3 + (6 * GetQWordAlignedSize(_graph.size() * sizeof(ID)) + 4 * bitSetLength) * Clique::FramesPerBlock * 2));

		Clique::SetsOfSet cliqueMembers;
		ReleaseMemoryToPool dtor(resourceManager.MemoryPool, resourceManager.MemoryPool.Allocate(bitSetLength + GetQWordAlignedSize(_graph.size() * sizeof(ID)) * 3 + GetQWordAlignedSize(sizeof(Clique::SetsOfSet::ElementData) * _graph.size())));
		ReleaseMemoryToPool dtor2(resourceManager.GraphMemoryPool, resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(_graph.size())));

		byte	*targettedVertices = (byte*)dtor.ptr();
		ID		*originalVertexId = (ID*)(targettedVertices + bitSetLength);

		cliqueMembers.Length = (ID)_graph.size();
		cliqueMembers.lCount = (ID*)(((byte*)originalVertexId) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		cliqueMembers.lStartIndex = (ID*)(((byte*)cliqueMembers.lCount) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		cliqueMembers.lList = (Clique::SetsOfSet::ElementData*)(((byte*)cliqueMembers.lStartIndex) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		void* pGraphMemory = dtor2.ptr();

		decltype(Vertex::Id) i;

		char sz[512];
		sprintf_s(sz, sizeof(sz), "%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n", "Vertices", "Clique", "Clique-R", "Ticks", "Calls", "TwoNHits", "SubgraphHits", "BtmUpHits", "BtmUpHits2", "Count8", "BtmUpCheck", "BtmUpCheck2", "Count9", "Count10", "Count11", "Count12", "Count13", "Count14");
		TraceMessage(sz);

		for (i = 0; i < _graph.size(); i++)
			originalVertexId[i] = i;

		auto graph = CloneGraph(_graph, pGraphMemory);

		// make sure that there is an edge connecting each vertex to itself.
		for (i = 0; i < graph.size(); i++)
		{
			if (!BitTest(graph[i].Neighbours, i))
			{
				BitSet(graph[i].Neighbours, i);
				graph[i].Count++;
			}
		}

		ZeroMemoryPack8(targettedVertices, bitSetLength);

		ZeroMemoryPack8(cliqueMembers.lCount, GetQWordAlignedSize(sizeof(ID) * cliqueMembers.Length));
		ZeroMemoryPack8(cliqueMembers.lStartIndex, GetQWordAlignedSize(sizeof(ID) * cliqueMembers.Length));
		ZeroMemoryPack8(cliqueMembers.lList, GetQWordAlignedSize(sizeof(Clique::SetsOfSet::ElementData) * _graph.size()));

		resourceManager.CliqueSize = 0;
		ZeroMemoryPack8(resourceManager.CliqueMembers, GetQWordAlignedSize(sizeof(ID) * _graph.size()));

		resourceManager.ClearCounters();
		resourceManager.TopGraphForMemoryReclaim = 1;

		auto ticks = GetCurrentTick();
		auto cliqueSize = (_cliqueSize == INVALID_ID) ? 3 : _cliqueSize;

		if (!TryFindClique(graph, originalVertexId, cliqueMembers, 0, cliqueSize, _op, targettedVertices, 0, 0, resourceManager))
		{
			cliqueSize = (cliqueSize > ((_cliqueSize == INVALID_ID) ? 3 : _cliqueSize)) ? (cliqueSize - 1) : 0;
			Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(resourceManager.CliqueMembers, nullptr, 0, cliqueSize, true, resourceManager.Stack);

			sprintf_s(sz, sizeof(sz), "CliqueSize: %d\r\n", cliqueSize);
			TraceMessage(sz);
			PrintArray(resourceManager.CliqueMembers, cliqueSize, TraceMessage);

			assert(IsClique(_graph, resourceManager.CliqueMembers, cliqueSize, resourceManager.BitSet));
		}

		sprintf_s(sz, sizeof(sz), "%15d %15d %15d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", (int)graph.size(), cliqueSize, _cliqueSize, GetCurrentTick() - ticks, resourceManager.Calls, resourceManager.TwoNHits, resourceManager.SubgraphHits, resourceManager.BtmUpHits, resourceManager.BtmUpHits2, resourceManager.Count8, resourceManager.BtmUpCheck, resourceManager.BtmUpCheck2, resourceManager.Count9, resourceManager.Count10, resourceManager.Count11, resourceManager.Count12, resourceManager.Count13, resourceManager.Count14);
		TraceMessage(sz);

		for (i = 0; (resourceManager.CallFrame[i].callCount > 0); i++)
		{
			sprintf_s(sz, sizeof(sz), "%15d Calls:%15I64d\n", (int)i + 1, resourceManager.CallFrame[i].callCount);
			TraceMessage(sz);
		}

		return cliqueSize;
	}

	decltype(Vertex::Id) FindVertextColor(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _cliqueSize)
	{
		if (IsCorrupt(_graph))
			throw "invalid graph.";

		if ((_graph.size() <= 2) || (_cliqueSize < 3))
			return 0;

		/*
		*	_blockSize:
		*			ResourceManager.m_Stack		:		2 * 32 * sizeof(int);
		*			ResourceManager.m_BitSet	:		2 * GetQWordAlignedSizeForBits(graph.size());
		*
		*		[
		*			Ext::Array<Vertex>			:		GetGraphAllocationSize(_graph.size());
		*												:		5 * GetQWORDAlignedSize(graph.size() * sizeof(ID));
		*												:		4 * GetQWordAlignedSizeForBits(graph.size());
		*		] * 5
		*/
		UInt32	bitSetLength = (UInt32)GetQWordAlignedSizeForBits(_graph.size());
		auto		resourceManager = Clique::ResourceManager((decltype(Vertex::Id))_graph.size(), true, (UInt32)(2 * 32 * sizeof(int) + bitSetLength * 2 + (GetGraphAllocationSize(_graph.size()) + 5 * GetQWordAlignedSize(_graph.size() * sizeof(ID)) + 4 * bitSetLength) * 5));

		Clique::SetsOfSet cliqueMembers;
		ReleaseMemoryToPool dtor(resourceManager.MemoryPool, resourceManager.MemoryPool.Allocate(GetQWordAlignedSize(_graph.size() * sizeof(ID)) * 5 + GetQWordAlignedSize(sizeof(Clique::SetsOfSet::ElementData) * _graph.size()) + bitSetLength * 2 + GetGraphAllocationSize(_graph.size())));

		byte	*targettedVertices = (byte*)dtor.ptr();
		ID		*originalVertexId = (ID*)(targettedVertices + bitSetLength);
		ID		*vertexMaxClique = (ID*)(((byte*)originalVertexId) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		byte	*activeNeighbours = (((byte*)vertexMaxClique) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));

		cliqueMembers.Length = (ID)_graph.size();
		cliqueMembers.lCount = (ID*)(((byte*)activeNeighbours) + bitSetLength);
		cliqueMembers.lStartIndex = (ID*)(((byte*)cliqueMembers.lCount) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		cliqueMembers.lList = (Clique::SetsOfSet::ElementData*)(((byte*)cliqueMembers.lStartIndex) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		void	*pGraphMemory = (((byte*)cliqueMembers.lList) + GetQWordAlignedSize(sizeof(Clique::SetsOfSet::ElementData) * _graph.size()));
		ID		*vertexColor = (ID*)((byte*)pGraphMemory + GetGraphAllocationSize(_graph.size()));

		decltype(Vertex::Id) i, j, color = 0;

		for (i = 0; i < _graph.size(); i++)
		{
			vertexMaxClique[i] = (decltype(Vertex::Id))_graph.size();
			vertexColor[i] = 0;
		}

		char sz[512];
		sprintf_s(sz, sizeof(sz), "%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n", "Vertices", "Clique", "Ticks", "Calls", "TwoNHits", "SubgraphHits", "BtmUpHits", "BtmUpHits2", "Count8", "BtmUpCheck", "BtmUpCheck2", "Count9", "Count10", "Count11", "Count12", "Count13", "Count14");
		TraceMessage(sz);

		SetNBits((UInt64*)activeNeighbours, _graph.size());

		for (decltype(Vertex::Id) cliqueSize = _cliqueSize; cliqueSize > 0; cliqueSize--)
		while (true)
		{
			for (i = 0, j = 0; i < _graph.size(); i++)
			{
				if (BitTest(activeNeighbours, i))
					originalVertexId[j++] = i;
			}

			auto graph = CreateGraph(j, pGraphMemory);
			ExtractGraph(_graph, graph, activeNeighbours, resourceManager.BitSet);
			Graph::ComplementGraph(graph, graph);

			// make sure that there is an edge connecting each vertex to itself.
			for (i = 0; i < graph.size(); i++)
			{
				if (!BitTest(graph[i].Neighbours, i))
				{
					BitSet(graph[i].Neighbours, i);
					graph[i].Count++;
				}
			}

			ZeroMemoryPack8(targettedVertices, bitSetLength);

			ZeroMemoryPack8(cliqueMembers.lCount, GetQWordAlignedSize(sizeof(ID) * cliqueMembers.Length));
			ZeroMemoryPack8(cliqueMembers.lStartIndex, GetQWordAlignedSize(sizeof(ID) * cliqueMembers.Length));
			ZeroMemoryPack8(cliqueMembers.lList, GetQWordAlignedSize(sizeof(Clique::SetsOfSet::ElementData) * _graph.size()));

			resourceManager.CliqueSize = 0;
			ZeroMemoryPack8(resourceManager.CliqueMembers, GetQWordAlignedSize(sizeof(ID) * _graph.size()));

			resourceManager.ClearCounters();
			resourceManager.TopGraphForMemoryReclaim = 1;

			if (TryFindClique(graph, originalVertexId, cliqueMembers, 0, cliqueSize, Clique::FindOperation::ExactSearch, targettedVertices, 0, 0, resourceManager))
			{
				color++;

				for (i = 0; i < cliqueSize; i++)
				{
					auto id = cliqueMembers.GetValue(i, 0).OriginalVertexId;
					resourceManager.lId[i] = id;
					assert(BitTest(activeNeighbours, id));
					BitReset(activeNeighbours, id);
					vertexColor[id] = color;
				}

				Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(resourceManager.lId, nullptr, 0, cliqueSize, true, resourceManager.Stack);
				PrintArray(resourceManager.lId, cliqueSize, TraceMessage);

				assert(IsIndependantSet(_graph, resourceManager.lId, cliqueSize, resourceManager.BitSet));
			}
			else
				break;
		}

		assert(IsProperColor(_graph, vertexColor));

		return color;
	}

#define TryFindClique_Recursion1

	bool TryFindClique(Ext::Array<Vertex> _graph, ID *_originalVertexId,
				Clique::SetsOfSet& _cliqueMembers, decltype(Vertex::Id) _cliqueMembersCount,
				decltype(Vertex::Id)& _cliqueSize, Clique::FindOperation _op,
				byte* _targettedVertices, decltype(Vertex::Id) _targettedVerticesCount,
				decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager)
	{
		byte *activeVertexList, *activeNeighbours, *pActiveNeighbours;
		decltype(Vertex::Id)	*vertexId, *vertexEdgeCount, *ids;
		decltype(Vertex::Id)	activeVertexCount, cliqueSize, cliqueVertexCount, bitSetLength;
		decltype(Vertex::Id)	id, i, j, k, l, n;
		bool isCliqueExist;
		Clique::TryFindCliqueCallFrame *frame;

		Ext::Array<Vertex> graph;
		auto ticks = GetCurrentTick();

	Enter:

		bitSetLength = (decltype(Vertex::Id))GetQWordAlignedSizeForBits(_graph.size());

		activeVertexList = (byte*) _resourceManager.MemoryPool.Allocate(bitSetLength * 2 + GetQWordAlignedSize(_graph.size() * sizeof(ID)) * 2);
		activeNeighbours = activeVertexList + bitSetLength;
		vertexId = (decltype(Vertex::Id)*)(activeNeighbours + bitSetLength);
		vertexEdgeCount = (decltype(Vertex::Id)*)((byte*)vertexId + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		isCliqueExist = false;
		cliqueVertexCount = 0;

		frame = &_resourceManager.CallFrame[_depth];
		frame->_cliqueMembersCount = _cliqueMembersCount;
		frame->_cliqueSize = _cliqueSize;
		frame->_originalVertexId = _originalVertexId;
		frame->_graph = _graph;
		frame->_targettedVertices = _targettedVertices;
		frame->_targettedVerticesCount = _targettedVerticesCount;
		frame->callCount++;
		frame->startCallCount = _resourceManager.Calls++;

		// Sort vertices in DESC order based on vertex degree
		for (i = 0; i < _graph.size(); i++)
		{
			// assert(i == _graph[i].Id);
			vertexId[i] = _graph[i].Id;
			vertexEdgeCount[i] = _graph[i].Count;
		}

		Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(vertexEdgeCount, vertexId, 0, (decltype(Vertex::Id))_graph.size(), false, _resourceManager.Stack);

		// start with all vertices as active.
		SetNBits((UInt64*)activeVertexList, _graph.size());

		cliqueSize = _cliqueSize;

		for (activeVertexCount = (decltype(i))_graph.size(); (0 < activeVertexCount) && (cliqueSize <= activeVertexCount); )
		{
#pragma region Top down processing
			// Top down processing only when all remaining vertices have at least degree of clique-size
			auto cliqueVertexCountAtStart = cliqueVertexCount;

			while ((0 < activeVertexCount) && (cliqueSize <= activeVertexCount) && (cliqueSize <= vertexEdgeCount[activeVertexCount - 1]))
			{
				k = activeVertexCount - vertexEdgeCount[0];
				id = vertexId[0];
				_cliqueMembers.InitSet(_cliqueMembersCount + cliqueVertexCount);
				_cliqueMembers.SetValue(_cliqueMembersCount + cliqueVertexCount, 0, id, _originalVertexId[id], (byte)Clique::PartitionVertexStatus::ConnectedToAll);

				ids = _resourceManager.lId;
				auto idx = _resourceManager.lId2;

				pActiveNeighbours = _graph[id].Neighbours;
				idx[0] = 0;
				ids[0] = id;

				for (i = 1, j = 1, l = 0; (i < activeVertexCount) && (j <= k); i++)
				{
					if (BitTest(pActiveNeighbours, vertexId[i]))
						continue;

					idx[j] = i;
					ids[j] = vertexId[i];

					n = (byte)Clique::PartitionVertexStatus::PartialUnverified;
					if (vertexEdgeCount[i] == vertexEdgeCount[0])
					{
						l = j;
						n = (byte)Clique::PartitionVertexStatus::ConnectedToAll;
					}

					// A vertex with ((vertexEdgeCount[i] < vertexEdgeCount[0])) is not connected to rest of the vertices, still it could be part of the clique.
					_cliqueMembers.SetValue(_cliqueMembersCount + cliqueVertexCount, j, vertexId[i], _originalVertexId[vertexId[i]], n);
					j++;
				}

				n = l + 1;
				_cliqueMembers.SetSetSize(_cliqueMembersCount + cliqueVertexCount, j);
				if (j < _graph.size())
					idx[j] = INVALID_ID;

				for (i = 2; (i <= k); i++)
				{
					pActiveNeighbours = _graph[ids[i]].Neighbours;
					for (j = 1; (j < i) && !BitTest(pActiveNeighbours, ids[j]); j++);
					if (j < i)
						goto LoopExit;
				}

				for (i = 0, j = 0, l = 0; j < activeVertexCount; j++)
				{
					if (j == idx[l])
						l++;
					else
					{
						vertexEdgeCount[i] = vertexEdgeCount[j] - n;
						vertexId[i] = vertexId[j];
						i++;
					}
				}

				id = k + 1;
				l = _targettedVerticesCount;

				for (j = 0; (j <= k); j++, i++)
				{
					vertexId[i] = ids[j];
					vertexEdgeCount[i] = INVALID_ID;

					if ((_targettedVerticesCount > 0) && BitTest(_targettedVertices, ids[j]))
						_targettedVerticesCount--;
					else if (id > k)
						id = j;
				}

				activeVertexCount -= (k + 1);
				cliqueVertexCount++;
				if (0 < cliqueSize)
					cliqueSize--;

				if (l > _targettedVerticesCount)
				{
					// (id < n) : There exist a non targetted vertex in this partition which is connected to rest of the graph.
					// (id >= n) : all ConnectedToAll vertices in this partition are targetted vertices.
					if ((_targettedVerticesCount == 0) && (id < n))
					{
						goto ExitOutermostLoop;	// done processing this graph
					}
					else if ((_targettedVerticesCount > 0) && (id >= n))
						_targettedVerticesCount = 0; // targetted vertices are connected to rest of the subgraph.
				}

				for (i = 0; i <= k; i++)
					BitReset(activeVertexList, ids[i]);

				for (l = n; l <= k; l++)
				{
					id = ids[l];
					pActiveNeighbours = _graph[id].Neighbours;
					for (i = activeVertexCount; i-- > 0; )
					{
						if (BitTest(pActiveNeighbours, vertexId[i]))
							vertexEdgeCount[i]--;
					}

					for (i = j = 0; (j < activeVertexCount); j++)
					{
						if (vertexEdgeCount[i] < vertexEdgeCount[j])
						{
							Swap(vertexEdgeCount[i], vertexEdgeCount[j], n);
							Swap(vertexId[i], vertexId[j], n);
							i++;
						}
						else if (vertexEdgeCount[i] > vertexEdgeCount[j])
							i = j;
					}
				}
			}
		LoopExit:
#pragma endregion

			if (activeVertexCount < cliqueSize)
				break;
			else if (activeVertexCount == 0)
			{
				if (cliqueSize == 0)
				{
					if (_op == Clique::FindOperation::EnumerateCliques)
					{
						// Invoke callback to process current clique set.
					}
					else if ((_cliqueMembersCount + cliqueVertexCount) > _resourceManager.CliqueSize)
					{
						_resourceManager.CliqueSize = _cliqueMembersCount + cliqueVertexCount;
						for (i = 0; i < _resourceManager.CliqueSize; i++)
							_resourceManager.CliqueMembers[i] = _cliqueMembers.GetValue(i, 0).OriginalVertexId;

						if (_op == Clique::FindOperation::MaximumClique)
							cliqueSize++; // Lets look for the next larger clique.
					}
				}

				break;
			}


#pragma region Check whether clique is possible
			if (activeVertexCount < (cliqueSize << 1))		// (activeVertexCount < (2 * cliqueSize))
			{
				size_t edgeTotal = 0, edgeTotal2 = 0;
				for (i = 0; i < cliqueSize; i++)
					edgeTotal += vertexEdgeCount[i];

				for (; i < activeVertexCount; i++)
					edgeTotal2 += vertexEdgeCount[i];

				i = activeVertexCount - cliqueSize;
				if ((edgeTotal - (cliqueSize * cliqueSize)) < (edgeTotal2 - (i * i)))
				{
					_resourceManager.TwoNHits++;
					break;
				}
			}
#pragma endregion

			id = vertexId[activeVertexCount - 1];
			auto subCliqueSize = cliqueSize;
			auto cliqueVertexCount2 = (decltype(cliqueVertexCount))0;
			auto subGraphSize = (decltype(Vertex::Id)) PopCountAandB_Set((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)activeNeighbours, (bitSetLength >> 3));
			decltype(Vertex::Id) id2;

			ids = _resourceManager.lId;
			for (bool dirty = true; dirty && (0 < subGraphSize) && (subCliqueSize <= subGraphSize); )
			{
				decltype(Vertex::Id) processedCount = 0;

				dirty = false;
				for (i = activeVertexCount; i-- > 0; )
				{
					id = vertexId[i];
					if (!BitTest(activeNeighbours, id))
						continue;

					auto count = (decltype(subGraphSize))PopCountAandB((UInt64*)activeNeighbours, (UInt64*)_graph[id].Neighbours, (bitSetLength >> 3));
					if (count < subCliqueSize)
					{
						BitReset(activeNeighbours, id);
						subGraphSize--;
						dirty |= (processedCount > 0);
					}
					else if (count == subGraphSize)
					{
						BitReset(activeNeighbours, id);
						subGraphSize--;
						if (0 < subCliqueSize)
							subCliqueSize--;

						// _cliqueMembers.NewSingleValueSet(_cliqueMembersCount + cliqueVertexCount + cliqueVertexCount2, id, _originalVertexId[id]);
						ids[cliqueVertexCount2] = id;
						cliqueVertexCount2++;
					}
					else
						processedCount++;
				}
			}

			auto isExist = (subCliqueSize <= subGraphSize);
			if ((cliqueVertexCount2 > 0) && isExist)
			{
				id = ids[0];	// vertexId[activeVertexCount - 1];
				_cliqueMembers.NewSingleValueSet(_cliqueMembersCount + cliqueVertexCount, id, _originalVertexId[id]);

				n = vertexEdgeCount[activeVertexCount - 1] - 1;
				if (n == subGraphSize)
					pActiveNeighbours = activeNeighbours;
				else
				{
					pActiveNeighbours = _resourceManager.BitSet;
					AandB((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)pActiveNeighbours, (bitSetLength >> 3));
					BitReset(pActiveNeighbours, id);
				}

				for (j = 1, i = activeVertexCount - 1; (i-- > 0) && (vertexEdgeCount[i] == (n + 1)); )
				{
					id2 = vertexId[i];
					if (BitTest(pActiveNeighbours, id2))
						continue;

					if (PopCountAandB((UInt64*)pActiveNeighbours, (UInt64*)_graph[id2].Neighbours, (bitSetLength >> 3)) == n)
						_cliqueMembers.SetValue(_cliqueMembersCount + cliqueVertexCount, j++, id2, _originalVertexId[id2], (byte)Clique::PartitionVertexStatus::ConnectedToAll);
				}

				_cliqueMembers.SetSetSize(_cliqueMembersCount + cliqueVertexCount, j);

				for (i = 1, j = _cliqueMembersCount + cliqueVertexCount + i; i < cliqueVertexCount2; i++, j++)
					_cliqueMembers.NewSingleValueSet(j, ids[i], _originalVertexId[ids[i]]);
			}

			if ((_targettedVerticesCount > 0) && isExist && (0 < subGraphSize))
			{
				j = 0;
				for (i = 1; i < cliqueVertexCount2; i++)
					j |= _targettedVertices[ids[i] >> 3] & Bit::BIT[ids[i] & 0x07];

				if (j == 0)
				{
					n = _cliqueMembersCount + cliqueVertexCount;
					for (i = 0, k = _cliqueMembers.GetSetSize(n); (i < k) && BitTest(_targettedVertices, _cliqueMembers.GetValue(n, i).VertexId); i++);
					if ((i != k) && (PopCountAandB((UInt64*)_targettedVertices, (UInt64*)activeNeighbours, (bitSetLength >> 3)) == 0))
					{
						subGraphSize = 0;	// skip; no targetted vertices has been found in this subgraph.
						isExist = false;
					}
				}
			}

			cliqueVertexCount2 += cliqueVertexCount;

			_resourceManager.SubgraphHits++;

			if ((0 < subGraphSize) && (subCliqueSize <= subGraphSize) && ((0 < subCliqueSize) || (_op != Clique::FindOperation::ExactSearch)))
			{
				isExist = false;
				id = vertexId[activeVertexCount - 1];

				decltype(Vertex::Id) commonCount = 0, activeNeighboursCount = subGraphSize + (cliqueVertexCount2 - cliqueVertexCount);
				decltype(Vertex::Id) commonCountMax = 0, commonCountMaxId = INVALID_ID;

				if ((cliqueVertexCountAtStart < cliqueVertexCount) || ((subGraphSize + (cliqueVertexCount2 - cliqueVertexCount)) < vertexEdgeCount[activeVertexCount - 1]))
				{
					_resourceManager.BtmUpCheck++;

					pActiveNeighbours = _resourceManager.BitSet;
					CopyMemoryPack8(pActiveNeighbours, activeNeighbours, bitSetLength);
					for (i = 0; i < (cliqueVertexCount2 - cliqueVertexCount); i++)
						BitSet(pActiveNeighbours, ids[i]);

					for (commonCount = 0, l = cliqueVertexCount, i = activeVertexCount; (i < _graph.size()) && (commonCount < activeNeighboursCount); i++)
					{
						id2 = vertexId[i];
						if (vertexEdgeCount[i] != INVALID_ID)
						{
							auto neighbours = _graph[id2].Neighbours;
							commonCount = (decltype(Vertex::Id))PopCountAandB((UInt64*)pActiveNeighbours, (UInt64*)neighbours, (bitSetLength >> 3));

							commonCount += BitTest(neighbours, id) ? 0 : 1;
							if (commonCount == activeNeighboursCount)
							{
								for (j = l, n = 0; j < cliqueVertexCount; j++)
									for (k = 0; k < _cliqueMembers.GetSetSize(j); k++)
										if ((_cliqueMembers.GetValue(j, k).Attribute == (byte)Clique::PartitionVertexStatus::ConnectedToAll) && BitTest(neighbours, _cliqueMembers.GetValue(j, k).VertexId))
										{
											n++;
											break;
										}

								if (n < (cliqueVertexCount - l))
									commonCount = 0;

								_resourceManager.BtmUpCheck2++;
							}

							if (commonCount > commonCountMax)
							{
								commonCountMax = commonCount;
								commonCountMaxId = id2;
							}
						}
						else if (vertexEdgeCount[i - 1] != INVALID_ID) // Once per partition; during cross-over
							l--;
					}

					if (commonCount == activeVertexCount) // no more cliques in this graph.
						break;
				}

				if (commonCount < activeNeighboursCount)
				{
					auto targettedVertices = (byte*)_resourceManager.MemoryPool.Allocate(GetQWordAlignedSize(subGraphSize * sizeof(ID)) + GetQWordAlignedSizeForBits(subGraphSize));
					auto originalVertexId = (decltype(Vertex::Id)*)(targettedVertices + GetQWordAlignedSizeForBits(subGraphSize));
					auto ptr = _resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(subGraphSize));
					if (ptr == nullptr)
					{
						i = _resourceManager.TopGraphForMemoryReclaim++;
						_resourceManager.GraphMemoryPool.Free(_resourceManager.CallFrame[i]._graph.ptr());
						_resourceManager.CallFrame[i]._graph = Ext::Array<Graph::Vertex>(nullptr, _resourceManager.CallFrame[i]._graph.size());
						ptr = _resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(subGraphSize));
					}

					graph = CreateGraph(subGraphSize, ptr);
					ExtractGraph(_graph, graph, activeNeighbours, _resourceManager.BitSet);

					for (j = 0, i = 0; i < _graph.size(); i++)
					{
						if (BitTest(activeNeighbours, i))
							originalVertexId[j++] = _originalVertexId[i];
					}

					decltype(Vertex::Id) targettedVerticesCount;
					if ((_targettedVerticesCount == 0) && (commonCountMaxId == INVALID_ID))
					{
						ZeroMemoryPack8(targettedVertices, GetQWordAlignedSizeForBits(subGraphSize));
						targettedVerticesCount = 0;
					}
					else if (_targettedVerticesCount > 0)
						targettedVerticesCount = (decltype(Vertex::Id))ExtractBits(_targettedVertices, targettedVertices, activeNeighbours, _resourceManager.BitSet, _graph.size());
					else // if (commonCountMaxId != INVALID_ID)
						targettedVerticesCount = (decltype(Vertex::Id))ExtractBitsFromComplement(_graph[commonCountMaxId].Neighbours, targettedVertices, activeNeighbours, _resourceManager.BitSet, _graph.size());


					frame->activeVertexList = activeVertexList;
					frame->activeNeighbours = activeNeighbours;
					frame->vertexId = vertexId;
					frame->vertexEdgeCount = vertexEdgeCount;
					frame->activeVertexCount = activeVertexCount;
					frame->cliqueSize = cliqueSize;
					frame->cliqueVertexCount = cliqueVertexCount;
					frame->bitSetLength = bitSetLength;
					frame->subGraphSize = subGraphSize;
					frame->cliqueVertexCount2 = cliqueVertexCount2;
					frame->isCliqueExist = isCliqueExist;

					// Done at the top of the function on enter.
					//frame->_targettedVertices = _targettedVertices;
					//frame->_targettedVerticesCount = _targettedVerticesCount;
					//frame->_cliqueMembersCount = _cliqueMembersCount;
					//frame->_cliqueSize = _cliqueSize;
					//frame->_originalVertexId = _originalVertexId;
					//frame->_graph = _graph;

#if (defined(TryFindClique_Recursion))
					isExist = TryFindClique(graph, originalVertexId, _cliqueMembers, _cliqueMembersCount + cliqueVertexCount2, subCliqueSize, _op, targettedVertices, targettedVerticesCount, _depth + 1, _resourceManager);

					// Possible memory reuse for graph allocation.
					graph = _resourceManager.CallFrame[_depth + 1]._graph;
					_graph = frame->_graph;
#else
#pragma region Push CallFrame

					_targettedVertices = targettedVertices;
					_targettedVerticesCount = targettedVerticesCount;
					_cliqueSize = subCliqueSize;
					_cliqueMembersCount = _cliqueMembersCount + cliqueVertexCount2;
					_originalVertexId = originalVertexId;
					_graph = graph;
					_depth++;

					goto Enter;
#pragma endregion

#pragma region Pop CallFrame
				ReturnTo:
					frame = &_resourceManager.CallFrame[--_depth];

					isExist = isCliqueExist;
					graph = _graph;
					originalVertexId = _originalVertexId;
					subCliqueSize = _cliqueSize;
					targettedVertices = _targettedVertices;

					_targettedVertices = frame->_targettedVertices;
					_targettedVerticesCount = frame->_targettedVerticesCount;
					_cliqueMembersCount = frame->_cliqueMembersCount;
					_cliqueSize = frame->_cliqueSize;
					_originalVertexId = frame->_originalVertexId;
					_graph = frame->_graph;

					isCliqueExist = frame->isCliqueExist;
					activeVertexList = frame->activeVertexList;
					activeNeighbours = frame->activeNeighbours;
					vertexId = frame->vertexId;
					vertexEdgeCount = frame->vertexEdgeCount;
					activeVertexCount = frame->activeVertexCount;
					cliqueSize = frame->cliqueSize;
					cliqueVertexCount = frame->cliqueVertexCount;
					bitSetLength = frame->bitSetLength;
					subGraphSize = frame->subGraphSize;
					cliqueVertexCount2 = frame->cliqueVertexCount2;
#pragma endregion
#endif
					_resourceManager.MemoryPool.Free(targettedVertices);
					_resourceManager.GraphMemoryPool.Free(graph.ptr());

					// If the graph memory is taken for sub-graph storage, recreate graph from input graph.
					if (_graph.ptr() == nullptr)
					{
						// assert(_depth > 0); // The input graph at depth 0 should never be touched.
						frame = &_resourceManager.CallFrame[0];
						pActiveNeighbours = _resourceManager.BitSet2;
						ZeroMemoryPack8(pActiveNeighbours, frame->bitSetLength);
						for (i = 0; i < _graph.size(); i++)
							BitSet(pActiveNeighbours, _originalVertexId[i]);

						_graph = CreateGraph((decltype(id))_graph.size(), _resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(_graph.size())));
						ExtractGraph(frame->_graph, _graph, pActiveNeighbours, _resourceManager.BitSet);

						_resourceManager.TopGraphForMemoryReclaim = _depth;
					}
				}
				else
				{
					// Subbu: Check whether this vertex forms clique or not.
					_resourceManager.BtmUpHits2++;
				}
			}

			//if (cliqueSize < ((cliqueVertexCount2 - cliqueVertexCount) + subCliqueSize))
				cliqueSize = (cliqueVertexCount2 - cliqueVertexCount) + subCliqueSize;

			if (isExist)
			{
				if (subGraphSize == 0)
				{
					if (_op == Clique::FindOperation::EnumerateCliques)
					{
						// Invoke callback to process current clique set.
					}
					else if ((_cliqueMembersCount + cliqueVertexCount2) > _resourceManager.CliqueSize)
					{
						_resourceManager.CliqueSize = _cliqueMembersCount + cliqueVertexCount2;
						for (i = 0; i < _resourceManager.CliqueSize; i++)
							_resourceManager.CliqueMembers[i] = _cliqueMembers.GetValue(i, 0).OriginalVertexId;

						if (_op == Clique::FindOperation::MaximumClique)
						{
							cliqueSize++; // Lets look for the next larger clique.
							isExist = false;
						}
					}
				}

				isCliqueExist = isExist;
				if (_op == Clique::FindOperation::ExactSearch)
					goto Return; // isCliqueExist can't be set to false after this since it is true now. Note: Refer before Return: label. 
			}


#pragma region Remove processed vertex and similar subgraphs
			Clique::CircularQueue<decltype(Vertex::Id)> queue(_resourceManager.lId2, _graph.size());
			byte *queuedVertices = _resourceManager.BitSet;

			ZeroMemoryPack8(queuedVertices, bitSetLength);

			if ((_targettedVerticesCount > 0) && BitTest(_targettedVertices, vertexId[activeVertexCount - 1]) && (--_targettedVerticesCount == 0))
			{
				goto ExitOutermostLoop;	// done processing this graph
			}

			while ((0 < activeVertexCount) && (cliqueSize <= --activeVertexCount))
			{
				id = vertexId[activeVertexCount];

				subGraphSize = vertexEdgeCount[activeVertexCount];
				vertexEdgeCount[activeVertexCount] = (cliqueVertexCount + cliqueSize) - (isExist ? 0 : 1);
				AandB((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)activeNeighbours, (bitSetLength >> 3));
				// assert(subGraphSize, PopCount((UInt64*)activeNeighbours, (bitSetLength >> 3)));
				BitReset(activeVertexList, id);

				if ((subGraphSize - 1) == activeVertexCount)
				{
					activeVertexCount = 0;	// Current vertex is connected with remaining all active vertices.
					break;
				}

				for (i = activeVertexCount; (i-- > 0) && (vertexEdgeCount[i] <= (subGraphSize + 1)); )
				{
					id2 = vertexId[i];
					bool isConnected = BitTest(activeNeighbours, id2);
					if (BitTest(queuedVertices, id2))
						continue;

					n = (decltype(Vertex::Id))PopCountAandB((UInt64*)activeNeighbours, (UInt64*)_graph[id2].Neighbours, (bitSetLength >> 3));
					if ((n == vertexEdgeCount[i]) || (((isConnected || (subGraphSize == 1)) ? subGraphSize : (subGraphSize - 1)) == n))
					{
						queue.push(id2);
						BitSet(queuedVertices, id2);
						_resourceManager.BtmUpHits++;

						if ((_targettedVerticesCount > 0) && BitTest(_targettedVertices, id2) && (--_targettedVerticesCount == 0))
						{
							goto ExitOutermostLoop;	// done processing this graph
						}
					}
				}

				if ((activeVertexCount + 1 - queue.size()) < cliqueSize)
				{
					activeVertexCount -= (decltype(Vertex::Id))queue.size();
					break;
				}

				for (i = activeVertexCount; i-- > 0; )
				{
					if (BitTest(activeNeighbours, vertexId[i]))
						vertexEdgeCount[i]--;
				}

				for (i = j = 0; (j < activeVertexCount); j++)
				{
					if (vertexEdgeCount[i] < vertexEdgeCount[j])
					{
						Swap(vertexEdgeCount[i], vertexEdgeCount[j], n);
						Swap(vertexId[i], vertexId[j], n);
						i++;
					}
					else if (vertexEdgeCount[i] > vertexEdgeCount[j])
						i = j;
				}

				if (queue.size() > 0)
				{
					id = queue.pop();
					BitReset(queuedVertices, id);

					for (i = activeVertexCount; (i-- > 0) && (vertexId[i] != id); );
					n = vertexEdgeCount[i];

					while (++i < activeVertexCount)
					{
						vertexId[i - 1] = vertexId[i];
						vertexEdgeCount[i - 1] = vertexEdgeCount[i];
					}

					vertexId[i - 1] = id;
					vertexEdgeCount[i - 1] = n;
				}
				else if (vertexEdgeCount[activeVertexCount - 1] >= cliqueSize)
					break;
			}
#pragma endregion

			if (_depth == 0)
			{
				char sz[512];
				sprintf_s(sz, sizeof(sz), "%15d %15d %15d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", (int)graph.size(), cliqueSize, _cliqueSize, GetCurrentTick() - ticks, _resourceManager.Calls, _resourceManager.TwoNHits, _resourceManager.SubgraphHits, _resourceManager.BtmUpHits, _resourceManager.BtmUpHits2, _resourceManager.Count8, _resourceManager.BtmUpCheck, _resourceManager.BtmUpCheck2, _resourceManager.Count9, _resourceManager.Count10, _resourceManager.Count11, _resourceManager.Count12, _resourceManager.Count13, _resourceManager.Count14);
				TraceMessage(sz);

				for (i = 0; (_resourceManager.CallFrame[i].callCount > 0); i++)
				{
					sprintf_s(sz, sizeof(sz), "%15d Calls:%15I64d\n", (int)i + 1, _resourceManager.CallFrame[i].callCount);
					TraceMessage(sz);
				}
			}
		}
	ExitOutermostLoop:

		if (_op != Clique::FindOperation::MaximumClique)
			isCliqueExist |= ((cliqueVertexCount >= _cliqueSize) || ((cliqueVertexCount + cliqueSize) > _cliqueSize));

	Return:
		if (_cliqueSize < (cliqueVertexCount + cliqueSize))
			_cliqueSize = (cliqueVertexCount + cliqueSize);

		_resourceManager.MemoryPool.Free(activeVertexList);

#if (!defined(TryFindClique_Recursion))
		if (_depth > 0)
			goto ReturnTo;
#endif

		return isCliqueExist;
	}


	bool IsClique(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_cliqueMembers, decltype(Vertex::Id) _cliqueSize, byte* _bitset)
	{
		decltype(Vertex::Id) i;
		auto bitSetLength = (decltype(Vertex::Id))GetQWordAlignedSizeForBits(_graph.size());

		ZeroMemoryPack8(_bitset, bitSetLength);
		for (i = 0; i < _cliqueSize; i++)
		{
			if (BitTest(_bitset, _cliqueMembers[i]))
				return false;	// invalidArgument : duplicate found in _cliqueMembers

			BitSet(_bitset, _cliqueMembers[i]);
		}

		for (i = 0; i < _cliqueSize; i++)
		{
			auto id = _cliqueMembers[i];
			auto count = PopCountAandB((UInt64*)_bitset, (UInt64*) _graph[id].Neighbours, (bitSetLength >> 3));
			if ((count != _cliqueSize) && (((count + 1) != _cliqueSize) || BitTest(_graph[id].Neighbours, id)))
				return false;
		}

		return true;
	}

	bool IsIndependantSet(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_vertices, decltype(Vertex::Id) _verticesSize, byte* _bitset)
	{
		decltype(Vertex::Id) i;
		auto bitSetLength = (decltype(Vertex::Id))GetQWordAlignedSizeForBits(_graph.size());

		ZeroMemoryPack8(_bitset, bitSetLength);
		for (i = 0; i < _verticesSize; i++)
		{
			if (BitTest(_bitset, _vertices[i]))
				return false;	// invalidArgument : duplicate found in _vertices

			BitSet(_bitset, _vertices[i]);
		}

		for (i = 0; i < _verticesSize; i++)
		{
			auto id = _vertices[i];
			auto count = PopCountAandB((UInt64*)_bitset, (UInt64*)_graph[id].Neighbours, (bitSetLength >> 3));
			if ((count != 0) && ((count != 1) || !BitTest(_graph[id].Neighbours, id)))
				return false;
		}

		return true;
	}

	bool IsProperColor(Ext::Array<Vertex> _graph, decltype(Vertex::Id) *_vertexColor)
	{
		for (decltype(Vertex::Id) i = 0; i < _graph.size(); i++)
		{
			if ((_vertexColor[i] == 0) || (_vertexColor[i] > _graph.size()))
				return false;

			auto neighbours = _graph[i].Neighbours;
			for (decltype(Vertex::Id) j = i + 1; j < _graph.size(); j++)
				if (BitTest(neighbours, j) && (_vertexColor[i] == _vertexColor[j]))
					return false;
		}

		return true;
	}
}
