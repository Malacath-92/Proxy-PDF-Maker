#pragma once

#include <QPainter>
#include <QPainterPath>

#include <ppp/util.hpp>

class Project;

void DrawSvg(QPainter& painter, const QPainterPath& path, PrintFn print_fn);

QPainterPath GenerateCardsPath(const Project& project, PrintFn print_fn);
QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               dla::uvec2 grid,
                               Size card_size,
                               Length bleed_edge,
                               Length corner_radius,
                               PrintFn print_fn);

void GenerateCardsSvg(const Project& project, PrintFn print_fn);
