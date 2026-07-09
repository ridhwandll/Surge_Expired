// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

#define BIND_PROP(STRUCTT, PROP) \
        sol::property( \
            [](STRUCTT& c) -> decltype(STRUCTT::PROP) { return c.PROP; }, \
            [](STRUCTT& c, const decltype(STRUCTT::PROP)& val) { c.PROP = val; } \
        )

#define STRICT_READ(STRUCTT) \
        sol::meta_function::index, [](STRUCTT&, sol::stack_object key, sol::this_state s) -> sol::object { \
            String keyStr = key.is<String>() ? key.as<String>() : "<non-string-key>"; \
            Log<Severity::Warn>("Script accessed invalid field {} on {}. Returning nil.", keyStr, #STRUCTT); \
            return sol::make_object(s, sol::lua_nil); \
        }