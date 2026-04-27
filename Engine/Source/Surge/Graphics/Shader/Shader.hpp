// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Hash.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/UUID.hpp"

namespace Surge
{
    enum class ShaderType
    {
        None = 0,
        Vertex = BIT(0),
        Pixel = BIT(1),
        Compute = BIT(2)
    };
    MAKE_BIT_ENUM(ShaderType, uint16_t)

    inline String ShaderTypeToString(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return "Vertex";
            case ShaderType::Pixel: return "Pixel";
            case ShaderType::Compute: return "Compute";
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }
        SG_ASSERT_INTERNAL("Unknown ShaderType!");
        return "None";
    }

    enum class SURGE_API ShaderDataType
    {
        None,
        Int,
        UInt,
        Float,
        Float2,
        Float3,
        Float4,
        Mat2,
        Mat4,
        Mat3,
        Bool,
        Struct
    };

    inline String ShaderDataTypeToString(const ShaderDataType& type)
    {
        switch (type)
        {
            case ShaderDataType::Int: return "Int";
            case ShaderDataType::UInt: return "UInt";
            case ShaderDataType::Float: return "Float";
            case ShaderDataType::Float2: return "Float2";
            case ShaderDataType::Float3: return "Float3";
            case ShaderDataType::Float4: return "Float4";
            case ShaderDataType::Mat2: return "Mat2";
            case ShaderDataType::Mat4: return "Mat4";
            case ShaderDataType::Mat3: return "Mat3";
            case ShaderDataType::Bool: return "Bool";
            case ShaderDataType::None: SG_ASSERT_INTERNAL("ShaderDataType::None is invalid in this case!");
            default:
                SG_ASSERT_INTERNAL("Tf?");
        }
        SG_ASSERT_INTERNAL("Unknown ShaderDataType!");
        return "None";
    }

    inline Uint ShaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float: return 4;
            case ShaderDataType::Float2: return 4 * 2;
            case ShaderDataType::Float3: return 4 * 3;
            case ShaderDataType::Float4: return 4 * 4;
            case ShaderDataType::Mat3: return 4 * 3 * 3;
            case ShaderDataType::Mat4: return 4 * 4 * 4;
            case ShaderDataType::Int: return 4;
            case ShaderDataType::UInt: return 4;
            case ShaderDataType::Bool: return 4;
            case ShaderDataType::Struct: return -1;
            default: SG_ASSERT_INTERNAL("Invalid case!");
        }

        SG_ASSERT_INTERNAL("Unknown ShaderDataType!");
        return 0;
    }

    struct SPIRVHandle
    {
        Vector<Uint> SPIRV;
        ShaderType Type;
    };

    class SURGE_API ShaderReflectionData;
    class SURGE_API Shader : public RefCounted
    {
    public:
        Shader() = default;
        virtual ~Shader() = default;

        virtual void Load(const HashMap<ShaderType, bool>& compileStages = {}) = 0;
        virtual void Reload() = 0;
        NODISCARD virtual UUID AddReloadCallback(const std::function<void()> callback) = 0;
        virtual void RemoveReloadCallback(const UUID& id) = 0;
        NODISCARD virtual const ShaderReflectionData& GetReflectionData() const = 0;
        NODISCARD virtual const Vector<SPIRVHandle>& GetSPIRVs() const = 0;
        NODISCARD virtual const Path& GetPath() const = 0;
        NODISCARD virtual const HashMap<ShaderType, String>& GetSources() const = 0;
        NODISCARD virtual const HashCode& GetHash(const ShaderType& type) const = 0;
        NODISCARD virtual const HashMap<ShaderType, HashCode>& GetHashCodes() const = 0;
        NODISCARD virtual const ShaderType& GetTypes() const = 0;

        static Ref<Shader> Create(const Path& path);
    };
} // namespace Surge