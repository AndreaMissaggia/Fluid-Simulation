#include "engine.hpp"
#include "logger.hpp"
#include "shader_compiler.hpp"

int main(int argc, char* argv[])
{
    const std::filesystem::path shaders_directory_path = std::filesystem::current_path() / "shaders";
    const std::filesystem::path spv_directory_path     = std::filesystem::current_path() / "shaders" / "spv";
    const std::filesystem::path header_file_path       = std::filesystem::current_path() / "strings" / "header.txt";

    try
    {
        ShaderCompiler::batchCompile(shaders_directory_path, spv_directory_path);
    }
    catch (const std::runtime_error& e)
    {
        LOG("Shaders compliation failed.", "MAIN", LogLevel::ERROR);

        std::cerr << std::endl << "======================== COMPILER OUTPUT ========================" << std::endl;
        std::cerr << std::endl << e.what() << std::endl;
        std::cerr << std::endl << "=================================================================" << std::endl;
    }

    std::ifstream file_stream { header_file_path.string() };

    if (file_stream)
    {
        std::string line;
        while (std::getline(file_stream, line))
            std::cout << line << std::endl;

        file_stream.close();
    }

    Engine engine {};

    engine.init();
    engine.run();
    engine.cleanup();

    return 0;
}

//std::cerr << "Shaders compliation failed: " << e.what() << std::endl;

