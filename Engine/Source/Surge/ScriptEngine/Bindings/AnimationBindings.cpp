// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AnimationBindings.hpp"
#include "BindingUtils.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include <glm/gtc/quaternion.hpp>

namespace Surge::ScriptBinding
{
    // A joint index (FindJoint) or a joint name
    static int ResolveJoint(const AnimatorComponent& component, const sol::object& joint)
    {
        if(joint.is<int>())
            return joint.as<int>();
        if(joint.is<String>())
            return component.Runtime.FindJoint(joint.as<String>());
        return -1;
    }

    void BindAnimation(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        lua.new_usertype<AnimatorComponent>("AnimatorComponent", sol::no_constructor,
                                            "Active", BIND_PROP(AnimatorComponent, Active),
                                            "Clip", BIND_PROP(AnimatorComponent, Clip),
                                            "PlayOnAwake", BIND_PROP(AnimatorComponent, PlayOnAwake),
                                            "Loop", BIND_PROP(AnimatorComponent, Loop),
                                            "Speed", BIND_PROP(AnimatorComponent, Speed),
                                            "AnimateOffscreen", BIND_PROP(AnimatorComponent, AnimateOffscreen),
                                            "Paused", sol::property([](AnimatorComponent& c) { return c.Runtime.IsPaused(); },
                                                                    [](AnimatorComponent& c, bool paused) { c.Runtime.SetPaused(paused); }),
                                            "CurrentClip", sol::property([](AnimatorComponent& c) { return c.Runtime.GetClipName(); }),
                                            "Play", [](AnimatorComponent& c, const String& clip, sol::optional<float> fade) { return c.Runtime.Play(clip, fade.value_or(0.0f)); },
                                            "Stop", [](AnimatorComponent& c) { c.Runtime.Stop(); },
                                            "IsPlaying", [](AnimatorComponent& c, sol::optional<String> clip) { return clip ? c.Runtime.IsPlaying(*clip) : c.Runtime.IsPlaying(); },
                                            "HasClip", [](AnimatorComponent& c, const String& clip) { return c.Runtime.HasClip(clip); },
                                            "GetTime", [](AnimatorComponent& c) { return c.Runtime.GetTime(); },
                                            "SetTime", [](AnimatorComponent& c, float time) { c.Runtime.SetTime(time); },
                                            "GetDuration", [](AnimatorComponent& c, sol::optional<String> clip) { return clip ? c.Runtime.GetDuration(*clip) : c.Runtime.GetDuration(); },
                                            "GetClipNames", [](AnimatorComponent& c, sol::this_state s) {
                                                sol::state_view state(s);
                                                sol::table names = state.create_table(static_cast<int>(c.Runtime.GetClipCount()), 0);
                                                for(Uint i = 0; i < c.Runtime.GetClipCount(); i++)
                                                    names[i + 1] = c.Runtime.GetClipNameAt(i);
                                                return names;
                                            },
                                            "FindJoint", [](AnimatorComponent& c, const String& name) { return c.Runtime.FindJoint(name); },
                                            // World position + rotation (degrees, like TransformC.Rotation) of a joint in this frame's pose, nil before the first pose
                                            "GetJointWorldTransform", [](AnimatorComponent& c, sol::object joint, sol::this_state s) -> std::tuple<sol::object, sol::object> {
                                                const int index = ResolveJoint(c, joint);
                                                if(!c.Runtime.HasPose() || index < 0 || static_cast<Uint>(index) >= c.Runtime.GetJointCount())
                                                    return { sol::make_object(s, sol::lua_nil), sol::make_object(s, sol::lua_nil) };

                                                const glm::mat4 world = c.Runtime.GetJointWorldTransform(index);
                                                const glm::vec3 scale(glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), glm::length(glm::vec3(world[2])));
                                                const glm::mat3 basis(glm::vec3(world[0]) / glm::max(scale.x, 1e-8f), glm::vec3(world[1]) / glm::max(scale.y, 1e-8f), glm::vec3(world[2]) / glm::max(scale.z, 1e-8f));
                                                const glm::vec3 rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(basis)));
                                                return { sol::make_object(s, glm::vec3(world[3])), sol::make_object(s, rotation) };
                                            },
                                            STRICT_READ(AnimatorComponent));
    }
}
