#include <ppp/svg/generate.hpp>

#include <QPen>
#include <QRect>
#include <QSvgGenerator>

#include <libdxfrw/drw_interface.h>
#include <libdxfrw/libdxfrw.h>

#include <ppp/qt_util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

#include <ppp/svg/dxf_writer.hpp>

void DrawSvg(QPainter& painter, const QPainterPath& path)
{
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

    QPen pen{};
    pen.setWidth(1);
    pen.setColor(QColor{ 255, 0, 0 });
    painter.setPen(pen);
    painter.drawPath(path);
}

QPainterPath GenerateCardsPath(const Project& project)
{
    const auto svg_size{ project.ComputeCardsSize() / 1_mm };
    return GenerateCardsPath(dla::vec2{ 0.0f, 0.0f },
                             svg_size,
                             project);
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               const Project& project)
{
    return GenerateCardsPath(origin,
                             size,
                             project.m_Data.m_CardLayout,
                             project.CardSize(),
                             project.m_Data.m_BleedEdge,
                             project.m_Data.m_Spacing,
                             project.CardCornerRadius());
}

QPainterPath GenerateCardsPath(dla::vec2 origin,
                               dla::vec2 size,
                               dla::uvec2 grid,
                               Size card_size,
                               Length bleed_edge,
                               Length spacing,
                               Length corner_radius)
{
    const auto card_size_with_bleed{ card_size + 2 * bleed_edge };
    const auto physical_canvas_size{
        grid * card_size_with_bleed + (grid - 1) * spacing
    };
    const auto pixel_ratio{ size / physical_canvas_size };
    const auto card_size_with_bleed_pixels{ card_size_with_bleed * pixel_ratio };
    const auto corner_radius_pixels{ corner_radius * pixel_ratio };
    const auto bleed_pixels{ bleed_edge * pixel_ratio };
    const auto spacing_pixels{ spacing * pixel_ratio };
    const auto card_size_pixels{ card_size_with_bleed_pixels - 2 * bleed_pixels };

    QPainterPath card_border;
    if (bleed_edge > 0_mm)
    {
        const QRectF rect{
            origin.x,
            origin.y,
            size.x,
            size.y,
        };
        card_border.addRect(rect);
    }

    const auto& [columns, rows]{ grid.pod() };
    for (uint32_t x = 0; x < columns; x++)
    {
        for (uint32_t y = 0; y < rows; y++)
        {
            const dla::uvec2 idx{ x, y };
            const auto top_left_corner{
                origin + idx * (card_size_with_bleed_pixels + spacing_pixels) + bleed_pixels
            };
            const QRectF rect{
                top_left_corner.x,
                top_left_corner.y,
                card_size_pixels.x,
                card_size_pixels.y,
            };
            card_border.addRoundedRect(rect, corner_radius_pixels.x, corner_radius_pixels.y);
        }
    }

    return card_border;
}

void GenerateCardsSvg(const Project& project)
{
    const auto svg_path{ fs::path{ project.m_Data.m_FileName }.replace_extension(".svg") };

    LogInfo("Generating card path...");
    const QPainterPath path{ GenerateCardsPath(project) };

    QSvgGenerator generator{};
    generator.setFileName(ToQString(svg_path));
    generator.setTitle("Card cutting guides.");
    generator.setDescription("An SVG containing exact cutting guides for the accompanying sheet.");

    LogInfo("Drawing card path...");
    QPainter painter;
    painter.begin(&generator);
    DrawSvg(painter, path);
    painter.end();
}

class PolyonWriter : public DRW_Interface
{
  public:
    void ReadPath(const char* dxf_path)
    {
        dxfRW reader{ dxf_path };
        m_Writer = &reader;
        reader.read(this, false);
    }

    void WritePath(const fs::path& dxf_path, const QPainterPath& path)
    {
        static constexpr auto c_SamplesPerCurve{ 20 };
        for (int i = 0; i < path.elementCount(); ++i)
        {
            const auto& el{ path.elementAt(i) };

            if (el.isMoveTo())
            {
                continue;
            }
            else if (el.isCurveTo())
            {
                QPointF p0 = path.elementAt(i - 1);
                QPointF p1 = QPointF(el.x, el.y);
                QPointF p2 = QPointF(path.elementAt(i + 1).x, path.elementAt(i + 1).y);
                QPointF p3 = QPointF(path.elementAt(i + 2).x, path.elementAt(i + 2).y);
                i += 2;

                std::vector<std::shared_ptr<DRW_Coord>> fitlist;
                for (int j = 0; j <= c_SamplesPerCurve; ++j)
                {
                    qreal t = qreal(j) / c_SamplesPerCurve;
                    qreal it = 1 - t;
                    QPointF pt = it * it * it * p0 + 3 * it * it * t * p1 + 3 * it * t * t * p2 + t * t * t * p3;
                    fitlist.emplace_back(new DRW_Coord(pt.x(), pt.y(), 0));
                }

                DRW_Spline& spline{ m_Splines.emplace_back() };
                spline.normalVec.z = 1;
                spline.degree = 3;
                spline.nfit = static_cast<dint32>(fitlist.size());
                spline.fitlist = std::move(fitlist);
                spline.flags = 8;
            }
            else if (el.isLineTo())
            {
                QPointF p0 = path.elementAt(i - 1);
                QPointF p1 = QPointF(el.x, el.y);

                DRW_Line& line{ m_Lines.emplace_back() };
                line.basePoint = DRW_Coord(p0.x(), p0.y(), 0);
                line.secPoint = DRW_Coord(p1.x(), p1.y(), 0);
            }
        }

        dxfRW writer{ dxf_path.string().c_str() };
        m_Writer = &writer;

        DRW::Version version{ DRW::Version::AC1032 };
        writer.write(this, version, false);
    }

    virtual void writeHeader(DRW_Header& /*header*/) override
    {
    }
    virtual void writeEntities() override
    {
        for (auto& poly_line : m_PolyLines)
        {
            m_Writer->writePolyline(&poly_line);
        }
        for (auto& spline : m_Splines)
        {
            m_Writer->writeSpline(&spline);
        }
        for (auto& line : m_Lines)
        {
            m_Writer->writeLine(&line);
        }
    }

  private:
#pragma region(Unimplemented)
    // clang-format off
    virtual void addHeader(const DRW_Header* /*data*/) override {}

    virtual void addLType(const DRW_LType& /*data*/) override {}
    virtual void addLayer(const DRW_Layer& /*data*/) override {}
    virtual void addDimStyle(const DRW_Dimstyle& /*data*/) override {}
    virtual void addVport(const DRW_Vport& /*data*/) override {}
    virtual void addTextStyle(const DRW_Textstyle& /*data*/) override {}
    virtual void addAppId(const DRW_AppId& /*data*/) override {}

    virtual void addBlock(const DRW_Block& /*data*/) override {}
    virtual void endBlock() override {}

    virtual void setBlock(const int /*handle*/) override {}

    virtual void addPoint(const DRW_Point& /*data*/) override {}
    virtual void addLine(const DRW_Line& /*data*/) override {}
    virtual void addRay(const DRW_Ray& /*data*/) override {}
    virtual void addXline(const DRW_Xline& /*data*/) override {}
    virtual void addArc(const DRW_Arc& /*data*/) override {}
    virtual void addCircle(const DRW_Circle& /*data*/) override {}
    virtual void addEllipse(const DRW_Ellipse& /*data*/) override {}
    virtual void addLWPolyline(const DRW_LWPolyline& /*data*/) override {}
    virtual void addPolyline(const DRW_Polyline& /*data*/) override {}
    virtual void addSpline(const DRW_Spline* data) override {
        __debugbreak();
        (void)data;
    }
    virtual void addKnot(const DRW_Entity& /*data*/) override {}

    virtual void addInsert(const DRW_Insert& /*data*/) override {}
    virtual void addTrace(const DRW_Trace& /*data*/) override {}
    virtual void add3dFace(const DRW_3Dface& /*data*/) override {}
    virtual void addSolid(const DRW_Solid& /*data*/) override {}
    virtual void addMText(const DRW_MText& /*data*/) override {}
    virtual void addText(const DRW_Text& /*data*/) override {}
    virtual void addDimAlign(const DRW_DimAligned* /*data*/) override {}
    virtual void addDimLinear(const DRW_DimLinear* /*data*/) override {}
    virtual void addDimRadial(const DRW_DimRadial* /*data*/) override {}
    virtual void addDimDiametric(const DRW_DimDiametric* /*data*/) override {}
    virtual void addDimAngular(const DRW_DimAngular* /*data*/) override {}
    virtual void addDimAngular3P(const DRW_DimAngular3p* /*data*/) override {}
    virtual void addDimOrdinate(const DRW_DimOrdinate* /*data*/) override {}
    virtual void addLeader(const DRW_Leader* /*data*/) override {}
    virtual void addHatch(const DRW_Hatch* /*data*/) override {}
    virtual void addViewport(const DRW_Viewport& /*data*/) override {}
    virtual void addImage(const DRW_Image* /*data*/) override {}

    virtual void linkImage(const DRW_ImageDef* /*data*/) override {}
    virtual void addComment(const char* /*comment*/) override {}
    virtual void addPlotSettings(const DRW_PlotSettings */*data*/) override {}

    virtual void writeBlocks() override {}
    virtual void writeBlockRecords() override {}
    virtual void writeLTypes() override {}
    virtual void writeLayers() override {}
    virtual void writeTextstyles() override {}
    virtual void writeVports() override {}
    virtual void writeDimstyles() override {}
    virtual void writeObjects() override {}
    virtual void writeAppId() override {}
    // clang-format on
#pragma endregion

    dxfRW* m_Writer{ nullptr };
    std::vector<DRW_Polyline> m_PolyLines;
    std::vector<DRW_Line> m_Lines;
    std::vector<DRW_Spline> m_Splines;
};

void GenerateCardsDxf(const Project& project)
{
    const auto dxf_path{ fs::path{ project.m_Data.m_FileName }.replace_extension(".dxf") };

    LogInfo("Generating card path...");
    const QPainterPath path{ GenerateCardsPath(project) };

    PolyonWriter polygon_writer{};
    polygon_writer.ReadPath("dynaprint_sample_working.dxf");
    polygon_writer.WritePath(dxf_path, path);
}
