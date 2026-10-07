// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"

namespace Surge
{
    struct ConsoleMessage;

    // Shows EditorConsole: severity filters, search, collapsed duplicates (xN), details + copy for the selected message
    class ConsolePanel : public IPanel
    {
    public:
        ConsolePanel() = default;
        virtual ~ConsolePanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {}
        virtual void Render(bool* show) override;
        virtual void Shutdown() override {}

        static PanelCode GetStaticCode() { return PanelCode::Console; }

        bool IsClearOnPlay() const { return mClearOnPlay; }
        void Focus();
        void FocusWithSearch(const char* search); // e.g. "[Export]" to show one subsystem's log

    private:
        void DrawToolbar();
        void DrawMessages();
        void DrawDetails();
        void RebuildFilter();
        bool PassesFilter(const ConsoleMessage& message) const;

    private:
        PanelCode mCode;

        bool mShowLogs = true;     // Trace, Info, Debug
        bool mShowWarnings = true;
        bool mShowErrors = true;   // Error, Fatal
        bool mClearOnPlay = false;
        bool mAutoScroll = true;
        char mSearch[256] = "";

        Vector<Uint> mFiltered;    // Indices into EditorConsole::GetMessages()
        uint64_t mFilteredRevision = UINT64_MAX;
        bool mFilterDirty = true;
        bool mScrollToBottom = false;
        bool mClearRequested = false;
        uint64_t mSelectedID = 0;

        bool mFocusWindow = false;
    };

} // namespace Surge
