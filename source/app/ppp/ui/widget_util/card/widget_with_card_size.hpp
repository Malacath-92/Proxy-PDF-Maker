#pragma once

#include <ppp/util.hpp>

class Project;

template<class WidgetT>
class WidgetWithCardSize : public WidgetT
{
  public:
    WidgetWithCardSize(float aspect_ratio)
        : m_AspectRatio{ aspect_ratio }
    {
        QSizePolicy pm{ QSizePolicy::Preferred, QSizePolicy::Preferred };
        pm.setHeightForWidth(true);
        WidgetT::setSizePolicy(pm);
    }

    void RefreshSize(float aspect_ratio)
    {
        m_AspectRatio = aspect_ratio;
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override
    {
        return int(std::round(width / m_AspectRatio));
    }
    virtual QSize sizeHint() const override
    {
        const int default_width{ 200 };
        return QSize(default_width, heightForWidth(default_width));
    }
    virtual QSize minimumSizeHint() const override
    {
        const int minimum_width{ static_cast<const WidgetT*>(this)->minimumWidth() };
        return QSize(minimum_width, heightForWidth(minimum_width));
    }

    float GetAspectRatio() const
    {
        return m_AspectRatio;
    }

  protected:
    float m_AspectRatio;
};
