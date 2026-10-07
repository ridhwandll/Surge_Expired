// Copyright (c) - SurgeTechnologies - All rights reserved
#include "History/EditorHistory.hpp"
#include "History/SceneCommands.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Serializer/Serializer.hpp"

namespace Surge
{
    static const String sEmptyName;

    void EditorHistory::SetScene(Scene* scene)
    {
        Clear();
        mScene = scene;
    }

    void EditorHistory::Clear()
    {
        mCommands.clear();
        mPosition = 0;
        mSavedPosition = 0;
        ResetTracking();
    }

    void EditorHistory::Push(Scope<EditorCommand> command)
    {
        FlushTracked();
        PushInternal(std::move(command));
    }

    void EditorHistory::PushInternal(Scope<EditorCommand> command)
    {
        // A new edit drops the redo branch, the saved state might have lived there
        if(mSavedPosition > static_cast<int64_t>(mPosition))
            mSavedPosition = -1;

        mCommands.erase(mCommands.begin() + static_cast<std::ptrdiff_t>(mPosition), mCommands.end());  // Kill the redo branch
        mCommands.push_back(std::move(command));                                                       // Append
        mPosition++;

        if(mCommands.size() > MAX_STEPS)
        {
            mCommands.erase(mCommands.begin());
            mPosition--;
            if(mSavedPosition >= 0)
                mSavedPosition--; // 0 -> -1: the saved state was the dropped one
        }
    }

    bool EditorHistory::Undo(UUID& outSelection)
    {
        if(!mScene)
            return false;

        FlushTracked();
        if(!CanUndo())
            return false;

        mPosition--;
        outSelection = mCommands[mPosition]->Undo(*mScene);
        ResyncTracked();
        return true;
    }

    bool EditorHistory::Redo(UUID& outSelection)
    {
        if(!mScene)
            return false;

        FlushTracked();
        if(!CanRedo())
            return false;

        outSelection = mCommands[mPosition]->Redo(*mScene);
        mPosition++;
        ResyncTracked();
        return true;
    }

    bool EditorHistory::JumpTo(size_t position, UUID& outSelection)
    {
        if(position > mCommands.size())
            return false;

        bool changed = false;
        while(mPosition > position && Undo(outSelection))
            changed = true;
        while(mPosition < position && Redo(outSelection))
            changed = true;
        return changed;
    }

    const String& EditorHistory::GetUndoName() const
    {
        return CanUndo() ? mCommands[mPosition - 1]->GetName() : sEmptyName;
    }

    const String& EditorHistory::GetRedoName() const
    {
        return CanRedo() ? mCommands[mPosition]->GetName() : sEmptyName;
    }

    void EditorHistory::TrackSelection(Entity selected, bool interacting)
    {
        if(!mScene)
            return;

        const bool validSelection = selected && selected.GetScene() == mScene;
        const UUID selectedID = validSelection ? selected.GetComponent<IDComponent>().ID : UUID(UUID::INVALID);

        if(selectedID != mTrackedID)
        {
            FlushTracked(); // Changes made to the previous selection before the switch
            mTrackedID = selectedID;
            mTrackedHandle = validSelection ? selected.Raw() : entt::null;
            mTrackedState = validSelection ? Serializer::SerializeEntity(selected) : String();
            mWasInteracting = interacting;
            mPendingChecks = 0;
            return;
        }

        if(!mTrackedID)
            return;

        if(interacting)
        {
            mWasInteracting = true;
            return;
        }

        // Check on the frame the interaction ends and on the next one, edits deferred to the frame end (e.g. Remove Component) land in between
        if(mWasInteracting)
        {
            mWasInteracting = false;
            mPendingChecks = 2; 
            // (Rid) Why 2 checks?
            // The user clicks "Remove Component → Rigidbody".The editor doesn't remove it immediately;
            // it queues the removal for the end of the frame, after TrackSelection has already run. So due to deffered deletion, there is 2 checks
        }

        if(mPendingChecks > 0)
        {
            mPendingChecks--;
            FlushTracked();
        }
    }

    void EditorHistory::FlushTracked()
    {
        if(!mTrackedID || !mScene)
            return;

        Entity entity = FindTrackedEntity();
        if(!entity)
        {
            ResetTracking(); // Destroyed, the delete command (if any) owns that change
            return;
        }

        String current = Serializer::SerializeEntity(entity);
        if(current == mTrackedState)
            return;

        String before = std::move(mTrackedState);
        mTrackedState = current;
        PushInternal(CreateScope<EntityChangeCommand>(mTrackedID, std::move(before), std::move(current)));
    }

    void EditorHistory::ResetTracking()
    {
        mTrackedID = UUID::INVALID;
        mTrackedHandle = entt::null;
        mTrackedState.clear();
        mWasInteracting = false;
        mPendingChecks = 0;
    }

    void EditorHistory::ResyncTracked()
    {
        // The command might have changed (or recreated) the tracked entity, re-baseline so that is not seen as a new edit
        Entity entity = FindTrackedEntity();
        if(entity)
        {
            mTrackedState = Serializer::SerializeEntity(entity);
            mWasInteracting = false;
            mPendingChecks = 0;
        }
        else
            ResetTracking();
    }

    Entity EditorHistory::FindTrackedEntity()
    {
        entt::registry& registry = mScene->GetRegistry();
        if(mTrackedHandle != entt::null && registry.valid(mTrackedHandle)) // Fast path
        {
            const IDComponent* idComponent = registry.try_get<IDComponent>(mTrackedHandle);
            if(idComponent && idComponent->ID == mTrackedID)
                return Entity(mTrackedHandle, mScene);
        }

        // Slow path
        Entity entity = mScene->FindEntityByUUID(mTrackedID);
        mTrackedHandle = entity ? entity.Raw() : entt::null;
        return entity;
    }

} // namespace Surge
