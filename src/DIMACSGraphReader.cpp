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
#include <stdio.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace Graph
{
	const byte DIMACSBitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	Ext::Array<Vertex> ReadDIMACSGraph(const char *_binGraphFile)
	{
		char	byteChar;
		size_t	i, j;
		size_t	preambleSize = 0, vertexCount = 0, edgeCount = 0;
		FILE	*fsIn = nullptr;
		byte	*buffer = nullptr;
		Ext::Array<Graph::Vertex> graph;

		size_t count = 0;

		try
		{
			if (fopen_s(&fsIn, _binGraphFile, "rb") != 0)
				return graph;

			// 1. Read preamble size.
			while ((fread(&byteChar, 1, 1, fsIn) != -1) && ('0' <= byteChar) && (byteChar <= '9'))
				preambleSize = preambleSize * 10 + byteChar - '0';

			buffer = new byte[preambleSize];

			//	2. Read preamble
			fread(buffer, sizeof(byte), preambleSize, fsIn);

			//	3. Parse preamble
			i = 0;
			while (i < preambleSize)
			{
				if (buffer[i] == 'p')
				{
					// skip white characters
					for (i++; (i < preambleSize) && ((buffer[i] == ' ') || (buffer[i] == '\t')); i++);

					// skip non-white text
					for (; (i < preambleSize) && (buffer[i] != ' ') && (buffer[i] != '\t'); i++);

					// skip white characters
					for (; (i < preambleSize) && ((buffer[i] == ' ') || (buffer[i] == '\t')); i++);

					//	Read vertexCount
					for (; (i < preambleSize) && ('0' <= buffer[i]) && (buffer[i] <= '9'); i++)
						vertexCount = vertexCount * 10 + buffer[i] - '0';

					// skip white characters
					for (; (i < preambleSize) && ((buffer[i] == ' ') || (buffer[i] == '\t')); i++);

					//	Read vertexCount
					for (; (i < preambleSize) && ('0' <= buffer[i]) && (buffer[i] <= '9'); i++)
						edgeCount = edgeCount * 10 + buffer[i] - '0';
				}

				// skip the entire line
				for (; (i < preambleSize) && (buffer[i] != '\r') && (buffer[i] != '\n'); i++);

				// skip new-lines
				for (; (i < preambleSize) && ((buffer[i] == '\r') || (buffer[i] == '\n')); i++);
			}

			//	4. Create vertices
			count = vertexCount;
			graph = CreateGraph((size_t)vertexCount);

			if (preambleSize < GetQWordAlignedSizeForBits(vertexCount))
			{
				preambleSize = GetQWordAlignedSizeForBits(vertexCount);
				delete[]buffer;
				buffer = nullptr; // new may throw exception based on new() behavior settings

				buffer = new byte[preambleSize];
				if (buffer == nullptr)
					throw std::bad_alloc();
			}

			// 5. Read and Create edges
			for (i = 0; (i < vertexCount); i++)
			{
				if ((j = (int)fread(buffer, sizeof(byte), (i + 8) / 8, fsIn)) != ((i + 8) / 8))
					throw "Invalid graph file.";

				for (j = 0; j < i; j++)
				{
					if ((buffer[j >> 3] & DIMACSBitMask[j & 0x07]) == DIMACSBitMask[j & 0x07])
					{
						BitSet(graph[i].Neighbours, j);
						BitSet(graph[j].Neighbours, i);
					}
				}

				BitSet(graph[i].Neighbours, i);
			}

			for (i = 0; (i < vertexCount); i++)
			{
				graph[i].Count = 0;

				size_t length;
				for (j = 0, length = GetQWordAlignedSizeForBits(vertexCount); j < length; j++)
					graph[i].Count += PopCount32(graph[i].Neighbours[j]);
			}

			fclose(fsIn);
		}
		catch (...)
		{
			if (buffer != nullptr)
				delete[]buffer;

			if (fsIn != nullptr)
				fclose(fsIn);

			FreeGraph(graph);
		}

		return graph;
	}

	SATFormula	ReadDIMACSSATFormula(const char * _satFormula)
	{
		SATFormula formula;

		try
		{
			std::string line;
			std::ifstream file(_satFormula);
			if (file.bad())
				return formula;

			do
			{
				if (!std::getline(file, line) || (line.size() <= 1) || (line[1] != ' ') || ((line[0] != 'c') && (line[0] != 'p')))
					return formula;
			} while (line[0] == 'c');

			std::string problem;
			int varialbes, clauses, literal;

			{
				std::stringstream tr(line.c_str() + 2);
				tr >> problem;
				if (problem.compare("cnf") != 0)
					return formula;

				tr >> varialbes >> clauses;
			}

			if ((varialbes <= 0) || (clauses <= 0))
				return formula;

			std::vector<int> offset;
			std::vector<int> literals;

			offset.reserve(clauses);
			literals.reserve(clauses * 3);

			for (int i = 0; i < clauses; i++)
			{
				if (!std::getline(file, line))
					return formula;

				offset.push_back((int)literals.size());

				std::stringstream tr(line.c_str());

				while (true)
				{
					tr >> literal;
					if (literal == 0)
						break;

					literals.push_back(literal);
				}
			}

			offset.push_back((int)literals.size());

			int *pData = new int[offset.size() + literals.size()];

			for (int i = 0; i < clauses; i++)
				pData[i] = offset[i];

			pData[clauses] = offset[clauses];

			for (int i = 0, j = clauses + 1; i < literals.size(); i++, j++)
				pData[j] = literals[i];


			formula.Variables = varialbes;
			formula.Clauses = clauses;
			formula.Nodes = (int)literals.size();
			formula.pData = pData;

			return formula;
		}
		catch (...)
		{
		}

		return formula;
	}
}

