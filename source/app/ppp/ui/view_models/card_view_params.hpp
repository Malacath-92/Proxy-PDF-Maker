#pragma once

#include <ppp/util.hpp>

struct CardViewParams
{
    bool m_RoundedCorners{ true };
    bool m_Backside{ false };
    Rotation m_Rotation{ Rotation::None };
    Length m_BleedEdge{ 0_mm };
    Pixel m_MinimumWidth{ 0_pix };
};
