#include <ppp/ui/widget_card.hpp>

#include <QCommonStyle>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedLayout>

#include <ppp/constants.hpp>

#include <ppp/project/project.hpp>

CardImage::CardImage(const Image& image, Params params)
{
    Refresh(image, params);
}

void CardImage::Refresh(const Image& image, Params params)
{
    const QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
    QPixmap pixmap{ raw_pixmap };

    if (params.RoundedCorners)
    {
        const Length card_corner_radius_inch{ 1_in / 8 };
        const Pixel card_corner_radius_pixels{ card_corner_radius_inch * image.Size().x / CardSizeWithoutBleed.x };

        QPixmap clipped_pixmap{ int(image.Size().x.value), int(image.Size().x.value) };
        clipped_pixmap.fill(Qt::GlobalColor::transparent);

        QPainterPath path{};
        path.addRoundedRect(
            QRectF(pixmap.rect()),
            card_corner_radius_pixels.value,
            card_corner_radius_pixels.value);

        QPainter painter{ &clipped_pixmap };
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
        painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform, true);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, pixmap);
    }

    if (params.Rotation != Image::Rotation::None)
    {
        QTransform transform{};

        switch (params.Rotation)
        {
        case Image::Rotation::Degree90:
            transform.rotate(90);
            break;
        case Image::Rotation::Degree270:
            transform.rotate(-90);
            break;
        case Image::Rotation::Degree180:
            transform.rotate(180);
            break;
        default:
            break;
        }
        pixmap = pixmap.transformed(transform);
    }

    setPixmap(pixmap);
    setSizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding);
    setScaledContents(true);

    static constexpr Pixel card_size_minimum_width_pixels{ 130_pix };
    setMinimumWidth(card_size_minimum_width_pixels.value);

    Rotated = params.Rotation == Image::Rotation::Degree90 or params.Rotation == Image::Rotation::Degree270;
}

int CardImage::heightForWidth(int width) const
{
    if (Rotated)
    {
        return int(width * CardRatio);
    }
    else
    {
        return int(width / CardRatio);
    }
}

BacksideImage::BacksideImage(const fs::path& backside_name, const Project& project)
    : CardImage{
        project.Previews.contains(backside_name)
            ? project.Previews.at(backside_name).CroppedImage
            : project.Previews.at("fallback.png"_p).CroppedImage,
        CardImage::Params{}
    }
{
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    CardImage::Refresh(
        project.Previews.contains(backside_name)
            ? project.Previews.at(backside_name).CroppedImage
            : project.Previews.at("fallback.png"_p).CroppedImage,
        CardImage::Params{});
}

StackedCardBacksideView::StackedCardBacksideView(QWidget* image, QWidget* backside)
{
    QCommonStyle style{};

    auto* reset_button{ new QPushButton };
    reset_button->setIcon(
        style.standardIcon(QStyle::StandardPixmap::SP_DialogResetButton));
    reset_button->setToolTip("Reset Backside to Default");
    reset_button->setFixedWidth(20);
    reset_button->setFixedHeight(20);
    QObject::connect(reset_button,
                     &QPushButton::clicked,
                     this,
                     &StackedCardBacksideView::BacksideReset);

    backside->setToolTip("Choose individual Backside");

    auto* backside_layout{ new QHBoxLayout };
    backside_layout->addStretch();
    backside_layout->addWidget(reset_button, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->addWidget(backside, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->setContentsMargins(0, 0, 0, 0);

    auto* backside_container{ new QWidget{ this } };
    backside_container->setLayout(backside_layout);

    image->setMouseTracking(true);
    backside->setMouseTracking(true);
    backside_container->setMouseTracking(true);
    setMouseTracking(true);

    addWidget(image);
    addWidget(backside_container);

    auto* this_layout{ static_cast<QStackedLayout*>(layout()) };
    this_layout->setStackingMode(QStackedLayout::StackingMode::StackAll);
    this_layout->setAlignment(backside, Qt::AlignmentFlag::AlignBottom | Qt::AlignmentFlag::AlignRight);

    Image = image;
    Backside = backside;
    BacksideContainer = backside_container;
}

void StackedCardBacksideView::RefreshBackside(QWidget* new_backside)
{
    new_backside->setMouseTracking(true);

    auto* backside_layout{ static_cast<QHBoxLayout*>(BacksideContainer->layout()) };
    Backside->setParent(nullptr);
    backside_layout->addWidget(new_backside);
    backside_layout->addWidget(new_backside, 0, Qt::AlignmentFlag::AlignBottom);
    Backside = new_backside;

    RefreshSizes(rect().size());
}

void StackedCardBacksideView::RefreshSizes(QSize size)
{
    const auto width{ size.width() };
    const auto height{ size.height() };

    const auto img_width{ int(width * 0.9) };
    const auto img_height{ int(height * 0.9) };

    const auto backside_width{ int(width * 0.45) };
    const auto backside_height{ int(height * 0.45) };

    Image->setFixedWidth(img_width);
    Image->setFixedHeight(img_height);
    Backside->setFixedWidth(backside_width);
    Backside->setFixedHeight(backside_height);
}

void StackedCardBacksideView::resizeEvent(QResizeEvent* event)
{
    QStackedWidget::resizeEvent(event);
    RefreshSizes(event->size());
}

void StackedCardBacksideView::mouseMoveEvent(QMouseEvent* event)
{
    QStackedWidget::mouseMoveEvent(event);

    const auto x{ event->pos().x() };
    const auto y{ event->pos().y() };

    const auto neg_backside_width{ rect().width() - Backside->rect().size().width() };
    const auto neg_backside_height{ rect().height() - Backside->rect().size().height() };

    if (x >= neg_backside_width && y >= neg_backside_height)
    {
        setCurrentWidget(BacksideContainer);
    }
    else
    {
        setCurrentWidget(Image);
    }
}

void StackedCardBacksideView::leaveEvent(QEvent* event)
{
    QStackedWidget::leaveEvent(event);
    setCurrentWidget(Image);
}

void StackedCardBacksideView::mouseReleaseEvent(QMouseEvent* event)
{
    QStackedWidget::mouseReleaseEvent(event);

    if (currentWidget() == BacksideContainer)
    {
        BacksideClicked();
    }
}

class CardWidget : public QWidget
{
  public:
};

class DummyCardWidget : public CardWidget
{
  public:
};

// class CardWidget(QWidget):
//     def __init__(self, print_dict, img_dict, card_name):
//         super().__init__()
//
//         if card_name in img_dict:
//             img_data = eval(img_dict[card_name]["data"])
//             img_size = img_dict[card_name]["size"]
//         else:
//             img_data = fallback.data
//             img_size = fallback.size
//         img = CardImage(img_data, img_size)
//
//         backside_enabled = print_dict["backside_enabled"]
//         oversized_enabled = print_dict["oversized_enabled"]
//
//         backside_img = None
//         if backside_enabled:
//             backside_name = (
//                 print_dict["backsides"][card_name]
//                 if card_name in print_dict["backsides"]
//                 else print_dict["backside_default"]
//             )
//             backside_img = BacksideImage(backside_name, img_dict)
//
//         initial_number = print_dict["cards"][card_name] if card_name is not None else 1
//
//         number_edit = QLineEdit()
//         number_edit.setValidator(QIntValidator(0, 100, self))
//         number_edit.setText(str(initial_number))
//         number_edit.setFixedWidth(40)
//
//         decrement_button = QPushButton("-")
//         increment_button = QPushButton("+")
//
//         decrement_button.setToolTip("Remove one")
//         increment_button.setToolTip("Add one")
//
//         number_layout = QHBoxLayout()
//         number_layout.addStretch()
//         number_layout.addWidget(decrement_button)
//         number_layout.addWidget(number_edit)
//         number_layout.addWidget(increment_button)
//         number_layout.addStretch()
//         number_layout.setContentsMargins(0, 0, 0, 0)
//
//         number_area = QWidget()
//         number_area.setLayout(number_layout)
//         number_area.setFixedHeight(20)
//
//         if backside_img is not None:
//             card_widget = StackedCardBacksideView(img, backside_img)
//
//             def backside_reset():
//                 if card_name in print_dict["backsides"]:
//                     del print_dict["backsides"][card_name]
//                     new_backside_img = BacksideImage(
//                         print_dict["backside_default"], img_dict
//                     )
//                     card_widget.refresh_backside(new_backside_img)
//
//             def backside_choose():
//                 backside_choice = image_file_dialog(self, print_dict["image_dir"])
//                 if backside_choice is not None and (
//                     card_name not in print_dict["backsides"]
//                     or backside_choice != print_dict["backsides"][card_name]
//                 ):
//                     print_dict["backsides"][card_name] = backside_choice
//                     new_backside_img = BacksideImage(backside_choice, img_dict)
//                     card_widget.refresh_backside(new_backside_img)
//
//             card_widget._backside_reset.connect(backside_reset)
//             card_widget._backside_clicked.connect(backside_choose)
//         else:
//             card_widget = img
//
//         if backside_enabled or oversized_enabled:
//             extra_options = []
//
//             if backside_enabled:
//                 is_short_edge = (
//                     print_dict["backside_short_edge"][card_name]
//                     if card_name in print_dict["backside_short_edge"]
//                     else False
//                 )
//                 short_edge_checkbox = QCheckBox("Sideways")
//                 short_edge_checkbox.setChecked(is_short_edge)
//                 short_edge_checkbox.setToolTip(
//                     "Determines whether to flip backside on short edge"
//                 )
//
//                 short_edge_checkbox.checkStateChanged.connect(
//                     functools.partial(self.toggle_short_edge, print_dict)
//                 )
//
//                 extra_options.append(short_edge_checkbox)
//
//             if oversized_enabled:
//                 is_oversized = (
//                     print_dict["oversized"][card_name]
//                     if card_name in print_dict["oversized"]
//                     else False
//                 )
//                 oversized_checkbox = QCheckBox("Big")
//                 oversized_checkbox.setToolTip(
//                     "Determines whether this is an oversized card"
//                 )
//                 oversized_checkbox.setChecked(is_oversized)
//
//                 oversized_checkbox.checkStateChanged.connect(
//                     functools.partial(self.toggle_oversized, print_dict)
//                 )
//
//                 extra_options.append(oversized_checkbox)
//
//             extra_options_layout = QHBoxLayout()
//             extra_options_layout.addStretch()
//             for opt in extra_options:
//                 extra_options_layout.addWidget(opt)
//             extra_options_layout.addStretch()
//             extra_options_layout.setContentsMargins(0, 0, 0, 0)
//
//             extra_options_area = QWidget()
//             extra_options_area.setLayout(extra_options_layout)
//             extra_options_area.setFixedHeight(20)
//
//             self._extra_options_area = extra_options_area
//         else:
//             self._extra_options_area = None
//
//         layout = QVBoxLayout()
//         layout.addWidget(card_widget)
//         layout.addWidget(number_area)
//         if self._extra_options_area is not None:
//             layout.addWidget(extra_options_area)
//         self.setLayout(layout)
//
//         palette = self.palette()
//         palette.setColor(self.backgroundRole(), 0x111111)
//         self.setPalette(palette)
//         self.setAutoFillBackground(True)
//
//         self._img_widget = img
//         self._number_area = number_area
//
//         number_edit.editingFinished.connect(
//             functools.partial(self.edit_number, print_dict)
//         )
//         decrement_button.clicked.connect(functools.partial(self.dec_number, print_dict))
//         increment_button.clicked.connect(functools.partial(self.inc_number, print_dict))
//
//         margins = self.layout().contentsMargins()
//         minimum_img_width = img.minimumWidth()
//         minimum_width = minimum_img_width + margins.left() + margins.right()
//         self.setMinimumSize(minimum_width, self.heightForWidth(minimum_width))
//
//         self._number_edit = number_edit
//         self._card_name = card_name
//
//     def heightForWidth(self, width):
//         margins = self.layout().contentsMargins()
//         spacing = self.layout().spacing()
//
//         img_width = width - margins.left() - margins.right()
//         img_height = self._img_widget.heightForWidth(img_width)
//
//         additional_widgets = self._number_area.height() + spacing
//
//         if self._extra_options_area:
//             additional_widgets += self._extra_options_area.height() + spacing
//
//         return img_height + additional_widgets + margins.top() + margins.bottom()
//
//     def apply_number(self, print_dict, number):
//         self._number_edit.setText(str(number))
//         print_dict["cards"][self._card_name] = number
//
//     def edit_number(self, print_dict):
//         number = int(self._number_edit.text())
//         number = max(number, 0)
//         self.apply_number(print_dict, number)
//
//     def dec_number(self, print_dict):
//         number = print_dict["cards"][self._card_name] - 1
//         number = max(number, 0)
//         self.apply_number(print_dict, number)
//
//     def inc_number(self, print_dict):
//         number = print_dict["cards"][self._card_name] + 1
//         number = min(number, 999)
//         self.apply_number(print_dict, number)
//
//     def toggle_short_edge(self, print_dict, s):
//         short_edge_dict = print_dict["backside_short_edge"]
//         if s == QtCore.Qt.CheckState.Checked:
//             short_edge_dict[self._card_name] = True
//         elif self._card_name in short_edge_dict:
//             del short_edge_dict[self._card_name]
//
//     def toggle_oversized(self, print_dict, s):
//         oversized_dict = print_dict["oversized"]
//         if s == QtCore.Qt.CheckState.Checked:
//             oversized_dict[self._card_name] = True
//         elif self._card_name in oversized_dict:
//             del oversized_dict[self._card_name]

// class DummyCardWidget(CardWidget):
//     def __init__(self, print_dict, img_dict):
//         super().__init__(print_dict, img_dict, None)
//         self._card_name = "__dummy"
//
//     def apply_number(self, print_dict, number):
//         pass
//
//     def edit_number(self, print_dict):
//         pass
//
//     def dec_number(self, print_dict):
//         pass
//
//     def inc_number(self, print_dict):
//         pass
//
//     def toggle_oversized(self, print_dict, s):
//         pass
//
