// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptSourceWriter.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/AssetManager.hpp"

#include <glm/glm.hpp>

namespace Surge::ScriptSourceWriter
{
    String GetDefaultScriptContent()
    {
        return R"(
function OnCreate(entity)
Log.Info("Hello from create!")
end

function OnUpdate(entity, dt)
Log.Info("Hello from update!")
end

function OnDestroy(entity)
Log.Info("Hello from destroy!")
end
)";
    }

    bool WriteNew(const String& absolutePath)
    {
        String sourceCode = GetDefaultScriptContent();
        return Filesystem::WriteTextFile(absolutePath, sourceCode);
    }

}

