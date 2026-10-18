#ifndef SLALOM_CXX_FILE_HPP
#define SLALOM_CXX_FILE_HPP

/*
 * RAII file handle for the C++ implementation.
 *
 * The C code resolves the data file relative to the executable through
 * fopen_p() (src/sl-driver.c), because 'geom' has to live in the same
 * directory as the binary. The C++ version keeps that behaviour but owns the
 * handle, so it is released on every exit path -- including the error and
 * exception paths that the hand-rolled FILE* juggling in the C code can leak.
 */

#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace slalom {

/* Absolute path of the running executable; empty if it cannot be determined
 * (e.g. /proc is not mounted). Linux specific, same as get_path_name(). */
inline std::string executable_path()
{
    std::vector<char> buffer(BUFSIZ);
    const ssize_t len =
        ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len < 0)
        return std::string();
    buffer[static_cast<std::size_t>(len)] = '\0';
    return std::string(buffer.data());
}

/* Open "name" next to this executable. */
class File
{
  public:
    explicit File(const char *name, const char *mode = "r")
        : path_(resolve(name)), stream_(std::fopen(path_.c_str(), mode))
    {
    }

    ~File()
    {
        if (stream_ != nullptr)
            std::fclose(stream_);
    }

    File(const File &) = delete;
    File &operator=(const File &) = delete;

    explicit operator bool() const
    {
        return stream_ != nullptr;
    }

    FILE *get() const
    {
        return stream_;
    }

    const std::string &path() const
    {
        return path_;
    }

  private:
    static std::string resolve(const char *name)
    {
        const std::string exe = executable_path();
        const std::string::size_type slash = exe.find_last_of('/');
        if (exe.empty() || slash == std::string::npos)
            return std::string(name);
        return exe.substr(0, slash + 1) + name;
    }

    std::string path_;
    FILE *stream_;
};

} // namespace slalom

#endif