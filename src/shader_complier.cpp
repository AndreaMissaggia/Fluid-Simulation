#include "shader_compiler.hpp"

void ShaderCompiler::compile(const std::filesystem::path& shader_source, const std::filesystem::path& output_directory)
{
    std::filesystem::path log_path { std::filesystem::temp_directory_path() / (shader_source.filename().string() + "_compile_log.temp") };

    std::string command
    {
        "glslangValidator -V " + shader_source.string() + " -o " + output_directory.string() + "/" + shader_source.filename().string() + ".spv"
        + " &> \"" + log_path.string() + "\""
    };

    int exit_code { std::system(command.c_str()) };
    std::ifstream file_stream { log_path };

    if (!file_stream)
        throw std::runtime_error("Unable to open shader compiler log file.");
    else
    {
        std::stringstream buffer {};
        buffer << file_stream.rdbuf();

        file_stream.close();
        std::filesystem::remove(log_path);

        if (exit_code <= -1)
            throw std::runtime_error("Unable to execute the glslangValidator command.");
        else if (exit_code == 0)
            LOG("Shader file \"" + shader_source.filename().string() + "\" complied to SPRI-V bytecode.", COMPONENT_NAME);
        else
            throw std::runtime_error("Compilation error of: " + shader_source.filename().string() + buffer.str());
    }
}

void ShaderCompiler::batchCompile(const std::filesystem::path& input_directory, const std::filesystem::path& output_directory)
{
    for (const auto& dir_entry : std::filesystem::directory_iterator { input_directory })
        if (dir_entry.path().extension() == ".comp")
            compile(dir_entry.path(), output_directory);
}
