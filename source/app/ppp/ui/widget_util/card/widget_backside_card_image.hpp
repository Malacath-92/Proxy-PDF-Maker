#pragma once

#include <ppp/ui/widget_util/card/widget_card_image.hpp>

class BacksideImage : public CardImage
{
  public:
    BacksideImage(const fs::path& backside_name, const Project& project);
    BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project);

    void Refresh(const fs::path& backside_name, const Project& project);
    void Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project);
};
