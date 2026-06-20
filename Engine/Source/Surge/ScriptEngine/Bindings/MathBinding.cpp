// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MathBinding.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/gtx/color_space.hpp>
#include <random>

namespace Surge::ScriptBinding
{
    void BindMath(void* luaState)
    {
        sol::state_view* lua = static_cast<sol::state_view*>(luaState);

        // Global Math namespace in Lua
        auto math = lua->create_table("Math");

        math["Pi"] = glm::pi<float>();
        math["Epsilon"] = glm::epsilon<float>();

        // Vector 2
        math.new_usertype<glm::vec2>("Vec2",
            sol::constructors<glm::vec2(), glm::vec2(float), glm::vec2(float, float)>(),
            "x", sol::property(
                [](glm::vec2& v) -> float { return v.x; },
                [](glm::vec2& v, float val) { v.x = val; }),
            "y", sol::property(
                [](glm::vec2& v) -> float { return v.y; },
                [](glm::vec2& v, float val) { v.y = val; }),

            // Operators
            sol::meta_function::addition,       [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
            sol::meta_function::subtraction,    [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec2& a, const glm::vec2& b) { return a * b; },
                [](const glm::vec2& a, float s) { return a * s; },
                [](float s, const glm::vec2& a) { return a * s; }
            ),
            sol::meta_function::division,       sol::overload(
                [](const glm::vec2& a, const glm::vec2& b) { return a / b; },
                [](const glm::vec2& a, float s) { return a / s; }
            ),
            sol::meta_function::unary_minus,    [](const glm::vec2& v) { return -v; },
            sol::meta_function::equal_to,       [](const glm::vec2& a, const glm::vec2& b) { return a == b; },
            sol::meta_function::to_string,      [](const glm::vec2& v) { return std::format("Vec2({:.3f}, {:.3f})", v.x, v.y); },

            // Methods
            "Length",    [](const glm::vec2& v) { return glm::length(v); },
            "Normalize", [](const glm::vec2& v) { return glm::length(v) > 0 ? glm::normalize(v) : glm::vec2(0.0f); },
            "Dot",       [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
            "Distance",  [](const glm::vec2& a, const glm::vec2& b) { return glm::distance(a, b); }
        );

        // Vector 3
        math.new_usertype<glm::vec3>("Vec3",
            sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float)>(),
            "x", sol::property([](glm::vec3& v) -> float { return v.x; }, [](glm::vec3& v, float val) { v.x = val; }),
            "y", sol::property([](glm::vec3& v) -> float { return v.y; }, [](glm::vec3& v, float val) { v.y = val; }),
            "z", sol::property([](glm::vec3& v) -> float { return v.z; }, [](glm::vec3& v, float val) { v.z = val; }),

            // Operators
            sol::meta_function::addition,       [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
            sol::meta_function::subtraction,    [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec3& a, const glm::vec3& b) { return a * b; },
                [](const glm::vec3& a, float s) { return a * s; },
                [](float s, const glm::vec3& a) { return a * s; }
            ),
            sol::meta_function::division,       sol::overload(
                [](const glm::vec3& a, const glm::vec3& b) { return a / b; },
                [](const glm::vec3& a, float s) { return a / s; }
            ),
            sol::meta_function::unary_minus,    [](const glm::vec3& v) { return -v; },
            sol::meta_function::equal_to,       [](const glm::vec3& a, const glm::vec3& b) { return a == b; },
            sol::meta_function::to_string,      [](const glm::vec3& v) { return std::format("Vec3({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z); },

            // Methods
            "Length",    [](const glm::vec3& v) { return glm::length(v); },
            "Normalize", [](const glm::vec3& v) { return glm::length(v) > 0 ? glm::normalize(v) : glm::vec3(0.0f); },
            "Dot",       [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
            "Cross",     [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); },
            "Distance",  [](const glm::vec3& a, const glm::vec3& b) { return glm::distance(a, b); },
            "Reflect",   [](const glm::vec3& i, const glm::vec3& n) { return glm::reflect(i, n); },
            "Refract",   [](const glm::vec3& i, const glm::vec3& n, float eta) { return glm::refract(i, n, eta); }
        );

        // Vector 4
        math.new_usertype<glm::vec4>("Vec4",
            sol::constructors<glm::vec4(), glm::vec4(float), glm::vec4(float, float, float, float)>(),
            "x", sol::property([](glm::vec4& v) -> float { return v.x; }, [](glm::vec4& v, float val) { v.x = val; }),
            "y", sol::property([](glm::vec4& v) -> float { return v.y; }, [](glm::vec4& v, float val) { v.y = val; }),
            "z", sol::property([](glm::vec4& v) -> float { return v.z; }, [](glm::vec4& v, float val) { v.z = val; }),
            "w", sol::property([](glm::vec4& v) -> float { return v.w; }, [](glm::vec4& v, float val) { v.w = val; }),

            // Operators
            sol::meta_function::addition,       [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
            sol::meta_function::subtraction,    [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec4& a, float s) { return a * s; },
                [](float s, const glm::vec4& a) { return a * s; }),
            sol::meta_function::division, sol::overload(
                [](const glm::vec4& a, const glm::vec4& b) { return a / b; },
                [](const glm::vec4& a, float s) { return a / s; }),

            sol::meta_function::unary_minus, [](const glm::vec4& v) { return -v; },
            sol::meta_function::equal_to, [](const glm::vec4& a, const glm::vec4& b) { return a == b; },
            sol::meta_function::to_string,      [](const glm::vec4& v) { return std::format("Vec4({:.3f}, {:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z, v.w); },

            "Length", [](const glm::vec4& v) { return glm::length(v); },
            "Normalize", [](const glm::vec4& v) { return glm::length(v) > 0 ? glm::normalize(v) : glm::vec4(0.0f); },
            "Dot", [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); },
            "Distance", [](const glm::vec4& a, const glm::vec4& b) { return glm::distance(a, b); },
            "Reflect", [](const glm::vec4& i, const glm::vec4& n) { return glm::reflect(i, n); },
            "Refract", [](const glm::vec4& i, const glm::vec4& n, float eta) { return glm::refract(i, n, eta); }
        );

        // Quaternions (Rotations)
        math.new_usertype<glm::quat>("Quat",
            sol::constructors<glm::quat(), glm::quat(float, float, float, float), glm::quat(glm::vec3)>(),
            "x", sol::property([](glm::quat& q) -> float { return q.x; }, [](glm::quat& q, float val) { q.x = val; }),
            "y", sol::property([](glm::quat& q) -> float { return q.y; }, [](glm::quat& q, float val) { q.y = val; }),
            "z", sol::property([](glm::quat& q) -> float { return q.z; }, [](glm::quat& q, float val) { q.z = val; }),
            "w", sol::property([](glm::quat& q) -> float { return q.w; }, [](glm::quat& q, float val) { q.w = val; }),
            
            // Operators
            sol::meta_function::multiplication, sol::overload(
                [](const glm::quat& a, const glm::quat& b) { return a * b; }, // Combine rotations
                [](const glm::quat& q, const glm::vec3& v) { return q * v; }  // Rotate vector
            ),
            sol::meta_function::to_string,      [](const glm::quat& q) { return std::format("Quat({:.3f}, {:.3f}, {:.3f}, {:.3f})", q.x, q.y, q.z, q.w); },

            // Methods
            "Normalize",    [](const glm::quat& q) { return glm::normalize(q); },
            "Inverse",      [](const glm::quat& q) { return glm::inverse(q); },
            "Slerp",        [](const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); },
            "Euler",        [](const glm::quat& q) { return glm::eulerAngles(q); },
            "LookRotation", [](const glm::vec3& forward, const glm::vec3& up)
                            {
                                float lenSq = glm::dot(forward, forward);
                                if(lenSq < glm::epsilon<float>()) return glm::quat();
                                glm::vec3 f = forward / glm::sqrt(lenSq);
                                glm::mat4 viewMatrix = glm::lookAt(glm::vec3(0.0f), f, up);
                                return glm::conjugate(glm::quat_cast(viewMatrix));
                            }
        );

        // Matrix 4x4
        math.new_usertype<glm::mat4>("Mat4",
            sol::constructors<glm::mat4(), glm::mat4(float)>(), // Identity constructor Mat4(1.0)
            
            // Operators
            sol::meta_function::multiplication, sol::overload(
                [](const glm::mat4& a, const glm::mat4& b) { return a * b; },
                [](const glm::mat4& m, const glm::vec4& v) { return m * v; }
            ),

            // Static Generators
            "Translate", [](const glm::mat4& m, const glm::vec3& v) { return glm::translate(m, v); },
            "Rotate",    [](const glm::mat4& m, float angleRad, const glm::vec3& axis) { return glm::rotate(m, angleRad, axis); },
            "Scale",     [](const glm::mat4& m, const glm::vec3& v) { return glm::scale(m, v); },
            "Inverse",   [](const glm::mat4& m) { return glm::inverse(m); },
            "Identity",  []() { return glm::mat4(1.0f); }
        );

        // Global Math Functions
        math["Up"] = []() { return glm::vec3(0.0f, 1.0f, 0.0f); };
        math["Right"] = []() { return glm::vec3(1.0f, 0.0f, 0.0f); };
        math["Forward"] = []() { return glm::vec3(0.0f, 0.0f, -1.0f); };

        // Trigonometry (Radians)
        math["Sin"]   = [](float x) { return glm::sin(x); };
        math["Cos"]   = [](float x) { return glm::cos(x); };
        math["Tan"]   = [](float x) { return glm::tan(x); };
        math["Asin"]  = [](float x) { return glm::asin(x); };
        math["Acos"]  = [](float x) { return glm::acos(x); };
        math["Atan2"] = [](float y, float x) { return glm::atan(y, x); };

        // Conversions
        math["ToRadians"] = [](float degrees) { return glm::radians(degrees); };
        math["ToDegrees"] = [](float radians) { return glm::degrees(radians); };

        // General Math
        math["Abs"]   = [](float x) { return glm::abs(x); };
        math["Sign"]  = [](float x) { return glm::sign(x); };
        math["Floor"] = [](float x) { return glm::floor(x); };
        math["Ceil"]  = [](float x) { return glm::ceil(x); };
        math["Round"] = [](float x) { return glm::round(x); };
        math["Pow"]   = [](float base, float exp) { return glm::pow(base, exp); };
        math["Sqrt"]  = [](float x) { return glm::sqrt(x); };
        
        math["Min"] = [](float a, float b) { return glm::min(a, b); };
        math["Max"] = [](float a, float b) { return glm::max(a, b); };

        // Interpolation & Bounds
        math["Clamp"] = [](float x, float minVal, float maxVal) { return glm::clamp(x, minVal, maxVal); };
        math["Lerp"]  = [](float a, float b, float t) { return glm::mix(a, b, t); };
        math["LerpVec3"] = [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); };
        math["SmoothStep"] = [](float edge0, float edge1, float x) { return glm::smoothstep(edge0, edge1, x); };

        // Randomization // TODO: Revisit this to use a better RNG and allow seeding from Lua
        static std::mt19937 gen(std::random_device{}());
        math["Random"] = []() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen); };        
        math["RandomRange"] = [](float min, float max) { return std::uniform_real_distribution<float>(min, max)(gen); };

        // Colors
        math["HSVToRGB"] = [](const glm::vec3& hsv) { return glm::rgbColor(hsv); };
        math["RGBToHSV"] = [](const glm::vec3& rgb) { return glm::hsvColor(rgb); };

        math["HexToRGB"] = [](const String& hex) {
            // "#FF0000" to Vec3(1.0, 0.0, 0.0)
            if(hex.length() < 6)
                return glm::vec3(1.0f);

            String cleanHex = hex[0] == '#' ? hex.substr(1) : hex;
            int r, g, b;
            std::sscanf(cleanHex.c_str(), "%02x%02x%02x", &r, &g, &b);
            return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
            };

        sol::table easing = lua->create_table();
        math["Easing"] = easing;

        easing["QuadIn"] = [](float t) { return t * t; };
        easing["QuadOut"] = [](float t) { return t * (2.0f - t); };
        easing["QuadInOut"] = [](float t) { return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t; };
        easing["ElasticOut"] = [](float t) {
            const float c4 = (2.0f * glm::pi<float>()) / 3.0f;
            return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f :
                glm::pow(2.0f, -10.0f * t) * glm::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
            };
    }
}
