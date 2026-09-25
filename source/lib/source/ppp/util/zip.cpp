#include <ppp/util/zip.hpp>

#include <archive.h>
#include <archive_entry.h>

ZipWorkerBase::Conclusion ZipWorkerBase::GetConclusion() const
{
    return m_Conclusion;
}

bool ZipWorkerBase::HasError() const
{
    return !m_Error.empty();
}
const std::string& ZipWorkerBase::GetError() const
{
    return m_Error;
}

void ZipWorkerBase::Failed()
{
    m_Conclusion = Conclusion::Failed;
    Done();
}
void ZipWorkerBase::Failed(std::string error)
{
    m_Conclusion = Conclusion::Failed;
    m_Error = std::move(error);
    Done();
}
void ZipWorkerBase::Succeeded()
{
    m_Conclusion = Conclusion::Success;
    Done();
}

UnzipWorker::UnzipWorker(QByteArray archive_data,
                         fs::path output_folder)
    : m_ArchiveData{ std::move(archive_data) }
    , m_OutputFolder{ std::move(output_folder) }
{
}

void UnzipWorker::run()
{
    if (m_ArchiveData.isEmpty())
    {
        Failed();
        return;
    }

    if (!fs::exists(m_OutputFolder))
    {
        fs::create_directories(m_OutputFolder);
    }

    auto* reader{ archive_read_new() };
    AtScopeExit reader_deleter{ std::bind_front(archive_read_free, reader) };
    auto* writer{ archive_write_disk_new() };
    AtScopeExit writer_deleter{ std::bind_front(archive_write_free, writer) };

    if (reader == nullptr || writer == nullptr)
    {
        Failed();
        return;
    }

    // Enable formats (.zip and .tar) and filters (.gz compression)
    archive_read_support_format_tar(reader);
    archive_read_support_format_zip(reader);
    archive_read_support_filter_gzip(reader);

    static constexpr int c_Flags{
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL
    };
    archive_write_disk_set_options(writer, c_Flags);
    archive_write_disk_set_standard_lookup(writer);

    if (archive_read_open_memory(reader, m_ArchiveData.data(), m_ArchiveData.size()) != ARCHIVE_OK)
    {
        Failed();
        return;
    }

    archive_entry* entry;
    while (archive_read_next_header(reader, &entry) == ARCHIVE_OK)
    {
        const auto current_path{ archive_entry_pathname(entry) };
        const auto full_output_path{ m_OutputFolder / current_path };
        const auto full_output_path_str{ full_output_path.string() };
        archive_entry_set_pathname(entry, full_output_path_str.c_str());

        const auto write_header_res{ archive_write_header(writer, entry) };
        if (write_header_res < ARCHIVE_OK)
        {
            Failed("Failed writing file header while extracting file from archive.");
            return;
        }

        // Extract the file content by streaming data blocks from reader to writer
        const auto entry_size{ archive_entry_size(entry) };
        if (entry_size > 0)
        {
            const void* buff;
            size_t size;
            int64_t offset;

            while (true)
            {
                const auto read_block_res{
                    archive_read_data_block(reader, &buff, &size, &offset)
                };

                if (read_block_res == ARCHIVE_OK)
                {
                    const int64_t total_bytes_read{ archive_filter_bytes(reader, -1) };
                    const auto progress{ static_cast<float>(total_bytes_read) / m_ArchiveData.size() };
                    Progress(100.0f * progress);
                }

                if (read_block_res == ARCHIVE_EOF)
                {
                    break;
                }
                if (read_block_res < ARCHIVE_OK)
                {
                    Failed("Failed reading data from archive.");
                    return;
                }

                const auto write_block_res{
                    archive_write_data_block(writer, buff, size, offset)
                };
                if (write_block_res < ARCHIVE_OK)
                {
                    Failed("Failed writing data from archive to disk.");
                    break;
                }
            }
        }

        archive_write_finish_entry(writer);
    }

    Progress(100.0f);
    Succeeded();
}

ZipWorker::ZipWorker(std::vector<std::pair<fs::path, fs::path>> files,
                     fs::path output_file)
    : m_Files{ std::move(files) }
    , m_OutputFile{ std::move(output_file) }
{
}

void ZipWorker::run()
{
    auto writer{ archive_write_new() };
    AtScopeExit writer_deleter{ std::bind_front(archive_write_free, writer) };

    archive_write_set_format_pax_restricted(writer);
    archive_write_add_filter_gzip(writer);

    if (archive_write_open_filename(writer, m_OutputFile.string().c_str()) != ARCHIVE_OK)
    {
        Failed(fmt::format("Error opening file to write archive: {}", archive_error_string(writer)));
        return;
    }

    static char s_Buf[8192]{};

    size_t total_files{ m_Files.size() };
    size_t files_handled{ 0 };
    const auto write{
        [=, &total_files, &files_handled, this](const fs::path& on_disk, const fs::path& in_zip)
        {
            const auto write_impl{
                [=, &total_files, &files_handled, this](const fs::path& on_disk, const fs::path& in_zip, auto& write_impl) -> void
                {
                    if (fs::is_directory(on_disk))
                    {
                        total_files--;

                        for (const auto& child : fs::directory_iterator{ on_disk })
                        {
                            const auto child_path{ child.path() };
                            const auto child_name{ child_path.filename() };
                            total_files++;
                            write_impl(child_path, in_zip / child_name, write_impl);
                        }

                        return;
                    }

                    auto entry{ archive_entry_new() };
                    AtScopeExit entry_deleter{ std::bind_front(archive_entry_free, entry) };

                    archive_entry_set_pathname(entry, in_zip.generic_string().c_str());
                    archive_entry_set_size(entry, fs::file_size(on_disk));
                    archive_entry_set_filetype(entry, AE_IFREG);
                    archive_entry_set_perm(entry, 0644);

                    if (archive_write_header(writer, entry) != ARCHIVE_OK)
                    {
                        Failed(fmt::format("Header error for '{}': {}", in_zip.generic_string(), archive_error_string(writer)));
                        return;
                    }

                    if (std::ifstream file{ on_disk, std::ios::binary })
                    {
                        while (file.read(s_Buf, sizeof(s_Buf)) || file.gcount() > 0)
                        {
                            archive_write_data(writer, s_Buf, file.gcount());
                        }
                    }
                    else
                    {
                        Failed(fmt::format("Could not read file '{}' on disk.", on_disk.generic_string()));
                        return;
                    }

                    files_handled++;
                    Progress(100.0f * static_cast<float>(files_handled) / total_files);
                }
            };

            return write_impl(on_disk, in_zip, write_impl);
        }
    };

    for (const auto& [on_disk, in_zip] : m_Files)
    {
        write(on_disk, in_zip);
    }

    archive_write_close(writer);

    Progress(100.0f);
    Succeeded();
}
