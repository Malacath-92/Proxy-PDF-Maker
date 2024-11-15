#include <ppp/version.hpp>

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
