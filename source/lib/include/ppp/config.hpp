#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <QObject>

#include <ppp/config_types.hpp>
#include <ppp/svg/util.hpp>
#include <ppp/units.hpp>
#include <ppp/util.hpp>

struct ConfigData
{
    bool m_AdvancedMode{ false };

    bool m_NoCropMode{ true };

    bool m_CheckVersionOnStartup{ true };
    uint32_t m_ToastTimeoutMS{ 8000 };

    bool m_EnableFancyUncrop{ true };

    Pixel m_BasePreviewWidth{ 512_pix };
    PixelDensity m_MaxDPI{ 1200_dpi };

    CardOrder m_CardOrder{ CardOrder::Alphabetical };
    CardOrderDirection m_CardOrderDirection{ CardOrderDirection::Ascending };

    uint32_t m_MaxWorkerThreads{ 16 };

    uint32_t m_DisplayColumns{ 5 };
    uint32_t m_MaxDisplayColumns{ 5 };

    std::optional<std::string> m_DefaultCardSize{};
    std::optional<std::string> m_DefaultPageSize{};

    std::string m_ColorCube{ "None" };

    fs::path m_FallbackName{ "fallback.png"_p };

    bool m_VersionOutput{ false };

    PdfBackend m_Backend{ PdfBackend::PoDoFo };
    ImageCompression m_PdfImageCompression{ ImageCompression::Lossy };
    std::optional<int> m_PngCompression{ std::nullopt };
    std::optional<int> m_JpgQuality{ std::nullopt };

    Unit m_BaseUnit{ Unit::Inches };

    bool m_DeterminsticPdfOutput{ false };

    std::unordered_map<std::string, bool> m_PluginsState{};

    // Hidden options, just doing someone a solid
    bool m_RenderZeroBleedRoundedEdges{ false };

    static inline constexpr std::string_view c_FitSize{ "Fit" };
    static inline constexpr std::string_view c_BasePDFSize{ "Base Pdf" };

    struct SizeInfo
    {
        Size m_Dimensions;
        Unit m_BaseUnit;
        uint32_t m_Decimals;
    };
    struct LengthInfo
    {
        Length m_Dimension;
        Unit m_BaseUnit;
        uint32_t m_Decimals;
    };

    struct CardSizeRoundedRectInfo
    {
        SizeInfo m_CardSize;
        LengthInfo m_CornerRadius;
    };

    struct CardSizeSvgInfo
    {
        std::string m_SvgName;
        Svg m_Svg;
    };

    struct CardSizeInfo
    {
        LengthInfo m_InputBleed;
        std::string m_Hint;
        float m_CardSizeScale;

        // Cards defined as a rounded rect
        std::optional<CardSizeRoundedRectInfo> m_RoundedRect;

        // Cards defined as an arbitrary shape
        std::optional<CardSizeSvgInfo> m_SvgInfo;
    };

    inline static const std::map<std::string, SizeInfo> g_DefaultPageSizes{
        { "Letter", { { 8.5_in, 11_in }, Unit::Inches, 1u } },
        { "Legal", { { 8.5_in, 14_in }, Unit::Inches, 1u } },
        { "Ledger", { { 11_in, 17_in }, Unit::Inches, 1u } },
        { "A5", { { 148.5_mm, 210_mm }, Unit::Millimeter, 1u } },
        { "A4", { { 210_mm, 297_mm }, Unit::Millimeter, 0u } },
        { "A4+", { { 240_mm, 329_mm }, Unit::Millimeter, 0u } },
        { "A3", { { 297_mm, 420_mm }, Unit::Millimeter, 0u } },
        { "A3+", { { 329_mm, 483_mm }, Unit::Millimeter, 0u } },
        { std::string{ c_FitSize }, {} },
        { std::string{ c_BasePDFSize }, {} },
    };
    std::map<std::string, SizeInfo> m_PageSizes{ g_DefaultPageSizes };

    inline static const std::map<std::string, CardSizeInfo> g_DefaultCardSizes{
        {
            "Standard",
            {
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_Hint{ ".e.g. Magic the Gathering, Pokemon, and other TCGs" },
                .m_CardSizeScale = 1.0f,

                .m_RoundedRect{ {
                    .m_CardSize{ { 2.48_in, 3.46_in }, Unit::Inches, 2u },
                    .m_CornerRadius{ 2.5_mm, Unit::Millimeter, 1u },
                } },

                .m_SvgInfo{ std::nullopt },
            },
        },
        {
            "Oversized",
            {
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_Hint{ ".e.g. oversized Magic the Gathering" },
                .m_CardSizeScale = 1.0f,

                .m_RoundedRect{ {
                    .m_CardSize{ { 3.46_in, 4.96_in }, Unit::Inches, 2u },
                    .m_CornerRadius{ 5_mm, Unit::Millimeter, 1u },
                } },

                .m_SvgInfo{ std::nullopt },
            },
        },
        {
            "Novelty",
            {
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_Hint{ ".e.g. novelty-sized Magic the Gathering" },
                .m_CardSizeScale = 0.5f,

                .m_RoundedRect{ {
                    .m_CardSize{ { 2.48_in, 3.46_in }, Unit::Inches, 2u },
                    .m_CornerRadius{ 2.5_mm, Unit::Millimeter, 1u },
                } },

                .m_SvgInfo{ std::nullopt },
            },
        },
        {
            "Japanese",
            {
                .m_InputBleed{ 3_mm, Unit::Millimeter, 0u },
                .m_Hint{ ".e.g. Yu-Gi-Oh!" },
                .m_CardSizeScale = 1.0f,

                .m_RoundedRect{ {
                    .m_CardSize{ { 59_mm, 86_mm }, Unit::Millimeter, 0u },
                    .m_CornerRadius{ 1_mm, Unit::Millimeter, 0u },
                } },

                .m_SvgInfo{ std::nullopt },
            },
        },
        {
            "Poker",
            {
                .m_InputBleed{ 3_mm, Unit::Millimeter, 0u },
                .m_Hint{},
                .m_CardSizeScale = 1.0f,

                .m_RoundedRect{ {
                    .m_CardSize{ { 2.5_in, 3.5_in }, Unit::Inches, 1u },
                    .m_CornerRadius{ 3_mm, Unit::Millimeter, 0u },
                } },

                .m_SvgInfo{ std::nullopt },
            },
        },
    };
    std::map<std::string, CardSizeInfo> m_CardSizes{ g_DefaultCardSizes };

    std::string_view GetFirstValidPageSize() const;
    const SizeInfo& GetFirstValidPageSizeInfo() const;

    std::string_view GetFirstValidCardSize() const;
    const CardSizeInfo& GetFirstValidCardSizeInfo() const;
};

class Config
    : public QObject,
      public ConfigData
{
    Q_OBJECT

  public:
    void Load();
    void Save() const;

    void SetAdvancedMode(bool advanced_mode);

    void SetNoCropMode(bool no_crop_mode);

    void SetCheckVersionOnStartup(bool check_version);
    void SetToastTimeoutMS(uint32_t toast_timeout_ms);

    void SetBasePreviewWidth(Pixel base_preview_width);
    void SetMaxDPI(PixelDensity max_dpi);

    void SetCardOrder(CardOrder card_order);
    void SetCardOrderDirection(CardOrderDirection card_order_direction);

    void SetMaxWorkerThreads(uint32_t max_worker_threads);

    void SetDisplayColumns(uint32_t display_columns);
    void SetMaxDisplayColumns(uint32_t max_display_columns);

    void SetColorCube(std::string_view color_cube);

    void SetVersionOutput(bool version_output);

    void SetPdfBackend(PdfBackend pdf_backend);
    void SetImageCompression(ImageCompression compression);
    void SetPngCompression(std::optional<int> png_compression);
    void SetJpgQuality(std::optional<int> jpg_quality);

    void SetBaseUnit(Unit base_unit);

    void EnablePlugin(std::string plugin_name);
    void DisablePlugin(std::string plugin_name);

    bool SvgCardSizeAdded(const fs::path& svg_path,
                          LengthInfo input_bleed = { 0.12_in, Unit::Inches, 2u });

    // TODO: Setters/Signals for add/remove card/page size

  signals:
    void AdvancedModeChanged(bool advanced_mode);

    void NoCropModeChanged(bool no_crop_mode);

    void CheckVersionOnStartupChanged(bool check_version);
    void ToastTimeoutMSChanged(uint32_t toast_timeout_ms);

    void BasePreviewWidthChanged(Pixel base_preview_width);
    void MaxDPIChanged(PixelDensity max_dpi);

    void CardOrderChanged(CardOrder card_order);
    void CardOrderDirectionChanged(CardOrderDirection card_order_direction);

    void MaxWorkerThreadsChanged(uint32_t max_worker_threads);

    void DisplayColumnsChanged(uint32_t display_columns);
    void MaxDisplayColumnsChanged(uint32_t max_display_columns);

    void ColorCubeChanged(std::string_view color_cube);

    void VersionOutputChanged(bool version_output);

    void PdfBackendChanged(PdfBackend pdf_backend);
    void ImageCompressionChanged(ImageCompression compression);
    void PngCompressionChanged(std::optional<int> png_compression);
    void JpgQualityChanged(std::optional<int> jpg_quality);

    void BaseUnitChanged(Unit base_unit);

    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);
};
