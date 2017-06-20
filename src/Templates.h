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

template <class T1, class T2, class SizeT>
void Sort(T1 *_array, T2 *_items, SizeT _beginIndex, SizeT _count, bool _ascOrder, SizeT *_stack)
{
	if ((_count <= 1) || (_array == nullptr))
		return;

	T1	mid;
	SizeT	low, high;
	SizeT	lowMarker, highMarker;
	int	stkPtr;
	T1	tmp;
	T2	iTmp;

	stkPtr = 0;

	low = _beginIndex;
	high = _beginIndex + _count - 1;


recurse:

	lowMarker = low;
	highMarker = high;

	mid = _array[(low + high) / 2];

	while (lowMarker <= highMarker)
	{
		if (_ascOrder)
		{
			while (_array[lowMarker] < mid)
				lowMarker++;

			while (_array[highMarker] > mid)
				highMarker--;
		}
		else
		{
			while (_array[lowMarker] > mid)
				lowMarker++;

			while (_array[highMarker] < mid)
				highMarker--;
		}

		if (highMarker < lowMarker)
			break;

		// swap.  Use std::swap()
		tmp = _array[lowMarker];
		_array[lowMarker] = _array[highMarker];
		_array[highMarker] = tmp;

		if (_items != nullptr)
		{
			iTmp = _items[lowMarker];
			_items[lowMarker] = _items[highMarker];
			_items[highMarker] = iTmp;
		}

		lowMarker++;
		highMarker--;
	}

	if ((highMarker - low) >= (high - lowMarker))
	{
		if (low < highMarker)
		{
			_stack[stkPtr++] = low;
			_stack[stkPtr++] = highMarker;
		}

		if (lowMarker < high)
		{
			low = lowMarker;
			goto recurse;
		}
	}
	else
	{
		if (lowMarker < high)
		{
			_stack[stkPtr++] = lowMarker;
			_stack[stkPtr++] = high;
		}

		if (low < highMarker)
		{
			high = highMarker;
			goto recurse;
		}
	}

	stkPtr--;
	if (stkPtr > 0)
	{
		high = _stack[stkPtr--];
		low = _stack[stkPtr];
		goto recurse;
	}
}
