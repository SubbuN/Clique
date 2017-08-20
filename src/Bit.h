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

#if (!BIT_H)
#define BIT_H

#include <intrin.h>
#include "Types.h"

#define	BitSetMaxValue	0xFF

class Bit
{
public:
	static const byte MaxValue = 0xFF;

	static const byte	FirstLSB[256];
	static const byte	FirstMSB[256];
	static const byte	BitCount[256];

	static const byte	BIT[8];
	static const byte	BitMask[8];
	static const byte	LsbBitMask[8];

public:
};

//int inline PopCount(int bits)
//{
//	return Bit::BitCount[bits];
//}

// #define PopCount(x) Bit::BitCount[(x)]
#define PopCount8(x) Bit::BitCount[(x)]
#define PopCount32(x) __popcnt((x))
#define PopCount64(x) __popcnt64((x))

#endif		//	BIT_H
