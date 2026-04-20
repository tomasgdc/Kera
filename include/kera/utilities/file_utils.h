#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace kera {

class FileUtils {
public:
    // File operations
    static bool fileExists(const std::string& path);
    static bool directoryExists(const std::string& path);
    static std::string readTextFile(const std::string& path);
    static std::vector<uint8_t> readBinaryFile(const std::string& path);
    static bool writeTextFile(const std::string& path, const std::string& content);
    static bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data);

    // Path operations
    static std::string getFileExtension(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getFileNameWithoutExtension(const std::string& path);
    static std::string getDirectory(const std::string& path);
    static std::string combinePath(const std::string& path1, const std::string& path2);
    static std::string getAbsolutePath(const std::string& path);

    // Directory operations
    static bool createDirectory(const std::string& path);
    static std::vector<std::string> listFiles(const std::string& directory, const std::string& extension = "");
    static std::vector<std::string> listDirectories(const std::string& directory);

private:
    FileUtils() = delete;
};

} // namespace kera