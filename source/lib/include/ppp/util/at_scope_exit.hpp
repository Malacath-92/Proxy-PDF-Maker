#pragma once

#include <utility>

template<class FunT>
struct AtScopeExit
{
    AtScopeExit() = delete;
    AtScopeExit(const AtScopeExit&) = delete;
    AtScopeExit(AtScopeExit&&) = delete;
    AtScopeExit& operator=(const AtScopeExit&) = delete;
    AtScopeExit& operator=(AtScopeExit&&) = delete;

    AtScopeExit(FunT fun)
        : m_Dtor{ std::move(fun) }
    {}

    ~AtScopeExit()
    {
        m_Dtor();
    }

    FunT m_Dtor;
};
