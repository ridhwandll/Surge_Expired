// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include <format>

namespace Surge
{
    class Path
    {
    public:
        Path() = default;
        Path(const char* cstr)
            : mPathStr(cstr) {}
        Path(const String& str)
            : mPathStr(str) {}

        void operator=(const Path& other)
        {
            mPathStr = other.mPathStr;
        }

        Path ParentPath()
        {
            String path = mPathStr.substr(0, mPathStr.find_last_of("/\\"));
            return path;
        }

        Path FileName()
        {
            String name;
            auto lastSlash = mPathStr.find_last_of("/\\");
            lastSlash = lastSlash == String::npos ? 0 : lastSlash + 1;
            auto lastDot = mPathStr.rfind('.');
            auto count = lastDot == String::npos ? mPathStr.size() - lastSlash : lastDot - lastSlash;
            name = mPathStr.substr(lastSlash, count);
            return name;
        }

        size_t Size() const { return mPathStr.size(); }

        const String& Str() const { return mPathStr; }
        std::wstring WStr() const { return std::wstring(mPathStr.begin(), mPathStr.end()); }

        operator String() { return mPathStr; }
        operator const char*() const { return mPathStr.c_str(); }
        operator bool() const { return !mPathStr.empty(); }

        [[nodiscard]] friend bool operator==(const Path& left, const String& right) { return left.Str() == right; }
        [[nodiscard]] friend Path operator/(const Path& left, const Path& right) { return std::format("{0}/{1}", left.Str(), right.Str()); }
        [[nodiscard]] friend Path operator/(const Path& left, const String& right) { return std::format("{0}/{1}", left.Str(), right); }
        [[nodiscard]] friend Path operator/(const Path& left, const char* right) { return std::format("{0}/{1}", left.Str(), right); }

    private:
        String mPathStr;
    };

} // namespace Surge