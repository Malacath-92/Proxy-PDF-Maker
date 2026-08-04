#pragma once

#include <QPainter>
#include <QPainterPath>

#include <ppp/util.hpp>

class Project;

void DrawSvg(QPainter& painter, const QPainterPath& path, QColor color = QColor{ 255, 0, 0 });

void GenerateCardsSvg(const Project& project, bool no_crop_mode);
void GenerateCardsDxf(const Project& project, bool no_crop_mode);
