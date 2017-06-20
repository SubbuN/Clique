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

#if (!Graph_Types_H)
#define Graph_Types_H

#include "Types.h"
#include <stdint.h>

namespace Graph
{
typedef unsigned __int32 ID;
#define INVALID_ID UINT32_MAX

	struct Vertex
	{
		byte* Neighbours;
		ID		Id;
		ID		Count;

		Vertex()
			: Neighbours(nullptr), Id(0), Count(0)
		{
		};
	};

	struct Vertex2
	{
		union
		{
			UInt64 lNeighbours;
			byte Neighbours[8];
		};
		ID		Id;
		ID		Count;

		Vertex2()
			: lNeighbours(0), Id(0), Count(0)
		{
		};
	};

	struct SATFormula
	{
		ID	Variables;
		ID	Clauses;
		ID	Nodes;
		int*	pData;

		SATFormula()
			: pData(nullptr), Variables(0), Clauses(0), Nodes(0)
		{
		};

		ID getClauseSize(ID clause)
		{
			return (clause >= Clauses) ? 0 : (ID)(pData[clause + 1] - pData[clause]);
		};

		ID getClauseStartNode(ID clause)
		{
			return (clause >= Clauses) ? INVALID_ID : (ID)pData[clause];
		}

		int* getClause(ID clause)
		{
			return (clause >= Clauses) ? nullptr : &pData[Clauses + 1 + pData[clause]];
		}
	};
}


#endif

