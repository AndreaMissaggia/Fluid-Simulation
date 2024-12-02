#ifndef SPIRV_HPP
#define SPIRV_HPP

#include <cstdint>
#include <string>
#include <vector>

struct SpirVData
{
    std::string name      {};
    std::string file_path {};

    std::vector<uint32_t> spv_code {};
};

#endif // SPIRV_HPP
