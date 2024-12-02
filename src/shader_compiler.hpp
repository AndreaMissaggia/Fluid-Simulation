#ifndef SHADER_COMPILER_HPP
#define SHADER_COMPLIER_HPP

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

#include "logger.hpp"

class ShaderCompiler {
public:
    static constexpr std::string COMPONENT_NAME { "SHADER COMPILER" };

    static void compile(const std::filesystem::path& shader_source, const std::filesystem::path& output_directory);
    static void batchCompile(const std::filesystem::path& input_directory, const std::filesystem::path& output_directory);
};

#endif // SHADER_COMPLIER_HPP
