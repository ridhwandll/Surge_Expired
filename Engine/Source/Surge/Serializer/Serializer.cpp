// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <json/json.hpp>

// https://github.com/nlohmann/json#arbitrary-types-conversions
namespace glm
{
    inline void to_json(nlohmann::json& j, const vec2& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}};
    }

    inline void from_json(const nlohmann::json& j, vec2& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
    }

    inline void to_json(nlohmann::json& j, const vec3& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}, {"Z", p.z}};
    }

    inline void from_json(const nlohmann::json& j, vec3& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
        j.at("Z").get_to(p.z);
    }

    inline void to_json(nlohmann::json& j, const vec4& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}, {"Z", p.z}, {"W", p.w}};
    }

    inline void from_json(const nlohmann::json& j, vec4& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
        j.at("Z").get_to(p.z);
        j.at("W").get_to(p.w);
    }
} // namespace glm

namespace Surge
{
    //----------
    // Serialize
    //----------

    // Enum Mapping(s)
    NLOHMANN_JSON_SERIALIZE_ENUM(RuntimeCamera::ProjectionType, {{RuntimeCamera::ProjectionType::Perspective, "Perspective"}, {RuntimeCamera::ProjectionType::Orthographic, "Orthographic"}})

    template <typename XComponent>
    FORCEINLINE static void SerializeComponent(nlohmann::json& j, Entity& e)
    {
        if (e.HasComponent<XComponent>())
        {
            XComponent& comp = e.GetComponent<XComponent>();
            const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<XComponent>();
            uint64_t offset = 0;

            nlohmann::json& out = j[clazz->GetName()];

            for (const auto& [name, var] : clazz->GetVariables())
            {
                uint64_t size = var.GetSize();
                const Byte* source = reinterpret_cast<const Byte*>(&comp) + offset;

                const SurgeReflect::Type& type = var.GetType();
                if (type.EqualTo<bool>())
                {
                    bool destination;
                    std::memcpy(&destination, reinterpret_cast<const bool*>(source), size);
                    out[name] = destination;
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<float>())
                {
                    float destination;
                    std::memcpy(&destination, reinterpret_cast<const float*>(source), size);
                    out[name] = destination;
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<UUID>())
                {
                    uint64_t destination = *reinterpret_cast<const uint64_t*>(source);
                    out[name] = destination;
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<Vector<UUID>>())
                {
                    Vector<uint64_t> destination;
                    const Vector<UUID>* src = reinterpret_cast<const Vector<UUID>*>(source);
                    destination.resize(src->size());
                    size = src->size() * sizeof(uint64_t);
                    std::memcpy(destination.data(), src->data(), size);
                    out[name] = destination;
                    continue;
                }
                else if (type.EqualTo<String>())
                {
                    // Get the string size; (we cannot use var.GetSize() as that uses sizeof())
                    const String* src = reinterpret_cast<const String*>(source);
                    size = src->size();

                    String destination;
                    destination.resize(size);
                    std::memcpy(reinterpret_cast<void*>(destination.data()), reinterpret_cast<const String*>(source)->c_str(), size);
                    out[name] = destination;
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<Path>())
                {
                    continue;
                }
                else if (type.EqualTo<glm::vec3>())
                {
                    glm::vec3 destination;
                    std::memcpy(glm::value_ptr(destination), glm::value_ptr(*reinterpret_cast<const glm::vec3*>(source)), size);
                    out[name] = destination;
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<RuntimeCamera>())
                {
                    const RuntimeCamera* cam = reinterpret_cast<const RuntimeCamera*>(source);

                    out["Vertical FOV"] = cam->GetPerspectiveVerticalFOV();
                    out["Perspective NearClip"] = cam->GetPerspectiveNearClip();
                    out["Perspective FarClip"] = cam->GetPerspectiveFarClip();
                    out["Orthographic NearClip"] = cam->GetOrthographicNearClip();
                    out["Orthographic FarClip"] = cam->GetOrthographicFarClip();
                    out["Orthographic Size"] = cam->GetOrthographicSize();
                    out["Projection"] = cam->GetProjectionType();
                    offset += size;
                    continue;
                }
                else if (type.EqualTo<Ref<Mesh>>())
                {
                    const Ref<Mesh>* mesh = reinterpret_cast<const Ref<Mesh>*>(source);
                    String path;
                    if (*mesh)
                        path = std::filesystem::relative((*mesh)->GetPath().Str()).string();

                    out[name] = path;
                    offset += size;
                    continue;
                }
                Log<Severity::Warn>("Unhandled Variable of type: '{0}' while serializing!", type.GetFullName());
            }
        }
    }

    template <typename... Components>
    FORCEINLINE void SerializeComponents(nlohmann::json& out, Entity& e)
    {
        (SerializeComponent<Components>(out, e), ...);
    }

    // nlohmann::json& j is in "Scene" scope
    static void SerializeEntity(nlohmann::json& j, Entity& e, uint64_t index)
    {
        SG_ASSERT_NOMSG(e);

        nlohmann::json& out = j[fmt::format("Entity{0}", index)];
        SerializeComponents<ALL_MAJOR_COMPONENTS>(out, e);
    }

    template <>
    void Surge::Serializer::Serialize(const Path& path, Scene* in)
    {
        SG_ASSERT_NOMSG(in);
        SCOPED_TIMER("Serialization");
        nlohmann::json outJson = nlohmann::json();

        uint64_t index = 0;
        in->GetRegistry().each([&](auto entityID) {
            Entity e = Entity(entityID, in);
            SerializeEntity(outJson["Scene"], e, index);
            index++;
        });

        const size_t size = in->GetRegistry().view<IDComponent>().size();
        outJson["Scene"]["Size"] = size;

        String result = outJson.dump(4);
        FILE* f;
        errno_t e = fopen_s(&f, path, "w");
        if (f)
        {
            fwrite(result.c_str(), sizeof(char), result.size(), f);
            fclose(f);
        }
    }

    //------------
    // Deserialize
    //------------

    template <typename XComponent>
    FORCEINLINE static void DeserializeComponent(nlohmann::json& j, Entity& e)
    {
        const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<XComponent>();

        // Check if the json contains the component name, if it does, then proceed with Deserialization
        bool hasComponent = j.contains(clazz->GetName());
        if (!hasComponent)
            return;

        if (!e.HasComponent<XComponent>())
            e.AddComponent<XComponent>();

        nlohmann::json& inJson = j[clazz->GetName()];
        XComponent& comp = e.GetComponent<XComponent>();
        uint64_t offset = 0;

        for (const auto& [name, var] : clazz->GetVariables())
        {
            const SurgeReflect::Type& type = var.GetType();
            uint64_t size = var.GetSize();
            Byte* destination = reinterpret_cast<Byte*>(&comp) + offset;

            if (type.EqualTo<bool>())
            {
                const bool source = inJson[name];
                bool* dst = reinterpret_cast<bool*>(destination);
                std::memcpy(dst, &source, size);
            }
            else if (type.EqualTo<float>())
            {
                if (inJson.contains(name))
                {
                    const float source = inJson[name];
                    float* dst = reinterpret_cast<float*>(destination);
                    std::memcpy(dst, &source, size);
                }
            }
            else if (type.EqualTo<UUID>())
            {
                const uint64_t source = inJson[name];
                uint64_t* dst = reinterpret_cast<uint64_t*>(destination);
                std::memcpy(dst, &source, size);
            }
            else if (type.EqualTo<Vector<UUID>>())
            {
                const Vector<uint64_t> source = inJson[name];
                size = source.size();
                Vector<UUID>* dst = reinterpret_cast<Vector<UUID>*>(destination);
                dst->resize(source.size());
                std::memcpy(dst->data(), source.data(), (size * sizeof(UUID)));
            }
            else if (type.EqualTo<String>())
            {
                const String source = inJson[name];
                size = source.size(); // We cannot use var.GetSize() as that uses sizeof()

                String* src = reinterpret_cast<String*>(destination);
                src->resize(size);
                std::memcpy(src->data(), source.c_str(), size);
            }
            else if (type.EqualTo<Path>())
            {
//                 const String source = inJson[name];
//                 size = source.size();
// 
//                 Path* src = reinterpret_cast<Path*>(destination);
//                 *src = Path(fmt::format("{0}/{1}", e.GetScene()->GetParentProject()->GetMetadata().ProjPath.Str(), source));
// 
//                 size = src->Size();
            }
            else if (type.EqualTo<glm::vec3>())
            {
                glm::vec3 source = inJson[name];
                glm::vec3* dst = reinterpret_cast<glm::vec3*>(destination);
                std::memcpy(dst, glm::value_ptr(source), size);
            }
            else if (type.EqualTo<RuntimeCamera>())
            {
                RuntimeCamera cam;
                cam.SetPerspectiveVerticalFOV(inJson["Vertical FOV"]);
                cam.SetPerspectiveNearClip(inJson["Perspective NearClip"]);
                cam.SetPerspectiveFarClip(inJson["Perspective FarClip"]);
                cam.SetOrthographicNearClip(inJson["Orthographic NearClip"]);
                cam.SetOrthographicFarClip(inJson["Orthographic FarClip"]);
                cam.SetOrthographicSize(inJson["Orthographic Size"]);
                cam.SetProjectionType(inJson["Projection"]);
                std::memcpy(destination, &cam, size);
            }
            else if (type.EqualTo<Ref<Mesh>>())
            {
                String path = inJson[name];

                Ref<Mesh>& src = *reinterpret_cast<Ref<Mesh>*>(destination);
                path.empty() ? src = nullptr : src = Ref<Mesh>::Create(path);
            }
            else
                Log<Severity::Warn>("Unhandled Variable of type: '{0}' while deserializing!", type.GetFullName());

            offset += size;
        }
    }

    template <typename... Components>
    FORCEINLINE void DeserializeComponents(nlohmann::json& json, Entity& e)
    {
        (DeserializeComponent<Components>(json, e), ...);
    }

    static void DeserializeEntity(nlohmann::json& j, Entity& e, uint64_t index)
    {
        SG_ASSERT_NOMSG(e);

        nlohmann::json& inJson = j[fmt::format("Entity{0}", index)];
        DeserializeComponents<ALL_MAJOR_COMPONENTS>(inJson, e);
    }

    template <>
    void Serializer::Deserialize(const Path& path, Scene* out)
    {
        SG_ASSERT_NOMSG(out);
        auto& registry = out->GetRegistry();
        registry.clear();

        String jsonContents = Filesystem::ReadFile<String>(path);

        // Parse the json
        nlohmann::json parsedJson = nlohmann::json::parse(jsonContents);
        uint64_t size = parsedJson["Scene"]["Size"];

        for (uint64_t i = 0; i < size; i++)
        {
            Entity newEntity;
            out->CreateEntity(newEntity, "");
            DeserializeEntity(parsedJson["Scene"], newEntity, i);
        }
    }

    ///////////
    //Project//
    ///////////

//     template <>
//     void Serializer::Serialize(const Path& path, ProjectMetadata* in)
//     {
//         nlohmann::json outJson = nlohmann::json();
// 
//         outJson["Name"] = in->Name;
//         outJson["UUID"] = in->ProjectID.Get();
//         outJson["ProjPath"] = in->ProjPath;
//         outJson["InternalDirectory"] = in->InternalDirectory;
//         outJson["ProjectMetadataPath"] = in->ProjectMetadataPath;
//         outJson["ActiveSceneIndex"] = in->ActiveSceneIndex;
// 
//         nlohmann::json& sceneNode = outJson["Scenes"];
//         Uint index = 0;
//         for (auto& sceneMetadata : in->SceneMetadatas)
//         {
//             String idx = fmt::format("{0}", index);
//             sceneNode[idx]["UUID"] = sceneMetadata.SceneUUID.Get();
//             sceneNode[idx]["Name"] = sceneMetadata.Name;
//             String relativeScenePath;
// 
//             std::filesystem::path path(sceneMetadata.ScenePath.Str());
//             path.is_absolute() ? relativeScenePath = std::filesystem::relative(sceneMetadata.ScenePath.Str(), in->ProjPath.Str()).string() : relativeScenePath = sceneMetadata.ScenePath.Str();
//             sceneNode[idx]["Path"] = relativeScenePath;
//             index++;
//         }
//         sceneNode["Size"] = in->SceneMetadatas.size();
// 
//         String result = outJson.dump(4);
//         FILE* f;
//         errno_t e = fopen_s(&f, path, "w");
//         if (f)
//         {
//             fwrite(result.c_str(), sizeof(char), result.size(), f);
//             fclose(f);
//         }
//     }
// 
//     template <>
//     void Serializer::Deserialize(const Path& path, ProjectMetadata* out)
//     {
//         String jsonContents = Filesystem::ReadFile<String>(path);
//         nlohmann::json inJson = jsonContents.empty() ? nlohmann::json() : nlohmann::json::parse(jsonContents);
// 
//         out->Name = inJson["Name"];
//         if (inJson.contains("UUID"))
//             out->ProjectID = inJson["UUID"].get<uint64_t>();
//         out->ProjPath = inJson["ProjPath"].get<String>();
//         out->InternalDirectory = inJson["InternalDirectory"].get<String>();
//         out->ProjectMetadataPath = inJson["ProjectMetadataPath"].get<String>();
//         out->ActiveSceneIndex = inJson["ActiveSceneIndex"];
// 
//         nlohmann::json& sceneNode = inJson["Scenes"];
//         Uint size = sceneNode["Size"];
//         for (Uint i = 0; i < size; i++)
//         {
//             String idx = fmt::format("{0}", i);
//             SceneMetadata& metadata = out->SceneMetadatas.emplace_back();
// 
//             metadata.Name = sceneNode[idx]["Name"];
//             metadata.ScenePath = fmt::format("{0}/{1}", out->ProjPath, sceneNode[idx]["Path"].get<String>());
//             metadata.SceneUUID = sceneNode[idx]["UUID"].get<uint64_t>();
//         }
//     }

} // namespace Surge