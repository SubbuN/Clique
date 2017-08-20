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

					//	Read edgeCount
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

	void SaveDIMACSGraph(const char * _binGraphFile, Ext::Array<Vertex> _graph, const char* _name, byte *_buffer)
	{
		if ((_binGraphFile == nullptr) || (*_binGraphFile == 0))
			return;

		if (_name == nullptr)
		{
			// Extract file name
			const char *name = _binGraphFile;
			for (_name = name = _binGraphFile; *name; name++)
			{
				if ((*name == '/') || (*name == '\\'))
					_name = name + 1;
			}
		}

		FILE	*fsOut = nullptr;
		byte	*buffer = nullptr;

		try
		{
			if (fopen_s(&fsOut, _binGraphFile, "wb") != 0)
				return ;

			char line[512], szCount[16];
			size_t edgeCount = 0;
			for (size_t i = 0; i < _graph.size(); i++)
			{
				edgeCount += _graph[i].Count;
				if (BitTest(_graph[i].Neighbours, i))
					edgeCount--;
			}

			int size = sprintf_s(line, sizeof(line), "c FILE: %s\nc%-64s\nc Graph Size:%I64d\nc\np edge %I64d %I64d\n", _name, "", _graph.size(), _graph.size(), edgeCount / 2);
			int size2 = sprintf_s(szCount, sizeof(szCount), "%d\n", size);

			fwrite(szCount, 1, size2, fsOut);
			fwrite(line, 1, size, fsOut);

			size_t bufferSize = GetQWordAlignedSizeForBits(_graph.size());
			buffer = (_buffer != nullptr) ? _buffer : new byte[bufferSize];
			for (size_t i = 0; i < _graph.size(); i++)
			{
				bufferSize = (i + 8) / 8;
				memset(buffer, 0, bufferSize);

				for (size_t j = 0; j < i; j++)
				{
					if (BitTest(_graph[i].Neighbours, j))
						buffer[j >> 3] |= DIMACSBitMask[j & 0x07];
				}

				fwrite(buffer, 1, bufferSize, fsOut);
			}

			fclose(fsOut);
		}
		catch (...)
		{
			if ((_buffer == nullptr) && (buffer != nullptr))
				delete[]buffer;

			if (fsOut != nullptr)
				fclose(fsOut);
		}
	}

	SAT::Formula ReadDIMACSSATFormula(const char * _satFormula)
	{
		SAT::Formula formula;

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

			formula.Variables = varialbes;
			formula.Clauses.ctor(new byte[Ext::ArrayOfArray<int, ID>::GetAllocationSize((ID)clauses, (ID)literals.size())], (ID)clauses, (ID)literals.size());

			memcpy(formula.Clauses.ptrList(), literals._Myfirst(), sizeof(int) * literals.size());
			for (int i = 0; i < clauses; i++)
				formula.Clauses.InitSet(i, ID(offset[i + 1] - offset[i]));

			return formula;
		}
		catch (...)
		{
		}

		return formula;
	}

	void PrintSATClause(int* _ptrClause, ID _size, TextStream _textStream, char* _buffer, size_t _bufferSize)
	{
		if (_textStream == nullptr)
			return;

		if (_buffer == nullptr)
			throw std::invalid_argument(__FUNCDNAME__);

		size_t size = 0;
		for (size_t i = 0; i < _size; i++)
		{
			size += sprintf_s(_buffer + size, _bufferSize - size, "%d ", _ptrClause[i]);

			if ((_bufferSize - size) < 10)
			{
				_textStream(_buffer);
				size = 0;
			}
		}

		size += sprintf_s(_buffer + size, _bufferSize - size, "0\n");
		_textStream(_buffer);
	}


	void SaveDIMACSSATFormula(const char * _binGraphFile, SAT::Formula _formula, const char* _name)
	{
		if ((_binGraphFile == nullptr) || (*_binGraphFile == 0))
			return;

		if (_name == nullptr)
		{
			// Extract file name
			const char *name = _binGraphFile;
			for (_name = name = _binGraphFile; *name; name++)
			{
				if ((*name == '/') || (*name == '\\'))
					_name = name + 1;
			}
		}

		FILE	*fsOut = nullptr;

		try
		{
			if (fopen_s(&fsOut, _binGraphFile, "wb") != 0)
				return;

			Ext::ArrayOfArray<int, ID>& clauses = _formula.Clauses;
			int *list = clauses.ptrList();
			char line[512];
			int size = sprintf_s(line, sizeof(line), "c FILE: %s\nc%-64s\np cnf %d %d\n", _name, "", _formula.Variables, clauses.setCount());

			fwrite(line, 1, size, fsOut);

			for (ID i = 0; i < clauses.setCount(); i++)
			{
				auto clauseSize = clauses.GetSetSize(i);
				auto start = clauses.GetSetStartIndex(i);

				size = 0;
				for (auto end = start + clauseSize; start < end; start++)
				{
					size += sprintf_s(line + size, sizeof(line) - size, "%d ", list[start]);

					if ((sizeof(line) - size) < 10)
					{
						fwrite(line, 1, size, fsOut);
						size = 0;
					}
				}

				size += sprintf_s(line + size, sizeof(line) - size, "0\n");
				fwrite(line, 1, size, fsOut);
			}

			fclose(fsOut);
		}
		catch (...)
		{
			if (fsOut != nullptr)
				fclose(fsOut);
		}
	}


	void SaveDIMACSSATClauses(const char* _binGraphFile, SAT::Formula _formula, ID _clauseSize)
	{
		if ((_binGraphFile == nullptr) || (*_binGraphFile == 0))
			return;

		FILE	*fsOut = nullptr;

		try
		{
			if (fopen_s(&fsOut, _binGraphFile, "wb") != 0)
				return;

			Ext::ArrayOfArray<int, ID>& clauses = _formula.Clauses;
			int *list = clauses.ptrList();
			char line[512];
			int size = 0;

			for (ID i = 0; i < clauses.setCount(); i++)
			{
				auto clauseSize = clauses.GetSetSize(i);
				auto start = clauses.GetSetStartIndex(i);

				if (clauseSize != _clauseSize)
					continue;

				size = 0;
				for (auto end = start + clauseSize; start < end; start++)
				{
					size += sprintf_s(line + size, sizeof(line) - size, "%d ", list[start]);

					if ((sizeof(line) - size) < 10)
					{
						fwrite(line, 1, size, fsOut);
						size = 0;
					}
				}

				size += sprintf_s(line + size, sizeof(line) - size, "0\n");
				fwrite(line, 1, size, fsOut);
			}

			fclose(fsOut);
		}
		catch (...)
		{
			if (fsOut != nullptr)
				fclose(fsOut);
		}
	}
}

