#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <dla/scalar_math.h>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/backend.hpp>
#include <ppp/pdf/util.hpp>

fs::path GeneratePdf(const Project& project)
{
    using CrossSegment = PdfPage::CrossSegment;

    const auto output_dir{ GetOutputDir(project.m_Data.m_CropDir, project.m_Data.m_BleedEdge, g_Cfg.m_ColorCube) };

    ColorRGB32f guides_color_a{
        static_cast<float>(project.m_Data.m_GuidesColorA.r) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorA.g) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorA.b) / 255.0f,
    };
    ColorRGB32f guides_color_b{
        static_cast<float>(project.m_Data.m_GuidesColorB.r) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorB.g) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorB.b) / 255.0f,
    };

    PdfPage::DashedLineStyle line_style{
        {
            project.m_Data.m_GuidesThickness,
            guides_color_a,
        },
        guides_color_b,
    };

    const auto page_size{ project.ComputePageSize() };
    const auto [page_width, page_height]{ page_size.pod() };

    const auto bleed{ project.m_Data.m_BleedEdge };
    const auto corner_guides_offset{ bleed - project.m_Data.m_GuidesOffset };

    const auto pages{ DistributeCardsToPages(project) };
    const auto transforms{ ComputeTransforms(project) };

    const auto backside_pages{
        project.m_Data.m_BacksideEnabled ? MakeBacksidePages(project, pages)
                                         : std::vector<Page>{}
    };
    const auto backside_transforms{
        project.m_Data.m_BacksideEnabled ? ComputeBacksideTransforms(project, transforms)
                                         : PageImageTransforms{}
    };

    std::vector<PdfPage::LineData> extended_guides;
    std::vector<PdfPage::LineData> backside_extended_guides;
    if (project.m_Data.m_ExtendedGuides)
    {
        static constexpr auto g_Precision{ 0.1_pts };

        const auto generate_extended_guides{
            [page_width, page_height, bleed](const auto& transforms)
            {
                std::vector<PdfPage::LineData> guides;
                if (transforms.empty())
                {
                    return guides;
                }

                std::vector<int32_t> unique_x;
                std::vector<int32_t> unique_y;
                for (const auto& transform : transforms)
                {
                    const auto top_left_corner{
                        static_cast<dla::ivec2>((transform.m_Position + bleed) / g_Precision)
                    };
                    if (!std::ranges::contains(unique_x, top_left_corner.x))
                    {
                        unique_x.push_back(top_left_corner.x);
                    }
                    if (!std::ranges::contains(unique_y, top_left_corner.y))
                    {
                        unique_y.push_back(top_left_corner.y);
                    }

                    const auto card_size{
                        static_cast<dla::ivec2>((transform.m_Size - bleed * 2) / g_Precision)
                    };
                    const auto bottom_right_corner{ top_left_corner + card_size };
                    if (!std::ranges::contains(unique_x, bottom_right_corner.x))
                    {
                        unique_x.push_back(bottom_right_corner.x);
                    }
                    if (!std::ranges::contains(unique_y, bottom_right_corner.y))
                    {
                        unique_y.push_back(bottom_right_corner.y);
                    }
                }

                const auto extended_offset{ bleed + 1_mm };
                const auto x_min{ std::ranges::min(unique_x) * g_Precision - extended_offset };
                const auto x_max{ std::ranges::max(unique_x) * g_Precision + extended_offset };
                const auto y_min{ std::ranges::min(unique_y) * g_Precision - extended_offset };
                const auto y_max{ std::ranges::max(unique_y) * g_Precision + extended_offset };

                for (const auto& x : unique_x)
                {
                    const auto real_x{ x * g_Precision };
                    guides.push_back(PdfPage::LineData{
                        .m_From{ real_x, y_min },
                        .m_To{ real_x, 0_mm },
                    });
                    guides.push_back(PdfPage::LineData{
                        .m_From{ real_x, y_max },
                        .m_To{ real_x, page_height },
                    });
                }

                for (const auto& y : unique_y)
                {
                    const auto real_y{ y * g_Precision };
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x_min, real_y },
                        .m_To{ 0_mm, real_y },
                    });
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x_max, real_y },
                        .m_To{ page_width, real_y },
                    });
                }

                return guides;
            }
        };

        extended_guides = generate_extended_guides(transforms);
        backside_extended_guides = generate_extended_guides(backside_transforms);
    }

    auto pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

#if __cpp_lib_ranges_enumerate
    for (auto [p, page] : pages | std::views::enumerate)
    {
#else
    for (size_t p = 0; p < pages.size(); p++)
    {
        const Page& page{ pages[p] };
#endif

        const auto draw_image{
            [&](PdfPage* page,
                const PageImage& image,
                const PageImageTransform& transform)
            {
                const auto img_path{ output_dir / image.m_Image };
                if (fs::exists(img_path))
                {
                    PdfPage::ImageData image_data{
                        .m_Path{ img_path },
                        .m_Pos{
                            transform.m_Position.x,
                            page_height - transform.m_Position.y - transform.m_Size.y,
                        },
                        .m_Size{ transform.m_Size },
                        .m_Rotation = transform.m_Rotation,
                    };
                    page->DrawImage(image_data);
                }
            }
        };

        const auto draw_corner_guides{
            [&](PdfPage* page, const PageImageTransform& transform)
            {
                const auto draw_cross_at_grid{
                    [&](PdfPage* page, Position pos, CrossSegment s)
                    {
                        PdfPage::CrossData cross{
                            .m_Pos{ pos },
                            .m_Length{ project.m_Data.m_GuidesLength },
                            .m_Segment = project.m_Data.m_CrossGuides ? CrossSegment::FullCross : s,
                        };
                        page->DrawDashedCross(cross, line_style);
                    }
                };

                const Position position{
                    transform.m_Position.x,
                    page_height - transform.m_Position.y - transform.m_Size.y,
                };
                const auto bottom_left{ position + corner_guides_offset };
                const auto top_right{ position + transform.m_Size - corner_guides_offset };

                draw_cross_at_grid(page,
                                   Position{
                                       bottom_left.x,
                                       top_right.y,
                                   },
                                   CrossSegment::TopLeft);
                draw_cross_at_grid(page,
                                   bottom_left,
                                   CrossSegment::BottomLeft);
                draw_cross_at_grid(page,
                                   Position{
                                       top_right.x,
                                       bottom_left.y,
                                   },
                                   CrossSegment::BottomRight);
                draw_cross_at_grid(page,
                                   top_right,
                                   CrossSegment::TopRight);
            }
        };

        {
            static constexpr const char c_RenderFmt[]{
                "Rendering page {}...\nImage number {} - {}"
            };

            PdfPage* front_page{ pdf->NextPage() };

            for (size_t i = 0; i < page.m_Images.size(); ++i)
            {
                const auto& card{ page.m_Images[i] };
                const auto& transform{ transforms[i] };

                LogInfo(c_RenderFmt, p + 1, i + 1, card.m_Image.get().string());
                draw_image(front_page, card, transform);
            }

            if (project.m_Data.m_EnableGuides)
            {
                if (project.m_Data.m_CornerGuides)
                {
                    for (const auto& transform : transforms)
                    {
                        draw_corner_guides(front_page, transform);
                    }
                }

                if (project.m_Data.m_ExtendedGuides)
                {
                    for (const auto& guide : extended_guides)
                    {
                        front_page->DrawDashedLine(guide, line_style);
                    }
                }
            }

            front_page->Finish();
        }

        if (project.m_Data.m_BacksideEnabled)
        {
            static constexpr const char c_RenderFmt[]{
                "Rendering backside for page {}...\nImage number {} - {}"
            };

            PdfPage* back_page{ pdf->NextPage() };

            const auto& backside_page{ backside_pages[p] };
            for (size_t i = 0; i < backside_page.m_Images.size(); ++i)
            {
                const auto& card{ backside_page.m_Images[i] };
                const auto& transform{ backside_transforms[i] };

                LogInfo(c_RenderFmt, p + 1, i + 1, card.m_Image.get().string());
                draw_image(back_page, card, transform);
            }

            if (project.m_Data.m_EnableGuides && project.m_Data.m_BacksideEnableGuides)
            {
                if (project.m_Data.m_CornerGuides)
                {
                    for (const auto& transform : backside_transforms)
                    {
                        draw_corner_guides(back_page, transform);
                    }
                }

                if (project.m_Data.m_ExtendedGuides)
                {
                    for (const auto& guide : backside_extended_guides)
                    {
                        back_page->DrawDashedLine(guide, line_style);
                    }
                }
            }

            back_page->Finish();
        }
    }

    const auto pdf_path{ pdf->Write(project.m_Data.m_FileName) };

    return pdf_path;
}

fs::path GenerateTestPdf(const Project& project)
{
    const auto page_size{ project.ComputePageSize() };
    const auto [page_width, page_height]{ page_size.pod() };

    const auto page_half{ page_size / 2 };
    const auto page_fourth{ page_size / 4 };
    const auto page_eighth{ page_size / 8 };
    const auto page_sixteenth{ page_size / 16 };

    auto pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

    PdfPage::LineStyle line_style{
        .m_Thickness = 0.2_mm,
        .m_Color = ColorRGB32f{},
    };

    {
        auto* front_page{ pdf->NextPage() };

        {
            const Size text_top_left{ 0_mm, page_height - page_sixteenth.y };
            const Size text_bottom_right{ page_width, page_height - page_eighth.y };
            front_page->DrawText("This is a test page, follow instructions to verify your settings will work fine for proxies.",
                                 { text_top_left, text_bottom_right });
        }

        {
            const auto left_line_x{ page_fourth.x };
            const PdfPage::LineData left_line{
                .m_From{ left_line_x, 0_mm },
                .m_To{ left_line_x, page_height - page_eighth.y },
            };
            front_page->DrawSolidLine(left_line, line_style);

            if (project.m_Data.m_BacksideEnabled)
            {
                const Size backside_text_top_left{ left_line_x, page_height - page_eighth.y };
                const Size backside_text_bottom_right{ page_width, page_half.y };
                front_page->DrawText("Shine a light through this page, the line on the back should align with the front. "
                                     "If not, measure the difference and paste it into the backside offset option.",
                                     { backside_text_top_left, backside_text_bottom_right });
            }

            const auto right_line_x{ page_fourth.x + 20_mm };
            const PdfPage::LineData right_line{
                .m_From{ right_line_x, 0_mm },
                .m_To{ right_line_x, page_half.y },
            };
            front_page->DrawSolidLine(right_line, line_style);

            const Size text_top_left{ right_line_x, page_fourth.y };
            const Size text_bottom_right{ page_width, 0_mm };
            front_page->DrawText("These lines should be exactly 20mm apart. If not, make sure to print at 100% scaling.",
                                 { text_top_left, text_bottom_right });
        }
    }

    if (project.m_Data.m_BacksideEnabled)
    {
        auto* back_page{ pdf->NextPage() };

        const auto backside_left_line_x{ page_width - page_fourth.x + project.m_Data.m_BacksideOffset };
        const PdfPage::LineData line{
            .m_From{ backside_left_line_x, 0_mm },
            .m_To{ backside_left_line_x, page_height },
        };
        back_page->DrawSolidLine(line, line_style);
    }

    return pdf->Write("alignment.pdf");
}
