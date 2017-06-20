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

#if (!Utility_H)
#define Utility_H

#include <string>
#include <locale>
#include <codecvt>



//////////////////////////////////////////////////////////////////
// String conversions

inline std::wstring ToWString(const char* start)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(start);
};

inline std::wstring ToWString(const char* start, const char* end)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(start, end);
};

inline std::wstring ToWString(const std::string& str)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
};

inline std::string ToString(const wchar_t* start)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(start);
};

inline std::string ToString(const wchar_t* start, const wchar_t* end)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(start, end);
};

inline std::string ToString(const std::wstring& str)
{
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(str);
};

#endif
