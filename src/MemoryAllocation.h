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

inline void* AllocMemory(size_t size)
{
	// This function needs to allocate memory using OS Memory allocation API
	// Make sure that the memory is pinned in RAM and no swapping.
	return new byte[size];
}

inline void FreeMemory(void* ptr)
{
	if (ptr != nullptr)
		delete[](byte*)ptr;
}

