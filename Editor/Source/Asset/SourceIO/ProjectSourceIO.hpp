// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Project.hpp"

namespace Surge
{
    // <Name>.surgeproj, the Editor's JSON project file. Exported games load the cooked Project::COOKED_FILE_NAME the export writes
    namespace ProjectSourceIO
    {
        bool Save(const Project& project, const Path& path);
        bool Load(const Path& path, Project& outProject); // Throws on malformed JSON
    }
}
