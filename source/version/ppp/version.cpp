#include <ppp/version.hpp>

#include <charconv>
#include <ranges>
#include <vector>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

std::string_view ProxyPdfVersion()
{
#ifdef PROXY_PDF_VERSION
    return TOSTRING(PROXY_PDF_VERSION);
#else
    return "<unkown version>";
#endif
}

std::string_view ProxyPdfBuildTime()
{
#ifdef PROXY_PDF_NOW
    return TOSTRING(PROXY_PDF_NOW);
#else
    return "<unkown build time>";
#endif
}

SemanticVersion ProxyPdfToSemanticVersion(std::string_view version)
{
    SemanticVersion semver{ 0 };

    auto part_to_int{
        [](std::string_view part)
        {
            if (part.starts_with('v'))
            {
                part = part.substr(1);
            }
            else if (part.contains('-'))
            {
                part = part.substr(0, part.find('-'));
            }

            int32_t i;
            std::from_chars(part.data(), part.data() + part.size(), i);
            return i;
        }
    };

    const auto parts{
        version |
            std::views::split('.') |
            std::views::transform([](auto part)
                                  { return std::string_view{ part.begin(), part.end() }; }) |
            std::views::transform(part_to_int) |
            std::ranges::to<std::vector>(),
    };
    if (parts.size() > 0)
    {
        semver.m_Major = parts[0];
    }
    if (parts.size() > 1)
    {
        semver.m_Minor = parts[1];
    }
    if (parts.size() > 2)
    {
        semver.m_Bugfix = parts[2];
    }

    return semver;
}
