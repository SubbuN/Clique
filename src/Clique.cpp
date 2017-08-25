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
	extern TextStream TraceMessage;

	static bool s_IsDiagnosticsEnabled = false;
	static ID s_DiagnosticsDepth = 0;

	static char s_SaveGraphPath[256] = "";
	static bool s_ShouldSaveGraph = false;
	static ID s_SaveGraphDepth = 0xFFFF;


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

	Ext::Array<Vertex> CreateFenceGraph(decltype(Vertex::Id) _n, decltype(Vertex::Id) _height)
	{
		if (_height > (_n / 2))
			throw "Invalid '_height'. _height should be less than or equal to n/2.";

		Ext::Array<Vertex> graph = CreateGraph(_n);

		for (decltype(Vertex::Id) i = 0; i < _n; i++)
		{
			for (decltype(Vertex::Id) j = 1; j <= _height; j++)
			{
				auto k = (j <= i) ? (i - j) : (_n - j + i);
				BitSet(graph[i].Neighbours, k);
				BitSet(graph[k].Neighbours, i);
			}

			for (decltype(Vertex::Id) j = 1; j <= _height; j++)
			{
				auto k = (i + j) % _n;
				BitSet(graph[i].Neighbours, k);
				BitSet(graph[k].Neighbours, i);
			}
		}

		for (decltype(Vertex::Id) i = 0; i < _n; i++)
			graph[i].Count = (decltype(Vertex::Count))PopCount((UInt64*)graph[i].Neighbours, GetQWordAlignedSizeForBits(_n) >> 3);

		return graph;
	}

	forceinline void AddSelfEdges(Ext::Array<Vertex>& _graph)
	{
		for (ID i = 0; i < _graph.size(); i++)
		{
			if (!BitTest(_graph[i].Neighbours, i))
			{
				BitSet(_graph[i].Neighbours, i);
				_graph[i].Count++;
			}
		}
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

		struct ElementData
		{
			typedef decltype(Vertex::Id) ID;
			ID	OriginalVertexId, VertexId, Attribute;

			inline void SetValue(ID vertexId, ID originalVertexId, byte attribute = (byte)PartitionVertexStatus::ConnectedToAll)
			{
				VertexId = vertexId;
				OriginalVertexId = originalVertexId;
				Attribute = attribute;
			};
		};

		struct TryFindCliqueThisContext
		{
			decltype(Vertex::Id)	*CliqueMembers;
			decltype(Vertex::Id)	ZeroReferenceDepth, CliqueSize, TopGraphForMemoryReclaim;
			bool PrintStatistics;

			TryFindCliqueThisContext()
				: CliqueMembers(nullptr), ZeroReferenceDepth(0),
				CliqueSize(0), TopGraphForMemoryReclaim(1),
				PrintStatistics(false)
			{
			}

			TryFindCliqueThisContext(decltype(Vertex::Id) *cliqueMembers,
						decltype(Vertex::Id)	zeroReferenceDepth,
						decltype(Vertex::Id) cliqueSize,
						bool printStatistics)
				:	CliqueMembers(cliqueMembers),
					ZeroReferenceDepth(zeroReferenceDepth),
					CliqueSize(cliqueSize),
					TopGraphForMemoryReclaim(zeroReferenceDepth + 1),
					PrintStatistics(printStatistics)
			{
			}

			void ctor(decltype(Vertex::Id) *cliqueMembers,
				decltype(Vertex::Id)	zeroReferenceDepth,
				decltype(Vertex::Id) cliqueSize,
				bool printStatistics)
			{
				CliqueMembers = cliqueMembers;
				ZeroReferenceDepth = zeroReferenceDepth;
				CliqueSize = cliqueSize;
				TopGraphForMemoryReclaim = zeroReferenceDepth + 1;
				PrintStatistics = printStatistics;
			}

			void PrepareForInvoke()
			{
				CliqueSize = 0;
				TopGraphForMemoryReclaim = ZeroReferenceDepth + 1;
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
			decltype(Vertex::Id)	pivotVertexIdx;

			Int64	callCount, startCallCount;
			Ext::BooleanError isCliqueExist;
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
			TryFindCliqueThisContext This;
			TryFindCliqueCallFrame	*CallFrame;

			UInt64	Calls, PartitionExtr, TwoNHits, TwoNColorHits, SubgraphHits, BtmUpHits, BtmUpHits2, BtmUpCheck, BtmUpCheck2, Count9, Count11, Count12, Count13, Count14;

		public:
			ResourceManager(decltype(Vertex::Id) _graphDegree, UInt32 _blockSize)
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

				This.ctor(decltype(TryFindCliqueThisContext::CliqueMembers) (((byte*)lId3) + GetQWordAlignedSize(_graphDegree * sizeof(ID))), 0, 0, false);
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
				Calls = PartitionExtr = TwoNHits = TwoNColorHits = SubgraphHits = BtmUpHits = BtmUpHits2 = BtmUpCheck = BtmUpCheck2 = Count9 = Count11 = Count12 = Count13 = Count14 = 0;
			}

			void* AllocateGraphMemory(size_t size)
			{
				auto ptr = GraphMemoryPool.Allocate(GetGraphAllocationSize(size));
				if (ptr == nullptr)
				{
					auto i = This.TopGraphForMemoryReclaim++;
					GraphMemoryPool.Free(CallFrame[i]._graph.ptr());
					CallFrame[i]._graph = Ext::Array<Graph::Vertex>(nullptr, CallFrame[i]._graph.size());
					ptr = GraphMemoryPool.Allocate(GetGraphAllocationSize(size));
				}

				return ptr;
			}
		};
	}

	namespace SAT
	{
		struct VariableStatistics
		{
			ID ClauseLiterals;
			ID Clauses;
			ID TrivialClauses;
		};

		struct Variable
		{
			VariableStatistics Positive;
			VariableStatistics Negative;

			bool IsPure()
			{
				return ((Positive.Clauses == 0) || (Negative.Clauses == 0));
			};

			bool IsPotentialClauseCombiner()
			{
				return ((Positive.Clauses == 1) && (Negative.Clauses == 1));
			}

			ID TotalClauses()
			{
				return Positive.Clauses + Negative.Clauses;
			};
		};

		enum struct ReturnCallSite : byte
		{
			Invalid = 0,
			Exit = 1,
			Reduction = 2,
		};

		struct SolveCallReturnParam
		{
			ID Assignment;
			ID Variable;
			bool IsSuccess;

			void ctor(ID variable, ID assignment, bool isSuccess)
			{
				Variable = variable;
				Assignment = assignment;
				IsSuccess = isSuccess;
			}
		};

		struct SolveCallFrame
		{
			Formula _formula;
			Ext::List<VariableAssignmentOverride, ID> _assignmentOverride;
			decltype(Vertex::Id) _assignmentOrder;

			ID assignmentOrder;
			int assignProbeVariableId;

			ReturnCallSite ReturnTo;

			UInt64	Calls;
		};

		struct SolveThisContext
		{
			Formula _formula;
			SAT::VariableAssignment *_variableAssignment;
			SAT::Variable *_variables;

			ID *OriginalVertexId;
			void *CliqueMembers;
			void *GraphMemory;
			ID TempGraphSize;

			ID AssignedDepth;
			bool	TraceEvaluation;

			SolveThisContext()
				: _variableAssignment(nullptr), 
				_variables(nullptr),
				OriginalVertexId(nullptr), CliqueMembers(nullptr),
				GraphMemory(nullptr), TempGraphSize(0),
				AssignedDepth(0), TraceEvaluation(false)
			{
			}

			void ctor(Formula _formula, SAT::VariableAssignment *_variableAssignment,
				SAT::Variable *_variables, ID *originalVertexId, void *cliqueMembers,
				void *pGraphMemory, ID tempGraphSize, bool traceEvaluation)
			{
				this->_formula = _formula;
				this->_variableAssignment = _variableAssignment;
				this->_variables = _variables;
				this->OriginalVertexId = originalVertexId;
				this->CliqueMembers = cliqueMembers;
				this->GraphMemory = pGraphMemory;
				this->TempGraphSize = tempGraphSize;
				this->TraceEvaluation = traceEvaluation;
				this->AssignedDepth = 0;
			}
		};

		class ResourceManager
		{
		public:
			Clique::ResourceManager CliqueResourceManager;
			SolveCallFrame	*CallFrame;
			SolveThisContext This;
			char	Line[256];

		public:
			ResourceManager(decltype(Vertex::Id) _variables, decltype(Vertex::Id) _graphDegree, UInt32 _blockSize)
				: CliqueResourceManager(_graphDegree, _blockSize), CallFrame(nullptr)
			{
				size_t allocationSize = sizeof(SolveCallFrame) * _variables;
				CallFrame = (SolveCallFrame*)AllocMemory(allocationSize);
				memset(CallFrame, 0, allocationSize);
			}

			~ResourceManager()
			{
				FreeMemory(CallFrame);
				CallFrame = nullptr;
			}

			void ClearCounters()
			{
			}
		};
	}


	void PrintCallFramDiagnosticsInfo(decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager, TextStream textStream)
	{
		if (textStream == nullptr)
			return;

		char sz[512];

		sprintf_s(sz, sizeof(sz), "%15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", _resourceManager.Calls, _resourceManager.PartitionExtr, _resourceManager.TwoNHits, _resourceManager.TwoNColorHits, _resourceManager.SubgraphHits, _resourceManager.BtmUpHits, _resourceManager.BtmUpHits2, _resourceManager.BtmUpCheck, _resourceManager.BtmUpCheck2, _resourceManager.Count9, _resourceManager.Count11, _resourceManager.Count12, _resourceManager.Count13, _resourceManager.Count14);
		textStream(sz);

		decltype(Vertex::Id) subGraphSize = 0;
		for (size_t i = 0; (_resourceManager.CallFrame[i].callCount > 0); i++)
		{
			Clique::TryFindCliqueCallFrame &frame = _resourceManager.CallFrame[i];
			sprintf_s(sz, sizeof(sz), "%8d Calls:%15I64d StartCall:%15I64d GraphSize:%8d ActiveVertices:%8d CliqueSize[In]:%8d CliqueSize:%8d ParentCliqueSize:%8d VertexEdgeCount[0]:%8d VertexEdgeCount[last]:%8d\n", 
				(int)i + 1, frame.callCount, frame.startCallCount,
				(i < _depth) ? (decltype(Vertex::Id))frame._graph.size() : subGraphSize,
				frame.activeVertexCount, frame._cliqueSize, frame.cliqueSize, frame._cliqueMembersCount,
				(i <= _depth) ? frame.vertexEdgeCount[0] : -1,
				(i <= _depth) ? frame.vertexEdgeCount[frame.activeVertexCount - 1] : -1
			);

			textStream(sz);

			subGraphSize = frame.subGraphSize;
		}
	}

	//
	//	Notes:
	//		For performance and simplicity, a self loop edge is created for each vertex in the _graph
	//		A look-up is maintained to answer the vertex at given rank(sorted order) using vertexId.
	//			But not vice-versa. Reverse look-up may be needed for further enhancements
	//
	//		A look-up is maintained to answer the original vertex id (in top most level _graph) using
	//		_originalVertexId at each level. The ordering is preserved at each level. So, to find a vertex
	//		in a given level for the original vertex, scan the index of original vertex id at the given level.
	//		Direct look-up may be needed for further enhancements. But this look-up needs two levels.
	//			1. OriginalVertexId to vertexId at each level
	//			2.	OriginalVertexId to deepest level where it is last appeared.
	//
	//		Callbacks and filters to avoid exploring certain path are not added in this implementation for simplicity.
	//


	Ext::BooleanError TryFindClique(Ext::Array<Vertex> _graph, ID *_originalVertexId,
				Ext::Unsafe::ArrayOfSet<Clique::ElementData, ID>& _cliqueMembers,
				decltype(Vertex::Id) _cliqueMembersCount,
				decltype(Vertex::Id)& _cliqueSize, Clique::FindOperation _op,
				byte* _targettedVertices, decltype(Vertex::Id) _targettedVerticesCount,
				decltype(Vertex::Id) _depth, Clique::CliqueHandler *_handler, 
				Clique::ResourceManager& _resourceManager);

	decltype(Vertex::Id) GetClusters(Ext::Array<Vertex> _graph,
		Ext::ArrayOfArray<decltype(Vertex::Id), decltype(Vertex::Id)> *_pClusters,
		Ext::Array<ID> _vertexColor, decltype(Vertex::Id) _cliqueSize,
		decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager);


	//
	//	Returns
	//		True : When formula is reduced without conflict
	//		False : resulted in conflict of the formula
	//

	bool Reduce(SAT::VariableAssignment *_variableAssignment, 
			Ext::ArrayOfArray<int, ID> _srcClauses,
			Ext::ArrayOfArray<int, ID>& _destClauses,
			bool _traceEvaluation)
	{
		if (_srcClauses.setCount() == 0)
		{
			_destClauses.Clear();
			return true;
		}

		if (_traceEvaluation)
			TraceMessage("\r\n");

		char msg[256];
		auto elementsCount = _srcClauses.elementsCount();
		int *list = _srcClauses.ptrList();
		int *list2 = _destClauses.ptrList();

		ID start2 = 0, clause2 = 0;

		auto clauseSize = _srcClauses.GetSetSize(0);
		auto start = _srcClauses.GetSetStartIndex(0);

		int literal = 0;
		SAT::TruthValue truthValue = SAT::TruthValue::Unassigned;

		for (ID i = 0; i < _srcClauses.setCount(); )
		{
			auto prevClauseSize = clauseSize;
			auto prevStart = start;

			auto base = start2;
			bool discardClause = false;

			for (auto end = start + clauseSize; start < end; start++)
			{
				literal = list[start];
				truthValue = (literal < 0) ? SAT::TruthValue::False : SAT::TruthValue::True;
				if (literal < 0)
					literal = -literal;

				if (_variableAssignment[literal].State == SAT::AssignmentState::Unassigned)
					list2[start2++] = list[start];
				else if (_variableAssignment[literal].Value == truthValue)
				{
					start2 = base;
					discardClause = true;
					break;
				}
				else // if (_variableAssignment[literal].Value != truthValue)
				{
					// evaluates to False. Discard the literal.
				}
			}

			if ((base == start2) && !discardClause)
			{
				if (_traceEvaluation)
				{
					sprintf_s(msg, sizeof(msg), "Clause %d resulted in conflict\r\n   ", (i + 1));
					TraceMessage(msg);
					PrintSATClause(list + prevStart, prevClauseSize, TraceMessage, msg, sizeof(msg));
				}

				return false; // reduction resulted formula failure
			}

			if (++i < _srcClauses.setCount())
			{
				clauseSize = _srcClauses.GetSetSize(i);
				start = _srcClauses.GetSetStartIndex(i);
			}

			if (base < start2)
			{
				if (clause2 == 0)
					_destClauses.Clear();

				_destClauses.InitSet(clause2++, start2 - base);

				if ((_traceEvaluation) && ((start2 - base) < prevClauseSize))
				{
					sprintf_s(msg, sizeof(msg), "Clause %d is reduced.\r\n   ", i);
					TraceMessage(msg);
					if (_srcClauses.ptr() != _destClauses.ptr())
					{
						PrintSATClause(list + prevStart, prevClauseSize, TraceMessage, msg, sizeof(msg));
						TraceMessage("   ");
					}

					PrintSATClause(list2 + base, start2 - base, TraceMessage, msg, sizeof(msg));
				}
			}
			else if (_traceEvaluation)
			{
				sprintf_s(msg, sizeof(msg), "Clause %d discarded due to %d satiesfies.\r\n   ", i, (truthValue == SAT::TruthValue::False) ? -literal : literal);
				TraceMessage(msg);
				PrintSATClause(list + prevStart, prevClauseSize, TraceMessage, msg, sizeof(msg));
			}
		}

		if (clause2 == 0)
			_destClauses.Clear();

		return true;
	}

	bool AssignValue(SAT::VariableAssignment *_variableAssignment,
			decltype(Vertex::Id)& assignmentOrder,
			Ext::List<SAT::VariableAssignmentOverride, ID>& _assignmentOverride,
			ID variable, SAT::AssignmentState state,
			SAT::TruthValue truthValue, bool traceEvaluation)
	{
		char msg[256];

		if (traceEvaluation)
		{
			if (state == SAT::AssignmentState::PureVariable)
				sprintf_s(msg, sizeof(msg), "Assignment:%7d Variable %d ==> Marked as pure.\r\n", assignmentOrder, variable);
			else
			{
				char *szTruthValue = "";
				if (truthValue == SAT::TruthValue::True)
					szTruthValue = "True";
				else if (truthValue == SAT::TruthValue::False)
					szTruthValue = "False";
				else if (truthValue == SAT::TruthValue::Null)
					szTruthValue = "Null";

				if (state == SAT::AssignmentState::AssignedFirst)
					TraceMessage("Selecting::\r\n");

				sprintf_s(msg, sizeof(msg), "Assignment:%7d Variable %d ==> %s\r\n", assignmentOrder, variable, szTruthValue);
			}

			TraceMessage(msg);
		}

		if ((_variableAssignment[variable].RequiredLiteral != SAT::TruthValue::Unassigned) && 
			 (_variableAssignment[variable].RequiredLiteral != truthValue))
		{
			return true;
		}

		_assignmentOverride.append(_variableAssignment[variable].ToOverride(variable));
		_variableAssignment[variable].Set(truthValue, state, assignmentOrder++);

		return false;
	}

	SAT::SolveCallReturnParam Solve(SAT::Formula _formula,
			SAT::VariableAssignment *_variableAssignment,
			Ext::List<SAT::VariableAssignmentOverride, ID> _assignmentOverride,
			decltype(Vertex::Id) _assignmentOrder,
			decltype(Vertex::Id) _depth,
			SAT::ResourceManager& _resourceManager)
	{
		Clique::ResourceManager& resourceManager = _resourceManager.CliqueResourceManager;
		SAT::SolveCallFrame *frame = &_resourceManager.CallFrame[_depth];
		SAT::SolveCallReturnParam returnValue;
		Ext::ArrayOfArray<int, ID> reducedClauses(nullptr, 0);
		SAT::Formula formula;
		SAT::VariableStatistics *pVariableStatistics;
		char msg[256];

		frame->ReturnTo = SAT::ReturnCallSite::Exit;

	Enter:
		frame->_formula = _formula;
		frame->_assignmentOrder = _assignmentOrder;
		frame->Calls++;

		ID assignmentOrder = _assignmentOrder;

		Ext::ArrayOfArray<int, ID> clauses = _formula.Clauses;
		auto variablesCount = _formula.Variables + 1;
		auto variables = _resourceManager.This._variables;
		auto assignmentOrderItr = assignmentOrder;
		ID pureLiterals = 0;
		int *list = clauses.ptrList();
		SAT::TruthValue truthValue;
		auto traceEvaluation = _resourceManager.This.TraceEvaluation;

		if (traceEvaluation)
		{
			sprintf_s(msg, sizeof(msg), "Evaluation Depth:%d\r\n\r\n", _depth);
			TraceMessage(msg);
		}

		memset(variables, 0, sizeof(SAT::Variable) * variablesCount);

		for (ID i = 0; i < clauses.elementsCount(); i++)
		{
			auto literal = list[i];
			if (literal < 0)
			{
				literal = -literal;
				variables[literal].Negative.Clauses++;
			}
			else
				variables[literal].Positive.Clauses++;

			// assert((_variableAssignment[literal].State == SAT::AssignmentState::Unassigned) || (_variableAssignment[literal].State == SAT::AssignmentState::Dependent));
		}

		bool isPotentialClauseCombinerExist = false;
		for (ID i = 0; i < variablesCount; i++)
		{
			if (_variableAssignment[i].State != SAT::AssignmentState::Unassigned)
				continue;

			isPotentialClauseCombinerExist |= variables[i].IsPotentialClauseCombiner();

			if (variables[i].IsPure())
			{
				pureLiterals++;
				truthValue = (variables[i].Positive.Clauses > 0) ? SAT::TruthValue::True : SAT::TruthValue::False;
				if (AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, i, SAT::AssignmentState::PureVariable, truthValue, traceEvaluation))
				{
					returnValue.ctor(i, assignmentOrder + 1, false);
					goto Return;
				}
			}
			else
			{
				auto totalClauses = variables[i].TotalClauses();
				if (totalClauses == 0)
				{
					if (AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, i, SAT::AssignmentState::Assigned, SAT::TruthValue::Null, traceEvaluation))
					{
						returnValue.ctor(i, assignmentOrder + 1, false);
						goto Return;
					}
				}
			}
		}

		for (ID i = 0; i < clauses.setCount(); i++)
		{
			auto clauseSize = clauses.GetSetSize(i);
			if (clauseSize > 1)
				continue;

			auto literal = list[clauses.GetSetStartIndex(i)];
			if (literal < 0)
			{
				literal = -literal;
				variables[literal].Negative.TrivialClauses++;
				truthValue = SAT::TruthValue::False;
			}
			else
			{
				variables[literal].Positive.TrivialClauses++;
				truthValue = SAT::TruthValue::True;
			}

			if (_variableAssignment[literal].State == SAT::AssignmentState::Unassigned)
			{
				if (AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, literal, SAT::AssignmentState::Assigned, truthValue, traceEvaluation))
				{
					returnValue.ctor(literal, assignmentOrder + 1, false);
					goto Return;
				}
			}
			else if (_variableAssignment[literal].Value != truthValue)
			{
				returnValue.ctor(literal, assignmentOrder + 1, false);
				goto Return;
			}
		}

		int assignProbeVariableId = 0;
		if (assignmentOrderItr == assignmentOrder)
		{
			if (traceEvaluation)
			{
				ID largestClauseSize = 0;
				auto clauseSizeCounts = _resourceManager.CliqueResourceManager.lId;
				clauseSizeCounts[0] = 0;

				for (ID i = 0; i < clauses.setCount(); i++)
				{
					auto clauseSize = clauses.GetSetSize(i);
					if (largestClauseSize < clauseSize)
					{
						for (ID i = largestClauseSize + 1; i <= clauseSize; i++)
							clauseSizeCounts[i] = 0;

						largestClauseSize = clauseSize;
					}

					clauseSizeCounts[clauseSize]++;
				}

				TraceMessage("ClauseSizeCounts: ");
				PrintArray(clauseSizeCounts, largestClauseSize + 1, TraceMessage);
				TraceMessage("\r\n\r\n");
			}

			void	*pGraphMemory = _resourceManager.This.GraphMemory;
			Ext::Unsafe::ArrayOfSet<Clique::ElementData, decltype(Vertex::Id)> cliqueMembers(_resourceManager.This.CliqueMembers, _resourceManager.This.TempGraphSize);

			auto isAnyLiteralRequired = false;
			for (ID i = 0; i < variablesCount; i++)
			{
				if (_variableAssignment[i].Order < _assignmentOrder)
					continue;

				if ((_variableAssignment[i].RequiredLiteral == SAT::TruthValue::True) ||
					(_variableAssignment[i].RequiredLiteral == SAT::TruthValue::False))
					continue;

				bool isPositiveValueRequired = false;

				pVariableStatistics = &variables[i].Positive;
				if (pVariableStatistics->TrivialClauses > 0)
					isPositiveValueRequired = true;	// unreachable
				else if (pVariableStatistics->Clauses <= 1)
					isPositiveValueRequired = false;
				else if (pVariableStatistics->Clauses > 1)
				{
					Ext::Array<Vertex> graph = CreateGraph(_formula, i, 0, resourceManager.lId, resourceManager.lId2, pGraphMemory);
					auto cliqueSize = pVariableStatistics->Clauses;

					if (graph.size() == 0)
						isPositiveValueRequired = true;
					else if (cliqueSize == 2)
						isPositiveValueRequired = false;
					else
					{
						AddSelfEdges(graph);
						resourceManager.This.PrepareForInvoke();
						isPositiveValueRequired = TryFindClique(graph, _resourceManager.This.OriginalVertexId, cliqueMembers, 0, cliqueSize, Clique::FindOperation::ExactSearch, nullptr, 0, 0, nullptr, resourceManager) == Ext::BooleanError::False;
					}
				}

				if (isPositiveValueRequired ? AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, i, SAT::AssignmentState::Assigned, SAT::TruthValue::True, traceEvaluation) : (_variableAssignment[i].RequiredLiteral == SAT::TruthValue::True))
				{
					returnValue.ctor(i, assignmentOrder + 1, false);
					goto Return;
				}

				bool isNegativeValueRequired = false;

				pVariableStatistics = &variables[i].Negative;
				if (pVariableStatistics->TrivialClauses > 0)
					isNegativeValueRequired = true;	// unreachable
				else if (pVariableStatistics->Clauses <= 1)
					isNegativeValueRequired = false;
				else if (pVariableStatistics->Clauses > 1)
				{
					Ext::Array<Vertex> graph = CreateGraph(_formula, 0, -(int)i, resourceManager.lId, resourceManager.lId2, pGraphMemory);
					auto cliqueSize = pVariableStatistics->Clauses;

					if (graph.size() == 0)
						isNegativeValueRequired = true;
					else if (cliqueSize == 2)
						isNegativeValueRequired = false;
					else
					{
						AddSelfEdges(graph);
						resourceManager.This.PrepareForInvoke();
						isNegativeValueRequired = TryFindClique(graph, _resourceManager.This.OriginalVertexId, cliqueMembers, 0, cliqueSize, Clique::FindOperation::ExactSearch, nullptr, 0, 0, nullptr, resourceManager) == Ext::BooleanError::False;
					}
				}

				if (isNegativeValueRequired ? AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, i, SAT::AssignmentState::Assigned, SAT::TruthValue::False, traceEvaluation) : (_variableAssignment[i].RequiredLiteral == SAT::TruthValue::False))
				{
					returnValue.ctor(i, assignmentOrder + 1, false);
					goto Return;
				}

				bool isLiteralRequired = isPositiveValueRequired || isNegativeValueRequired;
				if (!isLiteralRequired && (variables[i].Positive.Clauses > 0) && (variables[i].Negative.Clauses > 0))
				{
					Ext::Array<Vertex> graph = CreateGraph(_formula, i, -(int)i, resourceManager.lId, resourceManager.lId2, pGraphMemory);
					auto cliqueSize = variables[i].Positive.Clauses + variables[i].Negative.Clauses;

					if (graph.size() == 0)
						isLiteralRequired = true;
					else if (cliqueSize == 2)
						isLiteralRequired = false;
					else
					{
						AddSelfEdges(graph);
						resourceManager.This.PrepareForInvoke();
						isLiteralRequired = TryFindClique(graph, _resourceManager.This.OriginalVertexId, cliqueMembers, 0, cliqueSize, Clique::FindOperation::ExactSearch, nullptr, 0, 0, nullptr, resourceManager) == Ext::BooleanError::False;
					}
				}

				if (isLiteralRequired && (_variableAssignment[i].IsRequired != SAT::TruthValue::True))
				{
					_assignmentOverride.append(_variableAssignment[i].ToOverride(i));
					_variableAssignment[i].Order = assignmentOrder++;
					_variableAssignment[i].IsRequired = SAT::TruthValue::True;
					isAnyLiteralRequired = true;
				}
				else if (!isLiteralRequired && (_variableAssignment[i].IsRequired == SAT::TruthValue::True))
				{
					returnValue.ctor(i, assignmentOrder + 1, false);
					goto Return;
				}
			}

			if (assignmentOrderItr == assignmentOrder)
			{
				for (ID i = 0; i < clauses.setCount(); i++)
				{
					auto clauseSize = clauses.GetSetSize(i);
					auto start = clauses.GetSetStartIndex(i);
					for (ID end = start + clauseSize; start < end; start++)
					{
						auto literal = list[start];

						pVariableStatistics = (literal < 0) ? &variables[-literal].Negative : &variables[literal].Positive;
						pVariableStatistics->ClauseLiterals += clauseSize;
					}
				}

				ID clauseCount = clauses.setCount(), clauseLiterals = 0;
				int literal = 0;
				for (ID i = 0; i < variablesCount; i++)
				{
					if ((_variableAssignment[i].IsRequired != SAT::TruthValue::True) && isAnyLiteralRequired)
						continue;

					truthValue = _variableAssignment[i].RequiredLiteral;
					if (!isAnyLiteralRequired || (truthValue == SAT::TruthValue::True) || (truthValue != SAT::TruthValue::False))
					{
						auto count = variables[i].Positive.ClauseLiterals + variables[i].Negative.Clauses;
						if ((count > clauseLiterals) || ((count == clauseLiterals) && (variables[i].TotalClauses() > clauseCount)))
						{
							literal = i;
							clauseLiterals = count;
							clauseCount = variables[i].TotalClauses();
						}
					}

					if (!isAnyLiteralRequired || (truthValue == SAT::TruthValue::False) || (truthValue != SAT::TruthValue::True))
					{
						auto count = variables[i].Negative.ClauseLiterals + variables[i].Positive.Clauses;
						if ((count > clauseLiterals) || ((count == clauseLiterals) && (variables[i].TotalClauses() > clauseCount)))
						{
							literal = -(int)i;
							clauseLiterals = count;
							clauseCount = variables[i].TotalClauses();
						}
					}
				}

				truthValue = (literal < 0) ? SAT::TruthValue::False : SAT::TruthValue::True;
				if (literal < 0)
					literal = -literal;

				if (AssignValue(_variableAssignment, assignmentOrder, _assignmentOverride, literal, SAT::AssignmentState::AssignedFirst, truthValue, traceEvaluation))
				{
					returnValue.ctor(literal, assignmentOrder + 1, false);
					goto Return;
				}

				assignProbeVariableId = literal;
				_resourceManager.This.AssignedDepth++;
			}
		}

		frame->assignProbeVariableId = assignProbeVariableId;
		frame->assignmentOrder = assignmentOrder;
		frame->_assignmentOverride = _assignmentOverride;

		reducedClauses.ctor(_resourceManager.CliqueResourceManager.MemoryPool.Allocate(Ext::ArrayOfArray<int, ID>::GetAllocationSize((ID)clauses.setCount(), (ID)clauses.elementsCount())), clauses.setCount(), clauses.elementsCount());

	AlternateValueAssignment:
		if (!Reduce(_variableAssignment, clauses, reducedClauses, traceEvaluation))
		{
			resourceManager.MemoryPool.Free(reducedClauses.ptr());

			returnValue.ctor(0, assignmentOrder + 1, false);
			goto Return;
		}

		if (reducedClauses.setCount() == 0)
		{
			for (ID i = 0; i < variablesCount; i++)
			{
				if (_variableAssignment[i].State != SAT::AssignmentState::Unassigned)
					continue;

				_assignmentOverride.append(_variableAssignment[i].ToOverride(i));
				_variableAssignment[i].Set(SAT::TruthValue::Null, SAT::AssignmentState::Assigned, assignmentOrder++);
			}

			// resolve all Pure values

			resourceManager.MemoryPool.Free(reducedClauses.ptr());

			returnValue.ctor(0, assignmentOrder, true);
			goto Return;
		}

		// _variables = _variables;
		// _variableAssignment = _variableAssignment;
		_formula = SAT::Formula(_formula.Variables, reducedClauses);
		_assignmentOrder = assignmentOrder;

		frame = &_resourceManager.CallFrame[++_depth];
		frame->ReturnTo = SAT::ReturnCallSite::Reduction;
		goto Enter;

	Reduction:
		frame = &_resourceManager.CallFrame[--_depth];

		formula.ctor(_formula.Variables, _formula.Clauses);

		_formula = frame->_formula;
		_assignmentOrder = frame->_assignmentOrder;
		_assignmentOverride = frame->_assignmentOverride;
		// _variableAssignment = frame->_variableAssignment;

		assignmentOrder = frame->assignmentOrder;

		clauses = _formula.Clauses;

		// memset(_variables, 0, sizeof(SAT::Variable) * variablesCount); // uncomment if used after the call.

		if (returnValue.IsSuccess)
		{
			// resolve all Pure values
		}
		else if (frame->assignProbeVariableId > 0)
		{
			auto literal = frame->assignProbeVariableId;
			if (_variableAssignment[literal].State == SAT::AssignmentState::AssignedFirst)
			{
				_variableAssignment[literal].State = SAT::AssignmentState::AssignedSecond;
				_variableAssignment[literal].Value = (_variableAssignment[literal].Value == SAT::TruthValue::True) ? SAT::TruthValue::False : SAT::TruthValue::True;

				reducedClauses = formula.Clauses;

				// if (_resourceManager.TraceEvaluation)
				if (_resourceManager.This.AssignedDepth < 60)
				{
					sprintf_s(msg, sizeof(msg), "AssignedDepth:%d Depth:%7d Variable %d ==> %s  TickCount:%I64d\r\n", _resourceManager.This.AssignedDepth, _depth, literal, (_variableAssignment[literal].Value == SAT::TruthValue::True) ? "True" : "False", GetCurrentTick());
					TraceMessage(msg);
				}

				goto AlternateValueAssignment;
			}
			else if (_variableAssignment[literal].State == SAT::AssignmentState::AssignedSecond)
			{
				frame->assignProbeVariableId = 0;
				_resourceManager.This.AssignedDepth--;
			}
		}

		resourceManager.MemoryPool.Free(formula.Clauses.ptr());

	Return:

		if (!returnValue.IsSuccess)
		{
			// revert overrides made in this iteration.
			for (ID i = assignmentOrder; i-- > _assignmentOrder; )
				_variableAssignment[_assignmentOverride[i].Variable].Set(_assignmentOverride[i]);

			_assignmentOverride.size(_assignmentOrder);
		}

		switch (frame->ReturnTo)
		{
		case SAT::ReturnCallSite::Exit: break;
		case SAT::ReturnCallSite::Reduction: goto Reduction;
		default:
			throw std::exception("ReturnTo");
		}

		return returnValue;
	}

	bool Solve(SAT::Formula _formula)
	{
		auto graphSize = _formula.Clauses.elementsCount();
		auto bitSetLength = GetQWordAlignedSizeForBits(graphSize);
		auto resourceManager = SAT::ResourceManager(_formula.Variables, graphSize, (UInt32)(2 * 32 * sizeof(int) + bitSetLength * 3 + (3 * GetQWordAlignedSize(graphSize * sizeof(ID)) + 3 * bitSetLength) * Clique::FramesPerBlock * 2));
		auto variablesCount = _formula.Variables + 1;
		auto variableAssignment = (SAT::VariableAssignment*)resourceManager.CliqueResourceManager.MemoryPool.Allocate(
					GetQWordAlignedSize(sizeof(SAT::VariableAssignment) * variablesCount) + 
					GetQWordAlignedSize(sizeof(SAT::Variable) * variablesCount) + 
					GetQWordAlignedSize(sizeof(SAT::VariableAssignmentOverride) * variablesCount * 2) + 
					Ext::ArrayOfArray<int, ID>::GetAllocationSize((ID)_formula.Clauses.setCount(), (ID)_formula.Clauses.elementsCount())
				);

		auto variables = (SAT::Variable*)((byte*)variableAssignment + GetQWordAlignedSize(sizeof(SAT::VariableAssignment) * variablesCount));
		Ext::List<SAT::VariableAssignmentOverride, ID> assignmentOverride((SAT::VariableAssignmentOverride*)((byte*)variables + GetQWordAlignedSize(sizeof(SAT::Variable) * variablesCount)), variablesCount * 2, 0);
		SAT::Formula reducedFormula(_formula.Variables, _formula.Clauses.Clone(((byte*)assignmentOverride.ptr() + GetQWordAlignedSize(sizeof(SAT::VariableAssignmentOverride) * variablesCount * 2)), _formula.Clauses.elementsCount()));
		ReleaseMemoryToPool dtor(resourceManager.CliqueResourceManager.MemoryPool, variableAssignment);

		memset(variableAssignment, 0xFFFFFFFF, sizeof(SAT::VariableAssignment) * (variablesCount));

		ID tempGraphSize;

		{
			ID literalMaxClauses = 0, largestClauseSize = 0;
			auto clauses = _formula.Clauses;
			auto list = clauses.ptrList();

			memset(variables, 0, sizeof(SAT::Variable) * variablesCount);
			for (ID i = 0; i < clauses.elementsCount(); i++)
			{
				auto literal = list[i];
				if (literal < 0)
				{
					literal = -literal;
					variables[literal].Negative.Clauses++;
				}
				else
					variables[literal].Positive.Clauses++;
			}

			for (ID i = 0; i < variablesCount; i++)
			{
				if (variables[i].TotalClauses() > literalMaxClauses)
					literalMaxClauses = variables[i].TotalClauses();
			}

			for (ID i = 0; i < clauses.setCount(); i++)
			{
				auto clauseSize = clauses.GetSetSize(i);
				if (clauseSize > largestClauseSize)
					largestClauseSize = clauseSize;
			}

			tempGraphSize = literalMaxClauses * largestClauseSize;
		}

		auto originalVertexId = (ID*)resourceManager.CliqueResourceManager.MemoryPool.Allocate(
			GetQWordAlignedSize(sizeof(ID) * tempGraphSize) +
			Ext::Unsafe::ArrayOfSet<Clique::ElementData, decltype(Vertex::Id)>::GetAllocationSize(tempGraphSize)
		);
		void *cliqueMembers = (byte*)originalVertexId + GetQWordAlignedSize(sizeof(ID) * tempGraphSize);
		ReleaseMemoryToPool dtor2(resourceManager.CliqueResourceManager.GraphMemoryPool, resourceManager.CliqueResourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(tempGraphSize)));

		for (ID i = 0; i < tempGraphSize; i++)
			originalVertexId[i] = i;

		resourceManager.This.ctor(_formula, variableAssignment, variables, originalVertexId, cliqueMembers, dtor2.ptr(), tempGraphSize, false);
		resourceManager.This.TraceEvaluation = false;

		return Solve(reducedFormula, variableAssignment, assignmentOverride, 0, 0, resourceManager).IsSuccess;
	}


	decltype(Vertex::Id) FindClique(Ext::Array<Vertex> _graph, decltype(Vertex::Id) _cliqueSize, Clique::FindOperation _op, Clique::CliqueHandler *_handler)
	{
		if (IsCorrupt(_graph))
			throw "invalid _graph.";

	  /*
		*	_blockSize:
		*			ResourceManager.m_Stack		:		2 * 32 * sizeof(int);
		*			ResourceManager.m_BitSet,2	:		3 * GetQWordAlignedSizeForBits(_graph.size());
		*
		*		[
		*			Ext::Array<Vertex>			:		6 * GetQWORDAlignedSize(_graph.size() * sizeof(ID));
		*												:		4 * GetQWordAlignedSizeForBits(_graph.size());
		*		] * FramesPerBlock * 2
		*/
		UInt32	bitSetLength = (UInt32)GetQWordAlignedSizeForBits(_graph.size());
		Clique::ResourceManager	resourceManager((decltype(Vertex::Id))_graph.size(), (UInt32)(2 * 32 * sizeof(int) + bitSetLength * 3 + (3 * GetQWordAlignedSize(_graph.size() * sizeof(ID)) + 3 * bitSetLength) * Clique::FramesPerBlock * 2));

		ReleaseMemoryToPool dtor(resourceManager.MemoryPool, resourceManager.MemoryPool.Allocate(GetQWordAlignedSize(_graph.size() * sizeof(ID)) + Ext::Unsafe::ArrayOfSet<Clique::ElementData, decltype(Vertex::Id)>::GetAllocationSize(_graph.size())));
		ReleaseMemoryToPool dtor2(resourceManager.GraphMemoryPool, resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(_graph.size())));

		ID		*originalVertexId = (ID*)dtor.ptr();
		Ext::Unsafe::ArrayOfSet<Clique::ElementData, decltype(Vertex::Id)> cliqueMembers(((byte*)originalVertexId) + GetQWordAlignedSize(_graph.size() * sizeof(ID)), (ID)_graph.size());
		void* pGraphMemory = dtor2.ptr();

		decltype(Vertex::Id) i;

		char sz[512];
		sprintf_s(sz, sizeof(sz), "%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n", "Vertices", "Clique", "Clique-R", "Ticks", "Calls", "PartitionExtr", "TwoNHits", "TwoNColorHits", "SubgraphHits", "BtmUpHits", "BtmUpHits2", "BtmUpCheck", "BtmUpCheck2", "Count9", "Count11", "Count12", "Count13", "Count14");
		TraceMessage(sz);

		for (i = 0; i < _graph.size(); i++)
			originalVertexId[i] = i;

		auto graph = CloneGraph(_graph, pGraphMemory);

		cliqueMembers.ZeroMemory();
		AddSelfEdges(graph);
		resourceManager.This.PrintStatistics = true;

		auto ticks = GetCurrentTick();
		auto cliqueSize = (_cliqueSize == INVALID_ID) ? 0 : _cliqueSize;
		auto result = TryFindClique(graph, originalVertexId, cliqueMembers, 0, cliqueSize, _op, nullptr, 0, 0, _handler, resourceManager);

		if (result == Ext::BooleanError::Error)
			return INVALID_ID;

		if (result == Ext::BooleanError::False)
			cliqueSize = (cliqueSize > ((_cliqueSize == INVALID_ID) ? 0 : _cliqueSize)) ? (cliqueSize - 1) : 0;

		if ((result == Ext::BooleanError::True) || (cliqueSize >= 3))
		{
			Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(resourceManager.This.CliqueMembers, nullptr, 0, cliqueSize, true, resourceManager.Stack);
			sprintf_s(sz, sizeof(sz), "CliqueSize: %d\r\n", cliqueSize);
			TraceMessage(sz);
			PrintArray(resourceManager.This.CliqueMembers, cliqueSize, TraceMessage);

			assert(IsClique(_graph, resourceManager.This.CliqueMembers, cliqueSize, resourceManager.BitSet));
		}

		sprintf_s(sz, sizeof(sz), "%15d %15d %15d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", (int)graph.size(), cliqueSize, _cliqueSize, GetCurrentTick() - ticks, resourceManager.Calls, resourceManager.PartitionExtr, resourceManager.TwoNHits, resourceManager.TwoNColorHits, resourceManager.SubgraphHits, resourceManager.BtmUpHits, resourceManager.BtmUpHits2, resourceManager.BtmUpCheck, resourceManager.BtmUpCheck2, resourceManager.Count9, resourceManager.Count11, resourceManager.Count12, resourceManager.Count13, resourceManager.Count14);
		TraceMessage(sz);

		for (i = 0; (resourceManager.CallFrame[i].callCount > 0); i++)
		{
			sprintf_s(sz, sizeof(sz), "%15d Calls:%15I64d\n", (int)i + 1, resourceManager.CallFrame[i].callCount);
			TraceMessage(sz);
		}

		return cliqueSize;
	}

	decltype(Vertex::Id) GetClusters(Ext::Array<Vertex> _graph, 
				Ext::ArrayOfArray<decltype(Vertex::Id), decltype(Vertex::Id)> *_pClusters,
				Ext::Array<ID> _vertexColor, decltype(Vertex::Id) _cliqueSize,
				decltype(Vertex::Id) _depth, Clique::ResourceManager& _resourceManager)
	{
		Clique::TryFindCliqueThisContext ThisObject = _resourceManager.This;

		UInt32	bitSetLength = (UInt32)GetQWordAlignedSizeForBits(_graph.size());
		ReleaseMemoryToPool dtor(_resourceManager.MemoryPool, _resourceManager.MemoryPool.Allocate(GetQWordAlignedSize(_graph.size() * sizeof(ID)) * 3 + Ext::Unsafe::ArrayOfSet<Clique::ElementData, ID>::GetAllocationSize(_graph.size())));

		ID		*vertexColor = (ID*)dtor.ptr();
		ID		*cliqueMembers2 = (ID*)(((byte*)vertexColor) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		ID		*originalVertexId = (ID*)(((byte*)cliqueMembers2) + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		Ext::Unsafe::ArrayOfSet<Clique::ElementData, ID> cliqueMembers((((byte*)originalVertexId) + GetQWordAlignedSize(_graph.size() * sizeof(ID))), (ID)_graph.size());

		decltype(Vertex::Id) i, j, color = 0, cliqueSize = ((3 <= _cliqueSize) && (_cliqueSize <= _graph.size())) ? _cliqueSize : 0;
		size_t vertexColored = 0;

		for (i = 0; i < _graph.size(); i++)
		{
			vertexColor[i] = 0;
			originalVertexId[i] = i;
		}

		while (vertexColored < _graph.size())
		{
			for (i = 0; i < _graph.size(); i++)
			{
				if (_graph[i].Count != 1)
					continue;

				assert(vertexColor[i] == 0);
				color++;
				vertexColor[i] = color;
				vertexColored++;

				if (_pClusters != nullptr)
				{
					_pClusters->InitSet(_pClusters->setCount(), 1);
					_pClusters->Get(_pClusters->setCount() - 1, 0) = i;
				}

				_graph[i].Count = 0;
				BitReset(_graph[i].Neighbours, i);
			}

			for (i = 0; i < _graph.size(); i++)
			{
				if (_graph[i].Count != 2)
					continue;

				assert(vertexColor[i] == 0);
				_graph[i].Count = 0;
				BitReset(_graph[i].Neighbours, i);

				byte *ptr;
				for (ptr = _graph[i].Neighbours; *ptr == 0; ptr++);
				j = ((ID)(ptr - _graph[i].Neighbours) * 8) + Bit::FirstLSB[*ptr];

				BitReset(_graph[i].Neighbours, j);
				assert(vertexColor[j] == 0);

				color++;
				vertexColor[i] = color;
				vertexColor[j] = color;
				vertexColored += 2;

				if (_pClusters != nullptr)
				{
					_pClusters->InitSet(_pClusters->setCount(), 2);
					_pClusters->Get(_pClusters->setCount() - 1, 0) = (i < j) ? i : j;
					_pClusters->Get(_pClusters->setCount() - 1, 1) = (i < j) ? j : i;
				}

				_graph[j].Count = 0;
				ptr = _graph[j].Neighbours;
				BitReset(ptr, i);
				BitReset(ptr, j);

				for (ID k = 0; k < _graph.size(); k++)
				{
					if (!BitTest(ptr, k))
						continue;

					_graph[k].Count--;
					BitReset(_graph[k].Neighbours, j);
					BitReset(ptr, k);
				}

				break;
			}

			if (vertexColored == _graph.size())
				break;
			if (i < _graph.size()) // colored two vertices.
				continue;

			// cliqueMembers.ZeroMemory();

			_resourceManager.This.ctor(cliqueMembers2, _depth, 0, false);

			auto op = (cliqueSize == 0) ? Clique::FindOperation::MaximumClique : Clique::FindOperation::ExactSearch;
			auto result = TryFindClique(_graph, originalVertexId, cliqueMembers, 0, cliqueSize, op, nullptr, 0, _depth, nullptr, _resourceManager);
			if ((result == Ext::BooleanError::True) || (op == Clique::FindOperation::MaximumClique))
			{
				bool isExactSearch = (op == Clique::FindOperation::ExactSearch);
				if (op == Clique::FindOperation::MaximumClique)
					cliqueSize--;

				color++;

				for (i = 0; i < cliqueSize; i++)
				{
					j = isExactSearch ? cliqueMembers.GetValue(i, 0).OriginalVertexId : _resourceManager.This.CliqueMembers[i];
					_resourceManager.lId[i] = j;
					assert(vertexColor[j] == 0);
					vertexColor[j] = color;
				}

				Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(_resourceManager.lId, nullptr, 0, cliqueSize, true, _resourceManager.Stack);

				if (_pClusters != nullptr)
				{
					_pClusters->InitSet(_pClusters->setCount(), cliqueSize);
					for (i = 0; i < cliqueSize; i++)
						_pClusters->Get(_pClusters->setCount() - 1, i) = _resourceManager.lId[i];
				}

				vertexColored += cliqueSize;
				for (i = 0; i < cliqueSize; i++)
				{
					j = _resourceManager.lId[i];

					_graph[j].Count = 0;
					auto ptr = _graph[j].Neighbours;
					BitReset(ptr, j);

					for (ID k = 0; k < _graph.size(); k++)
					{
						if (!BitTest(ptr, k))
							continue;

						_graph[k].Count--;
						BitReset(_graph[k].Neighbours, j);
						BitReset(ptr, k);
					}
				}
			}
			else
			{
				if (op == Clique::FindOperation::ExactSearch)
					cliqueSize = 0;
				else // if (op == Clique::FindOperation::MaximumClique)
					cliqueSize--;
			}
		}

		for (i = 0; (i < _graph.size()) && (i < _vertexColor.size()); i++)
			_vertexColor[i] = vertexColor[i];

		_resourceManager.This = ThisObject;

		return color;
	}

	decltype(Vertex::Id) GetIndependentSets(Ext::Array<Vertex> _graph, Ext::ArrayOfArray<Graph::ID, Graph::ID> *_pSets,
						Ext::Array<ID> _vertexColor, decltype(Vertex::Id) _cliqueSize)
	{
		if (IsCorrupt(_graph))
			throw "invalid _graph.";

		/*
		*	_blockSize:
		*			ResourceManager.m_Stack		:		2 * 32 * sizeof(int);
		*			ResourceManager.m_BitSet	:		2 * GetQWordAlignedSizeForBits(_graph.size());
		*
		*		[
		*			Ext::Array<Vertex>			:		GetGraphAllocationSize(_graph.size());
		*												:		5 * GetQWORDAlignedSize(_graph.size() * sizeof(ID));
		*												:		4 * GetQWordAlignedSizeForBits(_graph.size());
		*		] * 5
		*/
		UInt32	bitSetLength = (UInt32)GetQWordAlignedSizeForBits(_graph.size());
		Clique::ResourceManager	resourceManager((decltype(Vertex::Id))_graph.size(), (UInt32)(2 * 32 * sizeof(int) + bitSetLength * 3 + (3 * GetQWordAlignedSize(_graph.size() * sizeof(ID)) + 3 * bitSetLength) * Clique::FramesPerBlock * 2));
		ReleaseMemoryToPool dtor(resourceManager.GraphMemoryPool, resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(_graph.size())));

		auto graph = CloneGraph(_graph, dtor.ptr());
		AddSelfEdges(graph);
		Graph::ComplementGraph(graph, graph);

		auto colors = GetClusters(graph, _pSets, _vertexColor, _cliqueSize, 0, resourceManager);

		// if (_vertexColor != nullptr) assert(IsValidColoring(_graph, _vertexColor));

		return colors;
	}

#define TryFindClique_Recursion1

	// Top level invokation parameters
	//		_graph : AddSelfEdges(_graph) must be called
	//		_cliqueMembersCount : 0
	//		_targettedVertices : nullptr is allowed when _targettedVerticesCount is 0
	//		_targettedVerticesCount : default(0)
	//		_resourceManager.This.ctor() or _resourceManager.This.PrepareForInvoke() must be called
	//			when top level is called more than once.
	//			_resourceManager.ctor() calls _resourceManager.This.ctor(). So no need for single invokation.
	//

	Ext::BooleanError TryFindClique(Ext::Array<Vertex> _graph, ID *_originalVertexId,
				Ext::Unsafe::ArrayOfSet<Clique::ElementData, ID>& _cliqueMembers,
				decltype(Vertex::Id) _cliqueMembersCount,
				decltype(Vertex::Id)& _cliqueSize, Clique::FindOperation _op,
				byte* _targettedVertices, decltype(Vertex::Id) _targettedVerticesCount,
				decltype(Vertex::Id) _depth, Clique::CliqueHandler *_handler,
				Clique::ResourceManager& _resourceManager)
	{
		byte *activeVertexList, *activeNeighbours, *pActiveNeighbours;
		decltype(Vertex::Id)	*vertexId, *vertexEdgeCount, *ids;
		decltype(Vertex::Id)	activeVertexCount, cliqueSize, cliqueVertexCount, bitSetLength;
		decltype(Vertex::Id)	id, i, j, k, l, n;
		Ext::BooleanError isCliqueExist;
		Clique::TryFindCliqueCallFrame *frame;

		Ext::Array<Vertex> graph;
		auto ticks = GetCurrentTick();

	Enter:

		bitSetLength = (decltype(Vertex::Id))GetQWordAlignedSizeForBits(_graph.size());

		activeVertexList = (byte*) _resourceManager.MemoryPool.Allocate(bitSetLength * 2 + GetQWordAlignedSize(_graph.size() * sizeof(ID)) * 2);
		activeNeighbours = activeVertexList + bitSetLength;
		vertexId = (decltype(Vertex::Id)*)(activeNeighbours + bitSetLength);
		vertexEdgeCount = (decltype(Vertex::Id)*)((byte*)vertexId + GetQWordAlignedSize(_graph.size() * sizeof(ID)));
		isCliqueExist = Ext::BooleanError::False;
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

		if (s_ShouldSaveGraph && (_depth >= s_SaveGraphDepth))
		{
			SaveDIMACSGraph(s_SaveGraphPath, _graph, "GraphSave");
			s_ShouldSaveGraph = false;
		}

		// Sort vertices in DESC order based on vertex degree
		for (i = 0, j = (ID)_graph.size(), k = 0; i < _graph.size(); i++)
		{
			n = _graph[i].Count;
			if (n > 0)
			{
				vertexId[k] = _graph[i].Id;
				vertexEdgeCount[k] = n;
				k++;
			}
			else
			{
				j--;
				vertexId[j] = _graph[i].Id;
				vertexEdgeCount[j] = 0;
			}
		}

		Sort<decltype(Vertex::Id), decltype(Vertex::Id), Int32>(vertexEdgeCount, vertexId, 0, (decltype(Vertex::Id))_graph.size(), false, _resourceManager.Stack);

		// start with all vertices as active.
		SetNBits((UInt64*)activeVertexList, _graph.size());

		cliqueSize = _cliqueSize;

		for (activeVertexCount = (decltype(i))_graph.size(); (0 < activeVertexCount) && (cliqueSize <= activeVertexCount); )
		{
			// GetClusters passes graph where self edge will not be present to imply exclusion of the vertex.
			if (vertexEdgeCount[activeVertexCount - 1] == 0)
			{
				activeVertexCount--;
				BitReset(activeVertexList, vertexId[activeVertexCount]);
				continue;
			}

#pragma region Top down processing
			// Top down processing only when all remaining vertices have at least degree of clique-size
			auto cliqueVertexCountAtStart = cliqueVertexCount;

			while ((0 < activeVertexCount) && (cliqueSize <= activeVertexCount) && (cliqueSize <= vertexEdgeCount[activeVertexCount - 1]))
			{
				k = activeVertexCount - vertexEdgeCount[0];
				_cliqueMembers.InitSet(_cliqueMembersCount + cliqueVertexCount);
				_cliqueMembers.SetSetSize(_cliqueMembersCount + cliqueVertexCount, k + 1);

				ids = _resourceManager.lId;
				ID m;
				
				for (m = 0; (m < activeVertexCount) && (vertexEdgeCount[m] == vertexEdgeCount[0]); m++)
				{
					id = vertexId[m];
					_cliqueMembers.GetValue(_cliqueMembersCount + cliqueVertexCount, 0).SetValue(id, _originalVertexId[id], (byte)Clique::PartitionVertexStatus::ConnectedToAll);

					pActiveNeighbours = _graph[id].Neighbours;
					ids[0] = id;

					for (j = 1, i = 0, l = 0; (i < activeVertexCount) && (j <= k); i++)
					{
						if (BitTest(pActiveNeighbours, vertexId[i]))
							continue;

						ids[j] = vertexId[i];

						n = (byte)Clique::PartitionVertexStatus::PartialUnverified;
						if (vertexEdgeCount[i] == vertexEdgeCount[0])
						{
							l = j;
							n = (byte)Clique::PartitionVertexStatus::ConnectedToAll;
						}

						// A vertex with ((vertexEdgeCount[i] < vertexEdgeCount[0])) is not connected to rest of the vertices, still it could be part of the clique.
						_cliqueMembers.GetValue(_cliqueMembersCount + cliqueVertexCount, j).SetValue(vertexId[i], _originalVertexId[vertexId[i]], n);
						j++;
					}

					n = l + 1;

					for (i = 2; (i <= k); i++)
					{
						pActiveNeighbours = _graph[ids[i]].Neighbours;
						for (j = 1; (j < i) && !BitTest(pActiveNeighbours, ids[j]); j++);
						if (j < i)
							break;
					}

					if (i > k)
						break; // valid partition;
				}

				if ((m == activeVertexCount) || (vertexEdgeCount[m] < vertexEdgeCount[0]))
					goto LoopExit;

				_resourceManager.PartitionExtr++;

				id = ids[0];
				pActiveNeighbours = _graph[id].Neighbours;
				for (i = 0, j = 0, l = 0; j < activeVertexCount; j++)
				{
					if ((id != vertexId[j]) && BitTest(pActiveNeighbours, vertexId[j]))
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
					// (id < n) : There exist a non targetted vertex in this partition which is connected to rest of the _graph.
					// (id >= n) : all ConnectedToAll vertices in this partition are targetted vertices.
					if ((_targettedVerticesCount == 0) && (id < n))
					{
						_resourceManager.Count9++;
						goto ExitOutermostLoop;	// done processing this _graph
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
					else if ((_cliqueMembersCount + cliqueVertexCount) > _resourceManager.This.CliqueSize)
					{
						_resourceManager.This.CliqueSize = _cliqueMembersCount + cliqueVertexCount;
						for (i = 0; i < _resourceManager.This.CliqueSize; i++)
							_resourceManager.This.CliqueMembers[i] = _cliqueMembers.GetValue(i, 0).OriginalVertexId;

						if (_op == Clique::FindOperation::MaximumClique)
							cliqueSize++; // Lets look for the next larger clique.
					}
				}

				break;
			}


#pragma region Check whether clique is possible
			if (activeVertexCount < (cliqueSize << 1))		// (activeVertexCount < (2 * cliqueSize))
			{
				Int64 edgeTotal = 0, edgeTotal2 = 0;
				for (i = 0; i < cliqueSize; i++)
					edgeTotal += vertexEdgeCount[i];

				for (; i < activeVertexCount; i++)
					edgeTotal2 += vertexEdgeCount[i];

				i = activeVertexCount - cliqueSize;
				edgeTotal -= cliqueSize * cliqueSize;
				edgeTotal2 -= i * i;
				if (edgeTotal < edgeTotal2)
				{
					_resourceManager.TwoNHits++;
					break;
				}
				else if ((cliqueSize > 32) && (vertexEdgeCount[activeVertexCount - 1] > cliqueSize) && 
					      ((edgeTotal - vertexEdgeCount[activeVertexCount - 1]) > edgeTotal2))
				{
					// (cliqueSize > 32) : cost of CreateGraph() is acceptable.
					// (vertexEdgeCount[activeVertexCount - 1] > cliqueSize) : All vertices have at least cliqueSize edges.
					// (edgeTotal > edgeTotal2) : If equal then removing one vertices would be enough.
					graph = CreateGraph(activeVertexCount, _resourceManager.AllocateGraphMemory(activeVertexCount));
					ExtractGraph(_graph, graph, activeVertexList, _resourceManager.BitSet);
					ComplementGraph(graph, graph);

					// GetClusters needs to be replaced with faster one.
					auto colors = GetClusters(graph, nullptr, Ext::Array<Graph::ID>(nullptr, 0), 0, _depth + 1, _resourceManager);
					_resourceManager.GraphMemoryPool.Free(graph.ptr());
					if (colors < cliqueSize)
					{
						_resourceManager.TwoNColorHits++;
						break;
					}

					_resourceManager.Count11++;
				}
			}
#pragma endregion

			i = vertexEdgeCount[0];
			j = vertexEdgeCount[activeVertexCount - 1];

			// ToDo: Fix vertex selection criteria
			auto pivotVertexIdx = (((activeVertexCount - vertexEdgeCount[0]) == 2) && (activeVertexCount > 32)) ? (decltype(Vertex::Id))0 : (activeVertexCount - 1);

			auto cliqueVertexCount2 = (decltype(cliqueVertexCount))0;
			auto subCliqueSize = cliqueSize;
			id = vertexId[pivotVertexIdx];
			auto subGraphSize = (decltype(Vertex::Id)) PopCountAandB_Set((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)activeNeighbours, (bitSetLength >> 3));
			decltype(Vertex::Id) id2;

			ids = _resourceManager.lId;

			{
				BitReset(activeNeighbours, id);
				subGraphSize--;
				if (0 < subCliqueSize)
					subCliqueSize--;
				ids[cliqueVertexCount2++] = id;
			}

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
				id = vertexId[pivotVertexIdx];
				n = vertexEdgeCount[pivotVertexIdx] - 1;

				_cliqueMembers.CreateTrivialSet(_cliqueMembersCount + cliqueVertexCount).SetValue(id, _originalVertexId[id]);

				if (n == subGraphSize)
					pActiveNeighbours = activeNeighbours;
				else
				{
					pActiveNeighbours = _resourceManager.BitSet;
					AandB((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)pActiveNeighbours, (bitSetLength >> 3));
					BitReset(pActiveNeighbours, id);
				}

				j = 1;
				k = (pivotVertexIdx == 0) ? 1 : -1;
				for (i = (pivotVertexIdx == 0) ? 0 : (activeVertexCount - 1); ((i += k) >= 0) && (i < activeVertexCount) && (vertexEdgeCount[i] == (n + 1)); )
				{
					id2 = vertexId[i];
					if (BitTest(pActiveNeighbours, id2)/* || (id2 == ids[0])*/)
						continue;

					if (PopCountAandB((UInt64*)pActiveNeighbours, (UInt64*)_graph[id2].Neighbours, (bitSetLength >> 3)) == n)
						_cliqueMembers.GetValue(_cliqueMembersCount + cliqueVertexCount, j++).SetValue(id2, _originalVertexId[id2], (byte)Clique::PartitionVertexStatus::ConnectedToAll);
				}

				_cliqueMembers.SetSetSize(_cliqueMembersCount + cliqueVertexCount, j);

				for (i = 1, j = _cliqueMembersCount + cliqueVertexCount + i; i < cliqueVertexCount2; i++, j++)
					_cliqueMembers.CreateTrivialSet(j).SetValue(ids[i], _originalVertexId[ids[i]]);
			}

			auto skipTargettedVerticesForSubgraph = true;
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

					skipTargettedVerticesForSubgraph = (i == k);
				}
			}

			cliqueVertexCount2 += cliqueVertexCount;

			_resourceManager.SubgraphHits++;

			if ((0 < subGraphSize) && (subCliqueSize <= subGraphSize) && ((0 < subCliqueSize) || (_op != Clique::FindOperation::ExactSearch)))
			{
				isExist = false;
				id = vertexId[pivotVertexIdx];

				decltype(Vertex::Id) commonCount = 0, activeNeighboursCount = subGraphSize + (cliqueVertexCount2 - cliqueVertexCount);
				decltype(Vertex::Id) commonCountMax = 0, commonCountMaxId = INVALID_ID;

				if ((cliqueVertexCountAtStart < cliqueVertexCount) || ((subGraphSize + (cliqueVertexCount2 - cliqueVertexCount)) < vertexEdgeCount[pivotVertexIdx]))
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
								for (j = _cliqueMembersCount + l, n = 0; j < (_cliqueMembersCount + cliqueVertexCount); j++)
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

					if (commonCount == activeVertexCount) // no more cliques in this _graph.
						break;
				}

				if (commonCount < activeNeighboursCount)
				{
					auto targettedVertices = (byte*)_resourceManager.MemoryPool.Allocate(GetQWordAlignedSize(subGraphSize * sizeof(ID)) + GetQWordAlignedSizeForBits(subGraphSize));
					auto originalVertexId = (decltype(Vertex::Id)*)(targettedVertices + GetQWordAlignedSizeForBits(subGraphSize));

					graph = CreateGraph(subGraphSize, _resourceManager.AllocateGraphMemory(subGraphSize));
					ExtractGraph(_graph, graph, activeNeighbours, _resourceManager.BitSet);

					for (j = 0, i = 0; i < _graph.size(); i++)
					{
						if (BitTest(activeNeighbours, i))
							originalVertexId[j++] = _originalVertexId[i];
					}

					decltype(Vertex::Id) targettedVerticesCount;
					if (skipTargettedVerticesForSubgraph || ((_targettedVerticesCount == 0) && (commonCountMaxId == INVALID_ID)))
					{
						ZeroMemoryPack8(targettedVertices, GetQWordAlignedSizeForBits(subGraphSize));
						targettedVerticesCount = 0;
					}
					else if (_targettedVerticesCount > 0)
						targettedVerticesCount = (decltype(Vertex::Id))ExtractBits(_targettedVertices, targettedVertices, activeNeighbours, _resourceManager.BitSet, _graph.size());
					else // if (commonCountMaxId != INVALID_ID)
						targettedVerticesCount = (decltype(Vertex::Id))ExtractBitsFromComplement(_graph[commonCountMaxId].Neighbours, targettedVertices, activeNeighbours, _resourceManager.BitSet, _graph.size());

					frame->pivotVertexIdx = pivotVertexIdx;
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
					isExist = TryFindClique(graph, originalVertexId, _cliqueMembers, _cliqueMembersCount + cliqueVertexCount2, subCliqueSize, _op, targettedVertices, targettedVerticesCount, _depth + 1, _handler, _resourceManager);

					// Possible memory reuse for _graph allocation.
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

					isExist = (isCliqueExist == Ext::BooleanError::True);
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
					pivotVertexIdx = frame->pivotVertexIdx;
#pragma endregion
#endif
					_resourceManager.MemoryPool.Free(targettedVertices);
					_resourceManager.GraphMemoryPool.Free(graph.ptr());

					if (isCliqueExist == Ext::BooleanError::Error)
						goto ReturnOnError;

					// If the _graph memory is taken for sub-graph storage, recreate _graph from input _graph.
					if (_graph.ptr() == nullptr)
					{
						// assert(_depth > _resourceManager.This.ZeroReferenceDepth); // The input _graph at depth 0 should never be touched.
						frame = &_resourceManager.CallFrame[0];
						pActiveNeighbours = _resourceManager.BitSet2;
						ZeroMemoryPack8(pActiveNeighbours, frame->bitSetLength);
						for (i = 0; i < _graph.size(); i++)
							BitSet(pActiveNeighbours, _originalVertexId[i]);

						_graph = CreateGraph((decltype(id))_graph.size(), _resourceManager.GraphMemoryPool.Allocate(GetGraphAllocationSize(_graph.size())));
						ExtractGraph(frame->_graph, _graph, pActiveNeighbours, _resourceManager.BitSet);

						_resourceManager.This.TopGraphForMemoryReclaim = _depth;
					}
				}
				else
				{
					// ToDo: Check whether this vertex forms clique or not.
					_resourceManager.BtmUpHits2++;
				}
			}

			// if (cliqueSize < ((cliqueVertexCount2 - cliqueVertexCount) + subCliqueSize))
				cliqueSize = (cliqueVertexCount2 - cliqueVertexCount) + subCliqueSize;

			if (isExist)
			{
				if (subGraphSize == 0)
				{
					if (_op == Clique::FindOperation::EnumerateCliques)
					{
						// Invoke callback to process current clique set.
					}
					else if ((_cliqueMembersCount + cliqueVertexCount2) > _resourceManager.This.CliqueSize)
					{
						_resourceManager.This.CliqueSize = _cliqueMembersCount + cliqueVertexCount2;
						for (i = 0; i < _resourceManager.This.CliqueSize; i++)
							_resourceManager.This.CliqueMembers[i] = _cliqueMembers.GetValue(i, 0).OriginalVertexId;

						if (_op == Clique::FindOperation::MaximumClique)
						{
							cliqueSize++; // Lets look for the next larger clique.
							isExist = false;
						}
					}
				}

				isCliqueExist = isExist ? Ext::BooleanError::True : Ext::BooleanError::False;
				if (_op == Clique::FindOperation::ExactSearch)
					goto Return; // isCliqueExist can't be set to false after this since it is true now. Note: Refer before 'Return:' label. 
			}


#pragma region Remove processed vertex and similar subgraphs
			Ext::Unsafe::CircularQueue<decltype(Vertex::Id)> queue(_resourceManager.lId2, _graph.size());
			byte *queuedVertices = _resourceManager.BitSet;

			ZeroMemoryPack8(queuedVertices, bitSetLength);

			if ((_targettedVerticesCount > 0) && BitTest(_targettedVertices, vertexId[pivotVertexIdx]) && (--_targettedVerticesCount == 0))
			{
				_resourceManager.Count9++;
				goto ExitOutermostLoop;	// done processing this _graph
			}

			if (pivotVertexIdx < (activeVertexCount - 1))
			{
				j = vertexId[pivotVertexIdx];
				k = vertexEdgeCount[pivotVertexIdx];
				for (i = pivotVertexIdx; ++i < activeVertexCount; )
				{
					vertexId[i - 1] = vertexId[i];
					vertexEdgeCount[i - 1] = vertexEdgeCount[i];
				}
				vertexId[activeVertexCount - 1] = j;
				vertexEdgeCount[activeVertexCount - 1] = k;
			}

			while ((0 < activeVertexCount) && (cliqueSize <= --activeVertexCount))
			{
				id = vertexId[activeVertexCount];

				subGraphSize = vertexEdgeCount[activeVertexCount];
				vertexEdgeCount[activeVertexCount] = (cliqueVertexCount + cliqueSize) - (isExist ? 0 : 1);
				AandB((UInt64*)activeVertexList, (UInt64*)_graph[id].Neighbours, (UInt64*)activeNeighbours, (bitSetLength >> 3));
				// assert(subGraphSize, PopCount((UInt64*)activeNeighbours, (bitSetLength >> 3)));
				BitReset(activeVertexList, id);

				if (subGraphSize > 1)
				{
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
						if (isConnected ? ((n == vertexEdgeCount[i]) || (n == vertexEdgeCount[i] - 1)) : (n == vertexEdgeCount[i] - 1))
						{
							queue.push(id2);
							BitSet(queuedVertices, id2);
							_resourceManager.BtmUpHits++;

							if ((_targettedVerticesCount > 0) && BitTest(_targettedVertices, id2) && (--_targettedVerticesCount == 0))
							{
								_resourceManager.Count9++;
								goto ExitOutermostLoop;	// done processing this _graph
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

			if (_resourceManager.This.PrintStatistics && (_depth == _resourceManager.This.ZeroReferenceDepth))
			{
				char sz[512];
				sprintf_s(sz, sizeof(sz), "%15d %15d %15d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d %15I64d\n", (int)graph.size(), cliqueSize, _cliqueSize, GetCurrentTick() - ticks, _resourceManager.Calls, _resourceManager.PartitionExtr, _resourceManager.TwoNHits, _resourceManager.TwoNColorHits, _resourceManager.SubgraphHits, _resourceManager.BtmUpHits, _resourceManager.BtmUpHits2, _resourceManager.BtmUpCheck, _resourceManager.BtmUpCheck2, _resourceManager.Count9, _resourceManager.Count11, _resourceManager.Count12, _resourceManager.Count13, _resourceManager.Count14);
				TraceMessage(sz);

				for (i = 0; (_resourceManager.CallFrame[i].callCount > 0); i++)
				{
					sprintf_s(sz, sizeof(sz), "%15d Calls:%15I64d\n", (int)i + 1, _resourceManager.CallFrame[i].callCount);
					TraceMessage(sz);
				}
			}

			if (s_IsDiagnosticsEnabled && (_depth >= s_DiagnosticsDepth))
			{
				PrintCallFramDiagnosticsInfo(_depth, _resourceManager, TraceMessage);
				s_IsDiagnosticsEnabled = false;
			}
		}
	ExitOutermostLoop:

		if ((_op != Clique::FindOperation::MaximumClique) &&
			((cliqueVertexCount >= _cliqueSize) || ((cliqueVertexCount + cliqueSize) > _cliqueSize)))
			isCliqueExist = Ext::BooleanError::True;

	Return:
		if (_cliqueSize < (cliqueVertexCount + cliqueSize))
			_cliqueSize = (cliqueVertexCount + cliqueSize);

	ReturnOnError:
		_resourceManager.MemoryPool.Free(activeVertexList);

#if (!defined(TryFindClique_Recursion))
		if (_depth > _resourceManager.This.ZeroReferenceDepth)
			goto ReturnTo;
#endif

		return isCliqueExist;
	}
}
