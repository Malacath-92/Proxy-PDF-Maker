#pragma once

#include <ppp/util.hpp>

class QPixmap;

class Project;
class Image;

float GetCardWidgetAspectRatio(const Project& project,
                               Rotation rotation,
                               Length bleed_edge);

QPixmap StoreIntoQtPixmap(const Image& img);
