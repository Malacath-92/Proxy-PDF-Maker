#pragma once

#include <type_traits>

#define FORWARD_SIGNAL_FROM_TO(from, to, sig)              \
    do                                                     \
    {                                                      \
        using FromT = std::remove_cvref_t<decltype(from)>; \
        using ToT = std::remove_cvref_t<decltype(to)>;     \
        QObject::connect(&(from),                          \
                         &FromT::sig,                      \
                         &(to),                            \
                         &ToT::sig);                       \
    } while (false)

#define FORWARD_SIGNAL_FROM(from, sig) \
    FORWARD_SIGNAL_FROM_TO(from, *this, sig)

#define FORWARD_SIGNAL_FROM_VIEW_MODEL(sig) \
    FORWARD_SIGNAL_FROM(m_ViewModel, sig)
