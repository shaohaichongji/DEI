#pragma once

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

/**
 * 递归获取指定目录下所有符合条件的文件
 * @param directory 目录路径
 * @param recursive 是否递归子目录
 * @return 符合条件的文件路径列表 FindImageFiles
 */
static std::vector<fs::path> FindByEndSymbolFiles(const fs::path& directory, const std::vector<std::string>& end_symbol, bool recursive = true)
{
    std::vector<fs::path> result;
    auto fun_is_contain = [&](const std::string& str_symbol) {
        for (auto iter : end_symbol) {
            if (iter == str_symbol) {
                return true;
            }
        }
        return false;
    };
    try {
        if (recursive) {
            // 递归遍历
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    const auto& path = entry.path();
                    if (fun_is_contain(path.extension().string())) {
                        result.push_back(path);
                    }
                }
            }
        }
        else {
            // 非递归遍历
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    const auto& path = entry.path();
                    if (fun_is_contain(path.extension().string())) {
                        result.push_back(path);
                    }
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    }

    return result;
}

/**
 * 按照时间顺序排序
 * @param paths 将std::vector<fs::path>按时间排序并返回
 */
static void SortByLastModified(std::vector<fs::path>& paths)
{
    std::sort(paths.begin(), paths.end(),
        [](const fs::path& a, const fs::path& b) {
            try {
                auto timeA = fs::last_write_time(a);
                auto timeB = fs::last_write_time(b);
                return timeA < timeB;
            }
            catch (const fs::filesystem_error& e) {
                std::cerr << "警告: 无法获取 " << a << " 的时间 - " << e.what() << std::endl;
                return false;  // 将错误文件放在后面
            }
        });
}

/**
 * 创建路径，判断传入参数的路径是否存在，不存在就创建
 * @param dir_path 判断路径是否存在，不存在就创建
 */
static bool MkdirPath(const std::string& dir_path)
{
    try {
        // 检查目录是否存在
        if (!fs::exists(dir_path)) {
            // 创建目录（包括父目录）
            bool created = fs::create_directories(dir_path);
            if (created) {
                std::cout << "目录创建成功: " << dir_path << std::endl;
            }
        }
        else {
            //std::cout << "目录已存在: " << dir_path << std::endl;
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

class ExecutablePath {
public:
    // 方法1：使用 std::filesystem（C++17）
    static fs::path GetExecutablePath() {
        try {
#ifdef _WIN32
            return GetExecutablePathWindows();
#else
            return GetExecutablePathPosix();
#endif
        }
        catch (const std::exception& e) {
            std::cerr << "获取可执行文件路径失败: " << e.what() << std::endl;
            return fs::current_path();  // 回退到当前工作目录
        }
    }

    // 方法2：仅获取目录（不含文件名）
    static fs::path GetExecutableDirectory() {
        fs::path exePath = GetExecutablePath();
        return exePath.parent_path();
    }

    // 方法3：获取文件名（不含路径）
    static std::string getExecutableName() {
        fs::path exePath = GetExecutablePath();
        return exePath.filename().string();
    }

private:
    // Windows 实现
    static fs::path GetExecutablePathWindows() {
#ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

        if (length == 0) {
            throw std::runtime_error("GetModuleFileNameA failed");
        }

        if (length == MAX_PATH) {
            // 路径可能被截断，尝试使用更大的缓冲区
            std::vector<char> largeBuffer(32768);  // 32KB
            length = GetModuleFileNameA(nullptr, largeBuffer.data(),
                static_cast<DWORD>(largeBuffer.size()));

            if (length == 0 || length == largeBuffer.size()) {
                throw std::runtime_error("Path too long or GetModuleFileNameA failed");
            }

            return fs::path(largeBuffer.data());
        }

        return fs::path(buffer);
#else
        throw std::runtime_error("Windows API not available on this platform");
#endif
    }

    // Linux/macOS 实现
    static fs::path GetExecutablePathPosix() {
#ifdef __linux__
        // Linux: 使用 /proc/self/exe
        char buffer[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

        if (length == -1) {
            throw std::runtime_error("readlink failed");
        }

        buffer[length] = '\0';
        return fs::path(buffer);

#elif defined(__APPLE__)
        // macOS: 使用 _NSGetExecutablePath
        char buffer[PATH_MAX];
        uint32_t size = sizeof(buffer);

        if (_NSGetExecutablePath(buffer, &size) != 0) {
            // 缓冲区太小，分配更大的缓冲区
            std::vector<char> largeBuffer(size);
            if (_NSGetExecutablePath(largeBuffer.data(), &size) != 0) {
                throw std::runtime_error("_NSGetExecutablePath failed");
            }
            return fs::path(largeBuffer.data());
        }

        return fs::path(buffer);

#else
        // 其他 POSIX 系统
        throw std::runtime_error("Unsupported POSIX platform");
#endif
    }
};

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

inline std::tm* safe_localtime(const std::time_t* timer, std::tm* result)
{
#ifdef _WIN32
    return localtime_s(result, timer) == 0 ? result : nullptr;
#else
    return localtime_r(timer, result);
#endif
}

static std::string GetCurrentTimeWithMilliseconds()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::tm bt;
    safe_localtime(&timer, &bt);

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}