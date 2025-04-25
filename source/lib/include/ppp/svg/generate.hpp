#pragma once

#include <QPainter>
#include <QPainterPath>

#include <ppp/util.hpp>

class Project;

void DrawSvg(QPainter& painter, const QPainterPath& path);

QPainterPath GenerateCardsPath(const Project& project);
QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               dla::uvec2 grid,
                               Size card_size,
                               Length bleed_edge,
                               Length corner_radius);

void GenerateCardsSvg(const Project& project);
