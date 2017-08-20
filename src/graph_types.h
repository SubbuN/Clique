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

	namespace SAT
	{
		enum struct AssignmentState : byte
		{
			Assigned = 0,				// final node
			PureVariable = 1,			// final node

			AssignedFirst = 2,		// intermediate node
			AssignedSecond = 3,		// intermediate node

			Unassigned = 0xFF,		// Start node
		};

		enum struct TruthValue : byte
		{
			False = 0,
			True = 1,
			Null = 2,
			Unassigned = 0xFF,
		};

		struct VariableAssignmentOverride
		{
			ID Variable;
			ID Order;
			AssignmentState State;
			TruthValue RequiredLiteral, IsRequired;

			VariableAssignmentOverride()
				: Variable(0), Order(0), State(AssignmentState::Unassigned),
				RequiredLiteral(TruthValue::Unassigned), IsRequired(TruthValue::Unassigned)
			{
			}

			VariableAssignmentOverride(ID variable, AssignmentState state, ID order, TruthValue requiredLiteral, TruthValue isRequired)
			{
				Variable = variable;
				State = state;
				Order = order;
				RequiredLiteral = requiredLiteral;
				IsRequired = isRequired;
			}
		};

		struct VariableAssignment
		{
			ID Order;
			TruthValue Value;
			AssignmentState State;
			TruthValue RequiredLiteral, IsRequired;

			void Set(TruthValue _value, AssignmentState _state, ID _order)
			{
				Order = _order;
				Value = _value;
				State = _state;
			}

			void Set(VariableAssignmentOverride& override)
			{
				Order = override.Order;
				Value = TruthValue::Unassigned;
				State = override.State;
				RequiredLiteral = override.RequiredLiteral;
				IsRequired = override.IsRequired;
			}

			void CopyTo(VariableAssignmentOverride& override, ID variable)
			{
				override.Order = Order;
				override.Variable = variable;
				override.State = State;
				override.RequiredLiteral = RequiredLiteral;
				override.IsRequired = IsRequired;
			}

			VariableAssignmentOverride ToOverride(ID variable)
			{
				return VariableAssignmentOverride(variable, State, Order, RequiredLiteral, IsRequired);
			}
		};

		struct Formula
		{
			ID	Variables;
			Ext::ArrayOfArray<int, ID> Clauses;

			Formula()
				: Variables(0), Clauses(nullptr, 0)
			{
			};

			Formula(ID variables, Ext::ArrayOfArray<int, ID>& clauses)
				: Variables(variables), Clauses(clauses)
			{
			};

			Formula(Formula& that)
				: Variables(that.Variables), Clauses(that.Clauses)
			{
			};

			void ctor(ID variables, Ext::ArrayOfArray<int, ID>& clauses)
			{
				Variables = variables;
				Clauses = clauses;
			};
		};

		struct  FormulaNode
		{
			int	Literal;
			ID		Clause;
		};
	};
}


#endif

