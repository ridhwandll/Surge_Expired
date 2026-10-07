// Copyright (c) - SurgeTechnologies - All rights reserved
#include "History/SceneCommands.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Serializer/Serializer.hpp"

#include <json/json.hpp>
#include <unordered_map>

namespace Surge
{
    ///////////////////////
    // SceneEditUtils    //
    ///////////////////////

    UUID SceneEditUtils::GetUUID(Entity entity)
    {
        return entity ? entity.GetComponent<IDComponent>().ID : UUID(UUID::INVALID);
    }

    String SceneEditUtils::GetName(Entity entity)
    {
        return entity && entity.HasComponent<NameComponent>() ? entity.GetComponent<NameComponent>().Name : String("Entity");
    }

    Entity SceneEditUtils::FindEntity(Scene& scene, UUID id)
    {
        return id ? scene.FindEntityByUUID(id) : Entity {};
    }

    Entity SceneEditUtils::GetParent(Entity entity)
    {
        const Uint parent = entity.GetComponent<RelationshipComponent>().Parent;
        return parent != RELATIONSHIP_NULL ? Entity(static_cast<entt::entity>(parent), entity.GetScene()) : Entity {};
    }

    Entity SceneEditUtils::GetPreviousSibling(Entity entity)
    {
        const Uint previous = entity.GetComponent<RelationshipComponent>().PreviousSibling;
        return previous != RELATIONSHIP_NULL ? Entity(static_cast<entt::entity>(previous), entity.GetScene()) : Entity {};
    }

    bool SceneEditUtils::IsDescendantOf(Entity entity, Entity ancestor)
    {
        Entity current = GetParent(entity);

        while(current)
        {
            if(current == ancestor)
                return true;

            current = GetParent(current);
        }

        return false;
    }

    void SceneEditUtils::SetParentAt(Scene& scene, Entity entity, Entity parent, Entity insertAfter)
    {
        scene.SetParent(entity, parent);
        entity.GetComponent<TransformComponent>().MarkDirty(); // World transform depends on the parent

        // Root order is the registry order, only children have a sibling chain
        if(!parent)
            return;

        // An anchor that is not our sibling is meaningless, keep the appended position then
        if(insertAfter && (insertAfter == entity || insertAfter.GetComponent<RelationshipComponent>().Parent != static_cast<Uint>(parent.Raw())))
            return;

        entt::registry& registry = scene.GetRegistry();
        const Uint self = static_cast<Uint>(entity.Raw());
        RelationshipComponent& rel = entity.GetComponent<RelationshipComponent>();
        RelationshipComponent& parentRel = parent.GetComponent<RelationshipComponent>();

        // Unlink (SetParent appended us as the last child)
        if(rel.PreviousSibling != RELATIONSHIP_NULL)
            registry.get<RelationshipComponent>(static_cast<entt::entity>(rel.PreviousSibling)).NextSibling = rel.NextSibling;
        else
            parentRel.FirstChild = rel.NextSibling;
        if(rel.NextSibling != RELATIONSHIP_NULL)
            registry.get<RelationshipComponent>(static_cast<entt::entity>(rel.NextSibling)).PreviousSibling = rel.PreviousSibling;

        // Relink at the requested position
        if(insertAfter)
        {
            RelationshipComponent& previousRel = insertAfter.GetComponent<RelationshipComponent>();
            rel.PreviousSibling = static_cast<Uint>(insertAfter.Raw());
            rel.NextSibling = previousRel.NextSibling;
            if(previousRel.NextSibling != RELATIONSHIP_NULL)
                registry.get<RelationshipComponent>(static_cast<entt::entity>(previousRel.NextSibling)).PreviousSibling = self;
            previousRel.NextSibling = self;
        }
        else
        {
            rel.PreviousSibling = RELATIONSHIP_NULL;
            rel.NextSibling = parentRel.FirstChild;
            if(parentRel.FirstChild != RELATIONSHIP_NULL)
                registry.get<RelationshipComponent>(static_cast<entt::entity>(parentRel.FirstChild)).PreviousSibling = self;
            parentRel.FirstChild = self;
        }
    }

    ////////////////////////
    // EntityTreeSnapshot //
    ////////////////////////

    EntityTreeSnapshot EntityTreeSnapshot::Capture(Entity root)
    {
        EntityTreeSnapshot snapshot;
        if(!root)
            return snapshot;

        snapshot.RootParent = SceneEditUtils::GetUUID(SceneEditUtils::GetParent(root));
        snapshot.RootPreviousSibling = SceneEditUtils::GetUUID(SceneEditUtils::GetPreviousSibling(root));

        Scene* scene = root.GetScene();
        auto visit = [&](auto&& self, Entity entity, UUID parent) -> void {
            const UUID id = SceneEditUtils::GetUUID(entity);
            snapshot.Nodes.push_back({ id, parent, Serializer::SerializeEntity(entity) });

            Uint child = entity.GetComponent<RelationshipComponent>().FirstChild;
            while(child != RELATIONSHIP_NULL)
            {
                Entity childEntity(static_cast<entt::entity>(child), scene);
                self(self, childEntity, id);
                child = childEntity.GetComponent<RelationshipComponent>().NextSibling;
            }
        };
        visit(visit, root, UUID::INVALID);

        return snapshot;
    }

    Entity EntityTreeSnapshot::Restore(Scene& scene) const
    {
        std::unordered_map<UUID, Entity> created;
        created.reserve(Nodes.size());

        Entity root;
        for(const Node& node : Nodes)
        {
            Entity entity;
            scene.CreateEntityWithID(entity, node.ID, "");
            Serializer::DeserializeEntity(node.Data, entity);

            if(!root)
            {
                root = entity;
                SceneEditUtils::SetParentAt(scene, entity, SceneEditUtils::FindEntity(scene, RootParent), SceneEditUtils::FindEntity(scene, RootPreviousSibling));
            }
            else
                scene.SetParent(entity, created.at(node.Parent)); // Appending keeps the sibling order, Nodes are stored in that order

            created[node.ID] = entity;
        }

        return root;
    }

    Entity EntityTreeSnapshot::Instantiate(Scene& scene, Entity parent, Entity insertAfter) const
    {
        std::unordered_map<UUID, Entity> created; // Old UUID -> new entity
        created.reserve(Nodes.size());

        Entity root;
        for(const Node& node : Nodes)
        {
            Entity entity;
            scene.CreateEntity(entity, ""); // Fresh UUID, DeserializeEntity keeps it
            Serializer::DeserializeEntity(node.Data, entity);

            if(!root)
            {
                root = entity;
                SceneEditUtils::SetParentAt(scene, entity, parent, insertAfter);
            }
            else
                scene.SetParent(entity, created.at(node.Parent));

            created[node.ID] = entity;
        }

        return root;
    }

    ///////////////////////
    // Commands          //
    ///////////////////////

    // Names the step after what actually changed, so the History panel reads "Edit Transform of 'Cube'" instead of "Edit"
    static String DescribeEntityChange(const String& before, const String& after)
    {
        const nlohmann::json beforeJson = nlohmann::json::parse(before, nullptr, false);
        const nlohmann::json afterJson = nlohmann::json::parse(after, nullptr, false);
        if(beforeJson.is_discarded() || afterJson.is_discarded())
            return "Edit Entity";

        const String nameKey = SurgeReflect::GetReflection<NameComponent>()->GetName();
        auto entityName = [&nameKey](const nlohmann::json& j) -> String {
            return j.contains(nameKey) ? j[nameKey].value("Name", String("Entity")) : String("Entity");
        };

        Vector<String> added, removed, changed;
        for(auto it = afterJson.begin(); it != afterJson.end(); ++it)
        {
            if(!beforeJson.contains(it.key())) // If not in Before JSON then it is added
                added.push_back(it.key());
            else if(beforeJson[it.key()] != it.value()) // If value doesnt match then it is changed
                changed.push_back(it.key());
        }
        for(auto it = beforeJson.begin(); it != beforeJson.end(); ++it)
        {
            if(!afterJson.contains(it.key())) // If not in After JSON then it is removed
                removed.push_back(it.key());
        }

        const String name = entityName(afterJson);
        const size_t total = added.size() + removed.size() + changed.size();
        if(total == 1)
        {
            if(!added.empty())
                return std::format("Add {} to '{}'", added[0], name);
            if(!removed.empty())
                return std::format("Remove {} from '{}'", removed[0], name);
            if(changed[0] == nameKey)
                return std::format("Rename '{}' to '{}'", entityName(beforeJson), name);

            return std::format("Edit {} of '{}'", changed[0], name);
        }
        return std::format("Edit '{}'", name);
    }

    EntityChangeCommand::EntityChangeCommand(UUID id, String before, String after)
        : EditorCommand(DescribeEntityChange(before, after)), mID(id), mBefore(std::move(before)), mAfter(std::move(after)) {}

    UUID EntityChangeCommand::Apply(Scene& scene, const String& state) const
    {
        Entity entity = SceneEditUtils::FindEntity(scene, mID);
        if(!entity)
        {
            Log<Severity::Warn>("[EditorHistory] '{}': entity {} no longer exists", mName, mID.Get());
            return UUID::INVALID;
        }

        Serializer::DeserializeEntity(state, entity);
        return mID;
    }

    UUID EntityLifetimeCommand::Create(Scene& scene) const
    {
        Entity root = mSnapshot.Restore(scene);
        return SceneEditUtils::GetUUID(root);
    }

    UUID EntityLifetimeCommand::Destroy(Scene& scene) const
    {
        Entity root = SceneEditUtils::FindEntity(scene, mSnapshot.GetRootID());
        if(root)
            scene.DestroyEntityImmediate(root); // Removes children too

        return UUID::INVALID;
    }

    UUID ReparentCommand::Apply(Scene& scene, const Placement& placement) const
    {
        Entity entity = SceneEditUtils::FindEntity(scene, mEntity);
        Entity parent = SceneEditUtils::FindEntity(scene, placement.Parent);
        if(!entity || (placement.Parent && !parent))
        {
            Log<Severity::Warn>("[EditorHistory] '{}': entity or parent no longer exists", mName);
            return UUID::INVALID;
        }

        SceneEditUtils::SetParentAt(scene, entity, parent, SceneEditUtils::FindEntity(scene, placement.PreviousSibling));
        return mEntity;
    }

} // namespace Surge
