// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    class UILayout;

    // UILayoutBinary (.rUI_LAYOUT) payload, cooked from the Editor's .sui JSON source
    // CanvasScaler  ReferenceResolution, MatchWidthOrHeight
    // WidgetDesc    pre-order tree: every field of the flat desc (defaults included, enums at their own size), ChildCount, then the children.
    //               Fonts/textures stay AssetIDs, resolved when the layout is instantiated
    namespace UILayoutBinary
    {
        // Bump when a WidgetDesc/ImagePartDesc/CanvasScaler field is added/removed/reordered
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, const UILayout& layout);
        bool Read(Rapt0R::Reader& reader, UILayout& outLayout);
    }

    class UILayoutFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::UI_LAYOUT; }
        virtual uint16_t GetFormatVersion() const override { return UILayoutBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
