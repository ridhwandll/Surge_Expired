// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Shader/ReflectionData.hpp"

namespace Surge::ShaderBinary
{
    // Engine shaders, compiled by the Editor's ShaderCompiler: SPIR-V of every stage + its reflection, loaded as is on every platform
    // Layout: Magic, Version, SourceHash, Stages, include list, stages (type + SPIR-V words), ShaderReflectionData (Rapt0R)
    constexpr Uint MAGIC = 0x52444853; // SHDR
    constexpr Uint VERSION = 2;        // 2: Rapt0R format

    struct CookedShader
    {
        uint64_t SourceHash = 0;
        ShaderType Stages = ShaderType::NONE;
        Vector<String> Includes;  // As written in #include, relative to the shader's directory
        Vector<SPIRVHandle> SPIRVs;
        ShaderReflectionData Reflection;
    };

    // "Engine/Assets/Shaders/Renderer3D.glsl" -> "Engine/Assets/Shaders/Cooked/Renderer3D.rSHADER"
    String GetCookedPath(const String& sourcePath);

    // Main file + every include (line endings ignored), 0 if one of them can't be read. Needs the sources, so never on Android
    uint64_t HashSource(const String& sourcePath, const Vector<String>& includes);

    bool Write(const String& path, const CookedShader& shader);
    bool Read(const String& path, CookedShader& outShader);

    bool ReadSourceInfo(const String& path, uint64_t& outHash, Vector<String>& outIncludes);
}
