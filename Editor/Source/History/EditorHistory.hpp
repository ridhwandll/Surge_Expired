// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Scope.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/ECS/Scene.hpp"

namespace Surge
{
    // An already applied, reversible scene edit. Undo/Redo return the entity to select afterwards (INVALID clears the selection)
    // Commands only reference entities by UUID: undo/redo recreate entities, so entt handles do not survive
    class EditorCommand
    {
    public:
        explicit EditorCommand(String name)
            : mName(std::move(name)) {}
        virtual ~EditorCommand() = default;

        virtual UUID Undo(Scene& scene) = 0;
        virtual UUID Redo(Scene& scene) = 0;

        const String& GetName() const { return mName; }

    protected:
        String mName;
    };

    /*
     * Scene Undo/Redo (Edit mode only)
     * - Create, delete, duplicate, paste, reparent are pushed explicitly as commands by the Editor
     * - Property edits are picked up automatically: the selected entity is snapshotted (Serializer::SerializeEntity) and when an interaction
     *   ends (mouse released, widget deactivated) the snapshot is compared against the live entity, a difference becomes one undo step.
     *   Every Inspector field, the gizmo and Add/Remove Component are undoable this way without the widgets knowing about the history,
     *   and a whole drag/typing session merges into a single step
     */
    class EditorHistory
    {
    public:
        static constexpr size_t MAX_STEPS = 256;

        EditorHistory() = default;
        ~EditorHistory() = default;
        SURGE_DISABLE_COPY(EditorHistory);

        void SetScene(Scene* scene); // Also clears the history
        void Clear();

        // 'command' must already be applied. Pending tracked changes are committed first so the steps stay in order
        void Push(Scope<EditorCommand> command);

        // Return false when there was nothing to do, outSelection is the entity to select
        bool Undo(UUID& outSelection);
        bool Redo(UUID& outSelection);
        bool JumpTo(size_t position, UUID& outSelection);

        bool CanUndo() const { return mPosition > 0; }
        bool CanRedo() const { return mPosition < mCommands.size(); }
        const String& GetUndoName() const;
        const String& GetRedoName() const;

        size_t GetPosition() const { return mPosition; }
        size_t GetSize() const { return mCommands.size(); }
        const EditorCommand& GetCommand(size_t index) const { return *mCommands[index]; }

        void MarkSaved() { mSavedPosition = static_cast<int64_t>(mPosition); }
        bool IsDirty() const { return mSavedPosition != static_cast<int64_t>(mPosition); }
        bool IsSavedPosition(size_t position) const { return mSavedPosition == static_cast<int64_t>(position); }

        // Selection tracker, call once per frame (edit mode) after every panel had the chance to modify the selected entity
        void TrackSelection(Entity selected, bool interacting);
        void FlushTracked(); // Commits pending changes of the tracked entity right away
        void ResetTracking(); // Forgets the tracked entity without committing (e.g. play mode)

    private:
        void PushInternal(Scope<EditorCommand> command);
        void ResyncTracked();
        Entity FindTrackedEntity();

    private:
        Scene* mScene = nullptr;
        Vector<Scope<EditorCommand>> mCommands;
        size_t mPosition = 0; // Position inside mCommands (cursor)
        int64_t mSavedPosition = 0; // -1 when the saved state is no longer reachable (dropped redo branch or trimmed history)

        // Selection tracker
        UUID mTrackedID = UUID::INVALID;          // (Rid) which entity we're watching
        entt::entity mTrackedHandle = entt::null; // Fast path, validated against mTrackedID before use
        String mTrackedState;                     // Serialized snapshot = "before"
        bool mWasInteracting = false;
        int mPendingChecks = 0;
    };

    /*
    * UNDO/REDO system explanation (Rid):
    * mCommands:  [A] [B] [C] [D]
                            ^
                     mPosition = 3
      Applied: [A], [B], [C]      Redoable: [D]

      [0, mPosition) is applied(can be undone) and [mPosition, end) can be redone
      Undo moves the cursor left(mPosition--) and calls mCommands[mPosition]->Undo().
      Redo calls mCommands[mPosition]->Redo() and moves the cursor right(mPosition++).
      CanUndo is mPosition > 0, and CanRedo is mPosition < size.
    */

} // namespace Surge
