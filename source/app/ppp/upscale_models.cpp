#include <ppp/upscale_models.hpp>

#include <array>
#include <ranges>

#include <QApplication>
#include <QDirIterator>
#include <QFile>

#include <opencv2/opencv.hpp>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <ppp/app.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/util/log.hpp>

struct DownloadableModel
{
    std::string_view m_Name;
    std::string_view m_Url;
};
inline constexpr std::array c_DownloadableModels{
    DownloadableModel{
        "4xESRGAN Anime - Fast",
        "https://github.com/Malacath-92/Proxy-PDF-Maker/raw/refs/heads/Upscaling/res/models/realesr_animevideov3.onnx",
    },
    DownloadableModel{
        "4xESRGAN Anime",
        "https://github.com/Malacath-92/Proxy-PDF-Maker/raw/refs/heads/Upscaling/res/models/realesrgan_x4plus_anime_6b.onnx",
    }
};

std::vector<std::string> GetModelNames()
{
    std::vector<std::string> model_names{ "None" };

    for (const auto& [model, url] : c_DownloadableModels)
    {
        model_names.push_back(std::string{ model });
    }

    Q_INIT_RESOURCE(resources);
    for (const auto models_dir : { ":/res/models", "./res/models" })
    {
        QDirIterator it(models_dir);
        while (it.hasNext())
        {
            const QFileInfo next{ it.nextFileInfo() };
            if (!next.isFile() || next.suffix().toLower() != "onnx")
            {
                continue;
            }

            std::string base_name{ next.baseName().toStdString() };
            if (std::ranges::contains(model_names, base_name))
            {
                continue;
            }

            model_names.push_back(std::move(base_name));
        }
    }

    return model_names;
}

std::string GetModelFilename(std::string_view model_name)
{
    return fmt::format("./res/models/{}.onnx", model_name);
}

bool ModelRequiresDownload(std::string_view model_name)
{
    return model_name != "None" && !fs::exists(GetModelFilename(model_name));
}

std::optional<std::string_view> GetModelUrl(std::string_view model_name)
{
    for (const auto& [model, url] : c_DownloadableModels)
    {
        if (model == model_name)
        {
            return url;
        }
    }

    return std::nullopt;
}

void LoadModel(PrintProxyPrepApplication& application, std::string_view model_name)
{
    if (application.GetUpscaleModel(std::string{ model_name }) != nullptr)
    {
        return;
    }

    static Ort::Env s_Env{ ORT_LOGGING_LEVEL_WARNING };
    // TODO: load providers??

    Ort::SessionOptions session_options;

    const auto available_providers{ Ort::GetAvailableProviders() };
    if (std::ranges::contains(available_providers, "CUDAExecutionProvider"))
    {
        OrtCUDAProviderOptions cuda_options;
        session_options.AppendExecutionProvider_CUDA(cuda_options);
    }

    auto session{
        std::make_unique<Ort::Session>(
            s_Env,
            fs::path{ GetModelFilename(model_name) }.c_str(),
            session_options),
    };

    application.SetUpscaleModel(std::string{ model_name }, std::move(session));
}

void UnloadModel(PrintProxyPrepApplication& application, std::string_view model_name)
{
    application.ReleaseUpscaleModel(std::string{ model_name });
}

Image RunModel(PrintProxyPrepApplication& application,
               std::string_view model_name,
               const Image& image,
               Size physical_size,
               PixelDensity max_density)
{
    auto* session{ application.GetUpscaleModel(std::string{ model_name }) };
    const Ort::TypeInfo input_type_info{ session->GetInputTypeInfo(0) };
    const auto input_tensor_info{ input_type_info.GetTensorTypeAndShapeInfo() };
    const auto input_shape{ input_tensor_info.GetShape() };

    if (input_shape[2] != input_shape[3])
    {
        LogError("Mixed input shape is not supported, model will not be run...");
        return image;
    }

    static constexpr auto c_PadToTwo{
        [](int tile_overlap)
        {
            return tile_overlap % 2 == 0 ? tile_overlap : tile_overlap + 1;
        }
    };
    static constexpr auto c_ComputePadding{
        [](const cv::Mat& image, int tile_size, int tile_overlap)
        {
            const int lap{ std::max(12, tile_overlap) };
            const int width{ image.cols - lap };
            const int height{ image.rows - lap };
            const int required_padding_width{ (tile_size - (width % tile_size)) % tile_size };
            const int required_padding_height{ (tile_size - (height % tile_size)) % tile_size };
            return dla::ivec2{ required_padding_width, required_padding_height };
        }
    };
    static constexpr auto c_PadImage{
        [](const cv::Mat& image, dla::ivec2 padding)
        {
            cv::Mat image_copy{ image };
            cv::copyMakeBorder(image_copy, image_copy, 0, padding.y, 0, padding.x, cv::BORDER_REFLECT_101);
            return image_copy;
        }
    };

    const int tile_size{ static_cast<int>(input_shape[2]) };
    const int full_size{ tile_size == -1 };
    const int tile_overlap{ c_PadToTwo(full_size ? 0 : tile_size / 4) };
    const int half_overlap{ tile_overlap / 2 };
    const int tile_delta{ tile_size - tile_overlap };

    const auto& underlying{ image.GetUnderlying() };
    const auto padding{ c_ComputePadding(underlying, full_size ? 16 : tile_delta, tile_overlap) };
    auto input_mat{ c_PadImage(underlying, padding) };

    // convert to float-rgb image
    cv::cvtColor(input_mat, input_mat, cv::COLOR_BGR2RGB);
    input_mat.convertTo(input_mat, CV_32F, 1.0 / 255.0);

    const auto run_model{
        [](Ort::Session* model, const cv::Mat& input) -> std::pair<cv::Mat, uint32_t>
        {
            const int input_height{ input.rows };
            const int input_width{ input.cols };

            // flatten all channels
            std::vector<cv::Mat> channels(3);
            cv::split(input, channels);
            for (cv::Mat& channel : channels)
            {
                channel = channel.reshape(1, 1);
            }

            // concatenate all channels
            cv::Mat flat_image;
            cv::hconcat(channels, flat_image);

            // create an allocator
            Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

            // put image into a tensor
            const std::vector<int64_t> input_shape{
                1,
                3,
                static_cast<int64_t>(input_height),
                static_cast<int64_t>(input_width),
            };
            const size_t input_size{ flat_image.total() };
            const Ort::Value input_tensor{
                Ort::Value::CreateTensor<float>(
                    memory_info,
                    (float*)flat_image.data,
                    input_size,
                    input_shape.data(),
                    input_shape.size()),
            };

            // get input/output names
            auto input_name{ model->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions{}) };
            auto output_name{ model->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions{}) };
            auto* input_name_ptr{ input_name.get() };
            auto* output_name_ptr{ output_name.get() };

            // run model
            Ort::Value output_tensor{ nullptr };
            model->Run(
                Ort::RunOptions{ nullptr },
                &input_name_ptr,
                &input_tensor,
                1,
                &output_name_ptr,
                &output_tensor,
                1);

            // collect info about the output
            const auto output_shape{ output_tensor.GetTensorTypeAndShapeInfo().GetShape() };
            const auto output_height{ static_cast<int>(output_shape[2]) };
            const auto output_width{ static_cast<int>(output_shape[3]) };
            const auto data_size_per_channel{ output_height * output_width };
            auto* output_data{ output_tensor.GetTensorMutableData<float>() };

            // convert to a cv::Mat
            cv::Mat output_mat;
            cv::merge(
                std::vector{
                    cv::Mat{
                        output_height,
                        output_width,
                        CV_32F,
                        output_data + 0 * data_size_per_channel,
                    },
                    cv::Mat{
                        output_height,
                        output_width,
                        CV_32F,
                        output_data + 1 * data_size_per_channel,
                    },
                    cv::Mat{
                        output_height,
                        output_width,
                        CV_32F,
                        output_data + 2 * data_size_per_channel,
                    },
                },
                output_mat);

            const auto upscale_factor{ static_cast<int>(output_width / input_width) };
            return { output_mat, upscale_factor };
        }
    };

    cv::Mat output_mat;
    int upscale_factor;
    if (full_size)
    {
        std::tie(output_mat, upscale_factor) = run_model(session, input_mat);
    }
    else
    {
        const Ort::TypeInfo output_type_info{ session->GetOutputTypeInfo(0) };
        const auto output_tensor_info{ output_type_info.GetTensorTypeAndShapeInfo() };
        const auto output_shape{ output_tensor_info.GetShape() };
        upscale_factor = dtatic_cast<int>(output_shape[2] / input_shape[2]);

        const int input_height{ input_mat.rows };
        const int input_width{ input_mat.cols };

        const int output_height{ static_cast<int>(input_height * upscale_factor) };
        const int output_width{ static_cast<int>(input_width * upscale_factor) };
        output_mat = cv::Mat::zeros(cv::Size{ output_width, output_height }, CV_32FC3);

        const dla::ivec2 tiles{
            static_cast<int>((input_width - tile_overlap) / tile_delta),
            static_cast<int>((input_height - tile_overlap) / tile_delta),
        };
        for (int x{ 0 }; x < tiles.x; ++x)
        {
            for (int y{ 0 }; y < tiles.y; ++y)
            {
                try
                {
                    const int pos_x{ x * tile_delta };
                    const int pos_y{ y * tile_delta };
                    const cv::Rect in_rect{ pos_x, pos_y, tile_size, tile_size };
                    const cv::Mat input_tile{ input_mat(in_rect) };
                    const auto [output_tile, _]{ run_model(session, input_tile) };

                    cv::Rect tile_rect{
                        half_overlap,
                        half_overlap,
                        tile_size - tile_overlap,
                        tile_size - tile_overlap,
                    };

                    if (x == 0)
                    {
                        tile_rect.x -= half_overlap;
                        tile_rect.width += half_overlap;
                    }
                    else if (x == tiles.x - 1)
                    {
                        tile_rect.width += half_overlap;
                    }

                    if (y == 0)
                    {
                        tile_rect.y -= half_overlap;
                        tile_rect.height += half_overlap;
                    }
                    else if (y == tiles.y - 1)
                    {
                        tile_rect.height += half_overlap;
                    }

                    const cv::Rect out_tile_rect{
                        tile_rect.x * upscale_factor,
                        tile_rect.y * upscale_factor,
                        tile_rect.width * upscale_factor,
                        tile_rect.height * upscale_factor,
                    };
                    const cv::Rect out_rect{
                        pos_x * upscale_factor + out_tile_rect.x,
                        pos_y * upscale_factor + out_tile_rect.y,
                        out_tile_rect.width,
                        out_tile_rect.height,
                    };
                    output_tile(out_tile_rect).copyTo(output_mat(out_rect));
                }
                catch (const std::exception& e)
                {
                    LogError("{}", e.what());
                }
            }
        }
    }

    // clamp values and convert to bgr-uchar image
    cv::threshold(output_mat, output_mat, 0, 0, cv::THRESH_TOZERO);
    cv::threshold(output_mat, output_mat, 1, 1, cv::THRESH_TRUNC);
    cv::cvtColor(output_mat, output_mat, cv::COLOR_RGB2BGR);
    output_mat.convertTo(output_mat, CV_8UC3, 255.0);

    // remove padding
    const auto final_width{ output_mat.cols - padding.x * upscale_factor };
    const auto final_height{ output_mat.rows - padding.y * upscale_factor };
    output_mat = output_mat(cv::Rect(0, 0, final_width, final_height));

    Image output_image{ output_mat };
    const auto density{ output_image.Density(physical_size) };
    if (density > max_density)
    {
        const PixelSize new_size{ dla::round(output_image.Size() * (max_density / density)) };
        output_image = output_image.Resize(new_size);
    }

    return output_image;
}
