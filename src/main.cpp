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
#include "Utility.h"
#include "Bit.h"

#include <conio.h>

#ifdef _WIN32
#pragma comment(lib, "Kernel32.lib")
	extern "C"
	{
		extern void __stdcall OutputDebugStringA(const char* lpOutputString);
	}

	void LogMessage(const char* message)
	{
		OutputDebugStringA(message);
	}
#else
	void LogMessage(const char* message)
	{
		OutputDebugStringA(message);
	}
#endif


int main(int argc, char* argv[])
{
	std::string path;
	Ext::Array<Graph::Vertex> graph;

	Graph::SetTraceMessageHandler(LogMessage);

	if (argc == 2)
	{
		path = std::string(argv[1]);

		graph = Graph::ReadDIMACSGraph(path.c_str());
		if (graph.size() > 0)
		{
			auto cliqueSize = Graph::FindClique(graph);
			Graph::FreeGraph(graph);

			printf("\r\n%d\r\n", cliqueSize);
		}
		else
		{
			printf("Please specify file containing DIMACS binary graph");
		}
	}
	else
	{
		printf("Invalid argument(s). Please specify file containing DIMACS binary graph");
	}

	return 0;
}
