// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "History/EditorHistory.hpp"

namespace Surge
{
    // An entity and all of its descendants serialized, used to undo deletes, redo creates and for copy/paste/duplicate
    struct EntityTreeSnapshot
    {
        struct Node
        {
            UUID ID;
            UUID Parent; // INVALID for the root of the tree
            String Data; // Serializer::SerializeEntity
        };

        Vector<Node> Nodes; // Parents always come before their children, siblings keep their order
        UUID RootParent = UUID::INVALID; // Where the root lived in the hierarchy
        UUID RootPreviousSibling = UUID::INVALID;

        static EntityTreeSnapshot Capture(Entity root);

        bool IsValid() const { return !Nodes.empty(); }
        UUID GetRootID() const { return Nodes.empty() ? UUID(UUID::INVALID) : Nodes.front().ID; }

        // Recreates the very same entities (same UUIDs) at their original place in the hierarchy
        Entity Restore(Scene& scene) const;

        // Creates a copy with new UUIDs under 'parent' (root level when null), placed right after 'insertAfter' (first child when null)
        Entity Instantiate(Scene& scene, Entity parent, Entity insertAfter) const;
    };

    namespace SceneEditUtils
    {
        UUID GetUUID(Entity entity); // INVALID for a null entity
        String GetName(Entity entity);
        Entity FindEntity(Scene& scene, UUID id); // Null for INVALID ids
        Entity GetParent(Entity entity);
        Entity GetPreviousSibling(Entity entity);
        bool IsDescendantOf(Entity entity, Entity ancestor);

        // Scene::SetParent() always appends, this also restores the position among the new siblings ('insertAfter' null = first child)
        void SetParentAt(Scene& scene, Entity entity, Entity parent, Entity insertAfter);
    } // namespace SceneEditUtils

    // Component/property changes of one entity, produced by EditorHistory's selection tracker
    class EntityChangeCommand final : public EditorCommand
    {
    public:
        EntityChangeCommand(UUID id, String before, String after);

        virtual UUID Undo(Scene& scene) override { return Apply(scene, mBefore); }
        virtual UUID Redo(Scene& scene) override { return Apply(scene, mAfter); }

    private:
        UUID Apply(Scene& scene, const String& state) const;

    private:
        UUID mID;
        String mBefore;
        String mAfter;
    };

    // An entity tree that appeared (create, duplicate, paste) or disappeared (delete)
    class EntityLifetimeCommand final : public EditorCommand
    {
    public:
        enum class Kind
        {
            CREATED,
            DELETED
        };

        EntityLifetimeCommand(String name, Kind kind, EntityTreeSnapshot snapshot)
            : EditorCommand(std::move(name)), mKind(kind), mSnapshot(std::move(snapshot)) {}

        virtual UUID Undo(Scene& scene) override { return mKind == Kind::CREATED ? Destroy(scene) : Create(scene); }
        virtual UUID Redo(Scene& scene) override { return mKind == Kind::CREATED ? Create(scene) : Destroy(scene); }

    private:
        UUID Create(Scene& scene) const;
        UUID Destroy(Scene& scene) const;

    private:
        Kind mKind;
        EntityTreeSnapshot mSnapshot;
    };

    class ReparentCommand final : public EditorCommand
    {
    public:
        struct Placement
        {
            UUID Parent = UUID::INVALID;
            UUID PreviousSibling = UUID::INVALID;
        };

        ReparentCommand(String name, UUID entity, const Placement& from, const Placement& to)
            : EditorCommand(std::move(name)), mEntity(entity), mFrom(from), mTo(to) {}

        virtual UUID Undo(Scene& scene) override { return Apply(scene, mFrom); }
        virtual UUID Redo(Scene& scene) override { return Apply(scene, mTo); }

    private:
        UUID Apply(Scene& scene, const Placement& placement) const;

    private:
        UUID mEntity;
        Placement mFrom;
        Placement mTo;
    };

} // namespace Surge
