#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <QPixmap>

#include <opencv2/opencv.hpp>

#include <ppp/image.hpp>
#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

float GetCardWidgetAspectRatio(const Project& project,
                               Rotation rotation,
                               Length bleed_edge)
{
    const auto card_size{ project.CardSize() + 2 * bleed_edge };
    const bool rotated{
        rotation == Rotation::Degree90 ||
        rotation == Rotation::Degree270
    };
    const auto aspect_ratio{ card_size.x / card_size.y };
    return rotated ? 1 / aspect_ratio
                   : aspect_ratio;
}

QPixmap StoreIntoQtPixmap(const Image& img)
{
    TRACY_AUTO_SCOPE();

    const auto& img_impl{ img.GetUnderlying() };
    switch (img_impl.channels())
    {
    case 1:
        return QPixmap::fromImage(QImage(img_impl.ptr(), img_impl.cols, img_impl.rows, img_impl.step, QImage::Format_Grayscale8));
    case 3:
        return QPixmap::fromImage(QImage(img_impl.ptr(), img_impl.cols, img_impl.rows, img_impl.step, QImage::Format_BGR888));
    case 4:
    {
        cv::Mat cvt_img;
        cv::cvtColor(img_impl, cvt_img, cv::COLOR_BGR2RGBA);
        return QPixmap::fromImage(QImage(cvt_img.ptr(), cvt_img.cols, cvt_img.rows, cvt_img.step, QImage::Format_RGBA8888));
    }
    default:
        return QPixmap{ img_impl.cols, img_impl.rows };
    }
}
