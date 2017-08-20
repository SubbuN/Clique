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

#include <stdexcept>

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


#define GetQWordAlignedSize(x) ((((x) + 7) >> 3) << 3)
#define GetQWordAlignedSizeForBits(x) ((((x) + 63) >> 6) << 3)
#define GetQWordSizeForBits(x) (((x) + 63) >> 6)
#define GetSizeForBits(x) (((x) + 7) >> 3)

void inline ZeroMemoryPack8(void* _ptr, size_t count)
{
	for (UInt64 *ptr = (UInt64 *)_ptr, *pEnd = ptr + (count >> 3); (ptr < pEnd); ptr++)
		*ptr = 0;
};

void inline CopyMemoryPack8(void* _dest, void* _src, size_t count)
{
	for (UInt64 *dest = (UInt64 *)_dest, *src = (UInt64 *)_src, *pEnd = dest + (count >> 3); (dest < pEnd); dest++, src++)
		*dest = *src;
};

#define SkipArgumentCheck

namespace Ext
{
	enum struct BooleanError : byte
	{
		False = 0,
		True = 1,
		Error = 0xFF,
	};

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

		T& operator[](size_type idx) { return Ptr[idx]; };
		const T& operator[](size_type idx) const { return Ptr[idx]; };

		T* ptr() { return Ptr; }
		size_type size() const { return Size; }
		size_type capacity() const { return Capacity; }

		void size(size_type size) { Size = size; }

		void append(T& value)
		{
#if !defined(SkipArgumentCheck)
			if (Size == Capacity)
				throw std::overflow_error(__FUNCDNAME__);
#endif

			Ptr[Size++] = value;
		}


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

		void ctor(T1 _first, T2 _second)
		{
			first = _first;
			second = _second;
		};


		pair(const pair&) = default;
		pair& operator=(const pair& that) = default;

		T1 first;
		T2 second;
	};

	template < class T, class size_type = size_t >
	class ArrayOfArray
	{
	private:
		size_type	*pStartIndex;
		T				*pList;
		size_type	SetCount;
		size_type	ElementsCapacity;
	public:
		typedef size_type size_type;

	public:
		static inline size_t GetAllocationSize(size_type elementsCapacity)
		{
			// pStartIndex[elements] : stores offset of non-existant set
			return GetQWordAlignedSize((elementsCapacity + 1) * sizeof(size_type)) + GetQWordAlignedSize(sizeof(T) * elementsCapacity);
		}

		static inline size_t GetAllocationSize(size_type setCount, size_type elementsCapacity)
		{
			// pStartIndex[elements] : stores offset of non-existant set
			return GetQWordAlignedSize((setCount + 1) * sizeof(size_type)) + GetQWordAlignedSize(sizeof(T) * elementsCapacity);
		}

		ArrayOfArray(void* ptr, size_type elements)
			: SetCount(0),
			pStartIndex((size_type*)ptr),
			ElementsCapacity((ptr == nullptr) ? 0 : elements),
			pList((ptr == nullptr) ? nullptr : (T*)(((byte*)ptr) + GetQWordAlignedSize((elements + 1) * sizeof(size_type))))
		{
			if (ptr != nullptr)
				ZeroMemoryPack8(pStartIndex, GetAllocationSize(ElementsCapacity));

			if (pStartIndex != nullptr)
				pStartIndex[0] = 0;
		}

		ArrayOfArray(void* ptr, size_type setCount, size_type elements)
			: SetCount(0),
			  pStartIndex((size_type*)ptr),
			  ElementsCapacity((ptr == nullptr) ? 0 : elements),
			  pList((ptr == nullptr) ? nullptr : (T*)(((byte*)ptr) + GetQWordAlignedSize((setCount + 1) * sizeof(size_type))))
		{
			if (ptr != nullptr)
				ZeroMemoryPack8(pStartIndex, GetAllocationSize(setCount, ElementsCapacity));

			if (pStartIndex != nullptr)
				pStartIndex[0] = 0;
		}

		ArrayOfArray(const ArrayOfArray& that)
			: SetCount(that.SetCount),
			pStartIndex(that.pStartIndex),
			ElementsCapacity(that.ElementsCapacity),
			pList(that.pList)
		{
		};

		ArrayOfArray& ctor(void* ptr, size_type elements)
		{
			SetCount = 0;
			pStartIndex = (size_type*)ptr;
			ElementsCapacity = (ptr == nullptr) ? 0 : elements;
			pList = (ptr == nullptr) ? nullptr : (T*)(((byte*)ptr) + GetQWordAlignedSize((elements + 1) * sizeof(size_type)));

			if (ptr != nullptr)
				ZeroMemoryPack8(pStartIndex, GetAllocationSize(setCount, ElementsCapacity));

			if (pStartIndex != nullptr)
				pStartIndex[0] = 0;

			return *this;
		}

		ArrayOfArray& ctor(void* ptr, size_type setCount, size_type elements)
		{
			SetCount = 0;
			pStartIndex = (size_type*)ptr;
			ElementsCapacity = (ptr == nullptr) ? 0 : elements;
			pList = (ptr == nullptr) ? nullptr : (T*)(((byte*)ptr) + GetQWordAlignedSize((setCount + 1) * sizeof(size_type)));

			if (ptr != nullptr)
				ZeroMemoryPack8(pStartIndex, GetAllocationSize(setCount, ElementsCapacity));

			if (pStartIndex != nullptr)
				pStartIndex[0] = 0;

			return *this;
		}

		ArrayOfArray& ctor(const ArrayOfArray& that)
		{
			SetCount = that.SetCount,
			pStartIndex = that.pStartIndex,
			ElementsCapacity = that.ElementsCapacity,
			pList = that.pList

			return *this;
		};


		void* ptr()
		{
			return (void*)pStartIndex;
		}

		T* ptrList()
		{
			return (T*)pList;
		}

		size_type setCount()
		{
			return SetCount;
		}

		size_type elementsCapacity()
		{
			return ElementsCapacity;
		}

		size_type elementsCount()
		{
			return pStartIndex[SetCount];
		}


		void Clear()
		{
			SetCount = 0;
		}

		void LimitToSet(size_type setCount)
		{
			if (setCount < SetCount)
				SetCount = setCount;
		}

		ArrayOfArray Clone(void* ptr, size_type elementsCapacity)
		{
			if (elementsCapacity < elementsCount())
				throw std::invalid_argument(__FUNCDNAME__);

			ArrayOfArray that(ptr, SetCount, elementsCapacity);
			that.SetCount = SetCount;

			if (ptr != nullptr)
			{
				memcpy(that.pStartIndex, pStartIndex, GetQWordAlignedSize((SetCount + 1) * sizeof(size_type)));
				memcpy(that.pList, pList, GetQWordAlignedSize(sizeof(T) * elementsCapacity));
			}

			return that;
		}


		void InitSet(size_type set, size_type size)
		{
#if !defined(SkipArgumentCheck)
			if ((set != SetCount) || (size == 0) || ((ElementsCapacity - pStartIndex[set]) < size))
				throw std::invalid_argument(__FUNCDNAME__);
#endif

			pStartIndex[set + 1] = pStartIndex[set] + size;
			SetCount++;
		};

		size_type GetSetSize(size_type set)
		{
#if !defined(SkipArgumentCheck)
			if (set >= SetCount)
				throw std::out_of_range(__FUNCDNAME__);
#endif

			return (pStartIndex[set + 1] - pStartIndex[set]);
		};

		size_type GetSetStartIndex(size_type set)
		{
#if !defined(SkipArgumentCheck)
			if (set >= SetCount)
				throw std::out_of_range(__FUNCDNAME__);
#endif

			return pStartIndex[set];
		};

		T* GetSet(size_type set)
		{
#if !defined(SkipArgumentCheck)
			if (set >= SetCount)
				throw std::out_of_range(__FUNCDNAME__);
#endif

			return &pList[pStartIndex[set]];
		};

		T& Get(size_type set, size_type idx)
		{
#if !defined(SkipArgumentCheck)
			if ((set >= SetCount) || ((pStartIndex[set + 1] - pStartIndex[set]) <= idx))
				throw std::out_of_range(__FUNCDNAME__);
#endif

			return pList[pStartIndex[set] + idx];
		};


		bool IsCorrupt()
		{
			for (size_type i = 0; i < SetCount; i++)
			{
				if ((pStartIndex[i] > ElementsCapacity) || (pStartIndex[i] >= pStartIndex[i + 1]))
					return true;
			}

			return false;
		}
	};

	namespace Unsafe
	{
		template <class T>
		class DeleteObject
		{
		public:
			DeleteObject(T* ptr) : Ptr(ptr)
			{
			};

			~DeleteObject()
			{
				delete Ptr;
				Ptr = nullptr;
			};

			T*	ptr() { return Ptr; };

		private:
			T*	Ptr;
		};

		template <class T>
		class DeleteObjects
		{
		public:
			DeleteObjects(T* ptr) : Ptr(ptr)
			{
			};

			~DeleteObjects()
			{
				delete[] Ptr;
				Ptr = nullptr;
			};

			T*	ptr() { return Ptr; };

		private:
			T*	Ptr;
		};

		template < class T, class size_type = size_t >
		class CircularQueue
		{
		public:
			CircularQueue(T* ptr, size_type capacity)
				: Ptr(ptr), Capacity(capacity), First(0), Last(0)
			{
			};

			size_type size() const { return Last - First; }
			bool isEmpty() { return (Last == First); }
			void push(T value) { Ptr[Last++ % Capacity] = value; };
			T pop() { return Ptr[First++ % Capacity]; };

		private:
			T* Ptr;
			size_type First, Last, Capacity;
		};

		template < class T, class size_type = size_t >
		struct ArrayOfSet
		{
			size_type	*pStartIndex;
			T				*pList;
			size_type	ElementsCapacity;

			ArrayOfSet()
				: pStartIndex(nullptr), pList(nullptr), ElementsCapacity(0)
			{
			}

			ArrayOfSet(void* ptr, size_type elementsCapacity)
				: pStartIndex(nullptr), pList(nullptr), ElementsCapacity(elementsCapacity)
			{
				ctor(ptr, elementsCapacity);
			}

			ArrayOfSet(ArrayOfSet& that)
				: pStartIndex(that.pStartIndex), pList(that.pList), ElementsCapacity(that.ElementsCapacity)
			{
			}

			void ZeroMemory()
			{
				ZeroMemoryPack8(pStartIndex, GetAllocationSize(ElementsCapacity));
			}

			inline void InitSet(size_type set)
			{
				pStartIndex[set + 1] = pStartIndex[set]; // Set count to 0
			};

			inline size_type GetSetSize(size_type set)
			{
				return (pStartIndex[set + 1] - pStartIndex[set]);
			};

			inline void SetSetSize(size_type set, size_type size)
			{
				pStartIndex[set + 1] = pStartIndex[set] + size;
			};

			inline T& GetValue(size_type set, size_type idx)
			{
				return pList[pStartIndex[set] + idx];
			};

			inline T& CreateTrivialSet(size_type set)
			{
				pStartIndex[set + 1] = pStartIndex[set] + 1;
				return pList[pStartIndex[set]];
			}

			static inline size_t GetAllocationSize(size_t elementsCapacity)
			{
				return GetQWordAlignedSize(sizeof(size_type) * (elementsCapacity + 1)) + GetQWordAlignedSize(sizeof(T) * elementsCapacity);
			}

			inline void ctor(void* ptr, size_type elementsCapacity)
			{
				ElementsCapacity = elementsCapacity;
				pStartIndex = (size_type*)ptr;
				pList = (ptr == nullptr) ? nullptr : (T*)(((byte*)pStartIndex) + GetQWordAlignedSize(sizeof(size_type) * (elementsCapacity + 1)));
				if (ptr != nullptr)
					pStartIndex[0] = 0;
			}
		};
	}
}

#endif
