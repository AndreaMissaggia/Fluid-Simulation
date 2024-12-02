#include "spirv_file_reader.hpp"

SpirVData SpirVFileReader::readSpirVFile(const std::filesystem::path& spv_path)
{
    std::ifstream file_stream { spv_path.string(), std::ios::binary | std::ios::ate };

    if (!file_stream.is_open())
        throw std::runtime_error("Could not open shader file: " + spv_path.string());

    size_t file_size { (size_t)file_stream.tellg() };
    std::vector<uint32_t> uint32_buffer( file_size / sizeof(uint32_t) );

    file_stream.seekg(0, std::ios::beg);

    if (!file_stream.read( (char*)uint32_buffer.data(), file_size) )
        throw std::runtime_error("Failed to read shader file: " + spv_path.string());

    file_stream.close();

    return SpirVData { .name = spv_path.filename().string(), .file_path = spv_path.string(), .spv_code = uint32_buffer };
}

std::vector<SpirVData> SpirVFileReader::readSpirVFiles(const std::filesystem::path& spv_directory)
{
    std::vector<SpirVData> output {};

    for (const auto& dir_entry : std::filesystem::directory_iterator { spv_directory })
        if (dir_entry.path().extension() == ".spv")
            output.push_back(readSpirVFile(dir_entry.path()));

    return output;
}
