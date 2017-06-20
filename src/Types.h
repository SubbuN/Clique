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

#if (!Types_H)
#define Types_H

typedef unsigned char byte;
typedef __int32 Int32;
typedef __int64 Int64;
typedef unsigned __int32 UInt32;
typedef unsigned __int64 UInt64;

#if (defined(_M_AMD64) || defined(_M_X64))
typedef __int64 PtrInt;
#else
typedef long PtrInt;
#endif


namespace Ext
{
	template < class T, class size_type = size_t >
	class Array
	{
	public:
		Array(T* ptr = nullptr, size_type size = 0)
			: Ptr(ptr), Size(size)
		{
		};

		Array(const Array& that)
			: Ptr(that.Ptr), Size(that.Size)
		{
		};

		Array& operator=(const Array& that)
		{
			Ptr = that.Ptr;
			Size = that.Size;

			return *this;
		};

		T& operator[](size_t idx) { return Ptr[idx]; };
		const T& operator[](size_t idx) const { return Ptr[idx]; };

		T* ptr() { return Ptr; }
		size_type size() const { return Size; }

	private:
		T* Ptr;
		size_type Size;
	};



	template < class size_type = size_t >
	class VList
	{
	public:
		VList(void* ptr = nullptr, size_type capacity = 0, size_type size = 0)
			: Ptr(ptr), Capacity(capacity), Size(size)
		{
		};

		VList(const VList& that)
			: Ptr(that.Ptr), Capacity(that.Capacity), Size(that.Size)
		{
		};

		VList& operator=(const VList& that)
		{
			Ptr = that.Ptr;
			Capacity = that.Capacity;
			Size = that.Size;

			return *this;
		};

		template<class T>
		T* ptr() { return (T*)Ptr; }

		void* ptr() { return Ptr; }
		size_type size() const { return Size; }
		size_type capacity() const { return Capacity; }

		void size(size_type size) { Size = size; }

	private:
		void* Ptr;
		size_type Size, Capacity;
	};

	template < class T, class size_type = size_t >
	class List
	{
	public:
		List(T* ptr = nullptr, size_type capacity = 0, size_type size = 0)
			: Ptr(ptr), Capacity(capacity), Size(size)
		{
		};

		List(const List& that)
			: Ptr(that.Ptr), Capacity(that.Capacity), Size(that.Size)
		{
		};

		List(const VList<size_type>& that)
			: Ptr((T*)that.Ptr), Capacity(that.Capacity), Size(that.Size)
		{
		};

		List& operator=(const List& that)
		{
			Ptr = that.Ptr;
			Capacity = that.Capacity;
			Size = that.Size;

			return *this;
		};

		T& operator[](size_t idx) { return Ptr[idx]; };
		const T& operator[](size_t idx) const { return Ptr[idx]; };

		T* ptr() { return Ptr; }
		size_type size() const { return Size; }
		size_type capacity() const { return Capacity; }

		void size(size_t size) { Size = size; }

		VList<size_type> toVList() { return VList<size_type>(Ptr, Capacity, Size); }

	private:
		T* Ptr;
		size_type Size, Capacity;
	};


	template <class T1, class T2>
	struct pair
	{
		pair(T1 _first, T2 _second)
			: first(_first), second(_second)
		{
		};

		pair(const pair&) = default;
		pair& operator=(const pair& that) = default;

		T1 first;
		T2 second;
	};
}

#endif
