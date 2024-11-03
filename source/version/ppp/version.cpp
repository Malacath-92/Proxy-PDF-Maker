#include <ppp/version.hpp>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

std::string_view proxy_pdf_version()
{
#ifdef PROXY_PDF_VERSION
    return TOSTRING(PROXY_PDF_VERSION);
#else
    return "<unkown version>";
#endif
}
