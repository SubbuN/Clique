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

	TextStream TraceMessage = NullTextStream;

	void SetTraceMessageHandler(TextStream textStream)
	{
		TraceMessage = (textStream != nullptr) ? textStream : NullTextStream;
	}

	void PrintArray(ID *list, size_t size, TextStream traceMessage, char *format)
	{
		if (traceMessage == nullptr)
			return;

		if (format == nullptr)
			format = "%d";

		char line[512 + 32];
		int length = 0;

		for (size_t i = 0; (i < size); i++)
		{
			length += sprintf_s(line + length, sizeof(line) - length, format, list[i]);
			line[length++] = ' ';
			line[length] = 0;

			if ((sizeof(line) - length) < 32)
			{
				traceMessage(line);
				length = 0;
			}
		}

		length += sprintf_s(line + length, sizeof(line) - length, "\r\n");
		traceMessage(line);
	}

	void PrintArrayIdexValue(ID *list, size_t size, TextStream traceMessage, char *format)
	{
		if (traceMessage == nullptr)
			return;

		if (format == nullptr)
			format = "%d:%d";

		char line[512 + 32];
		int length = 0;

		for (size_t i = 0; (i < size); i++)
		{
			length += sprintf_s(line + length, sizeof(line) - length, format, (int)i, list[i]);
			line[length++] = ' ';
			line[length] = 0;

			if ((sizeof(line) - length) < 32)
			{
				traceMessage(line);
				length = 0;
			}
		}

		length += sprintf_s(line + length, sizeof(line) - length, "\r\n");
		traceMessage(line);
	}

	void PrintKeyValueArray(ID *keys, ID *values, size_t size, TextStream traceMessage, char *format)
	{
		if (traceMessage == nullptr)
			return;

		if (format == nullptr)
			format = "%d:%d";

		char line[512 + 32];
		int length = 0;

		for (size_t i = 0; (i < size); i++)
		{
			length += sprintf_s(line + length, sizeof(line) - length, format, keys[i], values[i]);
			line[length++] = ' ';
			line[length] = 0;

			if ((sizeof(line) - length) < 32)
			{
				traceMessage(line);
				length = 0;
			}
		}

		length += sprintf_s(line + length, sizeof(line) - length, "\r\n");
		traceMessage(line);
	}

	void PrintSets(Ext::ArrayOfArray<ID, ID> _sets, TextStream traceMessage)
	{
		if (traceMessage == nullptr)
			return;

		auto *list = _sets.ptrList();
		char line[512 + 16];
		int size = 0;

		for (ID i = 0; i < _sets.setCount(); i++)
		{
			auto clauseSize = _sets.GetSetSize(i);
			auto start = _sets.GetSetStartIndex(i);

			size = 0;
			for (auto end = start + clauseSize; start < end; start++)
			{
				size += sprintf_s(line + size, sizeof(line) - size, "%d ", list[start]);

				if ((sizeof(line) - size) < 10)
				{
					traceMessage(line);
					size = 0;
				}
			}

			size += sprintf_s(line + size, sizeof(line) - size, "\n");
			traceMessage(line);
		}

		traceMessage("\r\n");
	}

	void PrintSets(Ext::ArrayOfArray<ID, ID> _sets, Ext::Array<SAT::FormulaNode> _nodes, TextStream traceMessage)
	{
		if (traceMessage == nullptr)
			return;

		auto *list = _sets.ptrList();
		char line[512 + 50];
		int size = 0;
		ID nodeIdx = 0;

		for (ID i = 0; i < _sets.setCount(); i++)
		{
			auto clauseSize = _sets.GetSetSize(i);
			auto start = _sets.GetSetStartIndex(i);

			size = 0;
			for (auto end = start + clauseSize; start < end; start++, nodeIdx++)
			{
				size += sprintf_s(line + size, sizeof(line) - size, "%d[%d:%d] ", list[start], _nodes[nodeIdx].Literal, _nodes[nodeIdx].Clause);

				if ((sizeof(line) - size) < 50)
				{
					traceMessage(line);
					size = 0;
				}
			}

			size += sprintf_s(line + size, sizeof(line) - size, "\n");
			traceMessage(line);
		}

		traceMessage("\r\n");
	}

	void PrintGraph(Ext::Array<Vertex>& _graph, TextStream traceMessage)
	{
		if (traceMessage == nullptr)
			return;

		char line[512 + 32];
		int length = 0;

		for (ID i = 0; (i < _graph.size()); i++)
		{
			length += sprintf_s(line + length, sizeof(line) - length, "%d:%d", _graph[i].Id, _graph[i].Count);
			line[length++] = ' ';
			line[length] = 0;

			if ((sizeof(line) - length) < 32)
			{
				traceMessage(line);
				length = 0;
			}
		}

		length += sprintf_s(line + length, sizeof(line) - length, "\r\n");
		traceMessage(line);
	}

	void PrintGraphMatrix(Ext::Array<Vertex>& _graph, TextStream textStream)
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
}
