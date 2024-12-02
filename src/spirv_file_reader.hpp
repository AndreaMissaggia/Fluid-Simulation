#ifndef SPIRV_FILE_READER_HPP
#define SPIRV_FILE_READER_HPP

#include <filesystem>
#include <fstream>
#include <vector>

#include "spirv_data.hpp"

class SpirVFileReader {
public:
    static SpirVData readSpirVFile(const std::filesystem::path& spv_path);
    static std::vector<SpirVData> readSpirVFiles(const std::filesystem::path& spv_directory);
};

#endif // SPIRV_FILE_READER_HPP
