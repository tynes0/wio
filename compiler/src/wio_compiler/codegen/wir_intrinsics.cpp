#include "wio/codegen/wir_intrinsics.h"

namespace wio::codegen
{
    std::optional<std::string> wirIntrinsicHelper(
        const wir::IntrinsicFamily family, const std::string_view selector)
    {
        using wir::IntrinsicFamily;
        std::string_view members;
        std::string_view prefix;
        switch (family)
        {
        case IntrinsicFamily::Array:
            prefix = "Array";
            members = "|Capacity|Contains|IndexOf|LastIndexOf|First|Last|GetOr|Clone|Slice|Take|Skip|Concat|Reversed|Join|Push|PushFront|Pop|PopFront|Insert|RemoveAt|Remove|Clear|Extend|Reserve|ShrinkToFit|Fill|Reverse|Sort|Sorted|";
            break;
        case IntrinsicFamily::Dictionary:
            prefix = "Dict";
            members = "|ContainsKey|ContainsValue|Get|GetOr|TryGet|Set|GetOrAdd|Keys|Values|Clone|Merge|Extend|Clear|Remove|";
            if (selector == "At") return "DictGet";
            if (selector == "FirstKey" || selector == "FirstValue" || selector == "LastKey" ||
                selector == "LastValue" || selector == "FloorKeyOr" || selector == "CeilKeyOr")
                return "Tree" + std::string(selector);
            break;
        case IntrinsicFamily::String:
            prefix = "String";
            members = "|Contains|ContainsChar|StartsWith|EndsWith|IndexOf|LastIndexOf|IndexOfChar|LastIndexOfChar|First|Last|GetOr|Slice|SliceFrom|Take|Skip|Left|Right|Trim|TrimStart|TrimEnd|ToLower|ToUpper|Replace|ReplaceFirst|Repeat|Split|Lines|PadLeft|PadRight|Reversed|ToI8|ToI16|ToI32|ToI64|ToU8|ToU16|ToU32|ToU64|ToISize|ToUSize|ToF32|ToF64|ToBool|Append|Push|Insert|Erase|Clear|Reverse|ReplaceInPlace|TrimInPlace|ToLowerInPlace|ToUpperInPlace|";
            break;
        case IntrinsicFamily::Text:
            prefix = "Text";
            members = "|Count|ByteCount|Empty|Slice|ToString|Contains|StartsWith|EndsWith|GraphemeCount|SliceGraphemes|DisplayWidth|CaseFold|CodePoints|Graphemes|";
            if (selector == "Utf8") return "TextToString";
            if (selector == "Get" || selector == "At") return "Index";
            break;
        default: return std::nullopt;
        }
        if (family != IntrinsicFamily::Text && (selector == "Count" || selector == "Empty"))
            return std::string(selector);
        if ((family == IntrinsicFamily::Array || family == IntrinsicFamily::String) &&
            (selector == "Get" || selector == "At"))
            return "Index";
        if (members.find("|" + std::string(selector) + "|") == std::string_view::npos)
            return std::nullopt;
        return std::string(prefix) + std::string(selector);
    }
}
