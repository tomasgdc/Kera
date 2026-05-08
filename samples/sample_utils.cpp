#include "sample_utils.h"

#include <filesystem>

#include "kera/utilities/file_utils.h"

namespace kera {

std::string resolveShaderPath(const std::string& shaderPath) {
  // Prefer the path exactly as provided.
  if (FileUtils::fileExists(shaderPath)) {
    return shaderPath;
  }

  // Then try locations used by source-tree and build-tree launches.
  const std::filesystem::path cwdPath =
      std::filesystem::current_path() / shaderPath;
  if (std::filesystem::exists(cwdPath) &&
      std::filesystem::is_regular_file(cwdPath)) {
    return cwdPath.string();
  }

  const std::filesystem::path parentPath =
      std::filesystem::current_path().parent_path() / shaderPath;
  if (std::filesystem::exists(parentPath) &&
      std::filesystem::is_regular_file(parentPath)) {
    return parentPath.string();
  }

  const std::filesystem::path buildPath =
      std::filesystem::current_path() / "samples" / shaderPath;
  if (std::filesystem::exists(buildPath) &&
      std::filesystem::is_regular_file(buildPath)) {
    return buildPath.string();
  }

  return shaderPath;
}

}  // namespace kera
