#include <ppp/upscale_models.hpp>

#include <array>
#include <ranges>

#include <QApplication>
#include <QDirIterator>
#include <QFile>

#include <opencv2/opencv.hpp>

#include <magic_enum/magic_enum.hpp>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

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
        "ESRGAN x2",
        "https://github.com/BobJr23/MTGA_Swapper/raw/refs/tags/v2.2.1/modelesrgan2.onnx",
    },
    DownloadableModel{
        "CCSR x4",
        "https://github.com/BobJr23/MTGA_Swapper/raw/refs/tags/v2.2.1/modelscsr.onnx",
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
    for (const auto cubes_dir : { ":/res/models", "./res/models" })
    {
        QDirIterator it(cubes_dir);
        while (it.hasNext())
        {
            const QFileInfo next{ it.nextFileInfo() };
            if (!next.isFile() || next.suffix().toLower() != "cube")
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

    static Ort::Env env{ ORT_LOGGING_LEVEL_WARNING };
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
            env,
            fs::path{ GetModelFilename(model_name) }.c_str(),
            session_options),
    };

    Ort::AllocatorWithDefaultOptions allocator;
    auto name = session->GetInputNameAllocated(0, allocator);
    Ort::TypeInfo type_info = session->GetInputTypeInfo(0);
    auto tensor_info = type_info.GetTensorTypeAndShapeInfo();

    ONNXTensorElementDataType elem_type = tensor_info.GetElementType();
    LogError("Element type: {}", magic_enum::enum_name(elem_type));

    std::vector<int64_t> input_shape = tensor_info.GetShape();
    LogError("Input shape: ", input_shape);

    application.SetUpscaleModel(std::string{ model_name }, std::move(session));
}

void UnloadModel(PrintProxyPrepApplication& application, std::string_view model_name)
{
    application.ReleaseUpscaleModel(std::string{ model_name });
}

Image RunModel(PrintProxyPrepApplication& application,
               std::string_view model_name,
               const Image& image)
{
    auto* session{ application.GetUpscaleModel(std::string{ model_name }) };

    cv::Mat image_copy{ image.GetUnderlying().clone() };

    // pad to multiple of 16
    const int height{ image_copy.rows };
    const int width{ image_copy.cols };
    const int required_padding_width{ (16 - (width % 16)) % 16 };
    const int required_padding_height{ (16 - (height % 16)) % 16 };
    const int input_height{ height + required_padding_height };
    const int input_width{ width + required_padding_width };
    cv::copyMakeBorder(image_copy, image_copy, 0, required_padding_height, 0, required_padding_width, cv::BORDER_REFLECT_101);

    // convert to float-rgb image
    cv::cvtColor(image_copy, image_copy, cv::COLOR_BGR2RGB);
    image_copy.convertTo(image_copy, CV_32F, 1.0 / 255.0);

    // flatten all channels
    std::vector<cv::Mat> channels(3);
    cv::split(image_copy, channels);
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
    auto input_name{ session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions{}) };
    auto output_name{ session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions{}) };
    auto* input_name_ptr{ input_name.get() };
    auto* output_name_ptr{ output_name.get() };

    // run model
    Ort::Value output_tensor{ nullptr };
    session->Run(
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
    cv::Mat output_image;
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
        output_image);

    // clamp values and convert to bgr-uchar image
    cv::threshold(output_image, output_image, 0, 0, cv::THRESH_TOZERO);
    cv::threshold(output_image, output_image, 1, 1, cv::THRESH_TRUNC);
    cv::cvtColor(output_image, output_image, cv::COLOR_RGB2BGR);
    output_image.convertTo(output_image, CV_8UC3, 255.0);

    // remove padding
    const auto upscale_factor{ output_width / width };
    const auto final_width{ output_width - required_padding_width * upscale_factor };
    const auto final_height{ output_height - required_padding_height * upscale_factor };

    return Image{ output_image(cv::Rect(0, 0, final_width, final_height)) };
}
