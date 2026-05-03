#include "kera/utilities/file_utils.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace kera 
{
    bool FileUtils::fileExists(const std::string& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    }

    bool FileUtils::directoryExists(const std::string& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }

    std::string FileUtils::readTextFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << path << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<uint8_t> FileUtils::readBinaryFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << path << std::endl;
            return {};
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            std::cerr << "Failed to read file: " << path << std::endl;
            return {};
        }

        return buffer;
    }

    bool FileUtils::writeTextFile(const std::string& path, const std::string& content)
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for writing: " << path << std::endl;
            return false;
        }

        file << content;
        return file.good();
    }

    bool FileUtils::writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for writing: " << path << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    }

    std::string FileUtils::getFileExtension(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.extension().string();
    }

    std::string FileUtils::getFileName(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.filename().string();
    }

    std::string FileUtils::getFileNameWithoutExtension(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.stem().string();
    }

    std::string FileUtils::getDirectory(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.parent_path().string();
    }

    std::string FileUtils::combinePath(const std::string& path1, const std::string& path2)
    {
        std::filesystem::path p1(path1);
        std::filesystem::path p2(path2);
        return (p1 / p2).string();
    }

    std::string FileUtils::getAbsolutePath(const std::string& path)
    {
        return std::filesystem::absolute(path).string();
    }

    bool FileUtils::createDirectory(const std::string& path)
    {
        return std::filesystem::create_directories(path);
    }

    std::vector<std::string> FileUtils::listFiles(const std::string& directory, const std::string& extension)
    {
        std::vector<std::string> files;

        if (!directoryExists(directory))
        {
            return files;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                if (extension.empty() || filename.ends_with(extension))
                {
                    files.push_back(filename);
                }
            }
        }

        return files;
    }

    std::vector<std::string> FileUtils::listDirectories(const std::string& directory)
    {
        std::vector<std::string> directories;

        if (!directoryExists(directory))
        {
            return directories;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_directory())
            {
                directories.push_back(entry.path().filename().string());
            }
        }
        return directories;
    }
} // namespace kera