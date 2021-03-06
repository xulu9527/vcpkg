#pragma once

#include <vcpkg/base/expected.h>
#include <vcpkg/base/ignore_errors.h>
#include <vcpkg/base/pragmas.h>

#include <string.h>

#if !defined(VCPKG_USE_STD_FILESYSTEM)
#error The build system must set VCPKG_USE_STD_FILESYSTEM.
#endif // !defined(VCPKG_USE_STD_FILESYSTEM)

#if VCPKG_USE_STD_FILESYSTEM
#include <filesystem>
#else
#include <experimental/filesystem>
#endif

namespace fs
{
#if VCPKG_USE_STD_FILESYSTEM
    namespace stdfs = std::filesystem;
#else
    namespace stdfs = std::experimental::filesystem;
#endif

    using stdfs::copy_options;
    using stdfs::directory_iterator;
    using stdfs::path;
    using stdfs::perms;

    path u8path(vcpkg::StringView s);
    inline path u8path(const char* first, const char* last) { return u8path(vcpkg::StringView{first, last}); }
    inline path u8path(const char* s) { return u8path(vcpkg::StringView{s, s + ::strlen(s)}); }

#if defined(_MSC_VER)
    inline path u8path(std::string::const_iterator first, std::string::const_iterator last)
    {
        if (first == last)
        {
            return path{};
        }
        else
        {
            auto firstp = &*first;
            return u8path(vcpkg::StringView{firstp, firstp + (last - first)});
        }
    }
#endif

    std::string u8string(const path& p);
    std::string generic_u8string(const path& p);

#if defined(_WIN32)
    enum class file_type
    {
        none = 0,
        not_found = -1,
        regular = 1,
        directory = 2,
        symlink = 3,
        block = 4,
        character = 5,
        fifo = 6,
        socket = 7,
        unknown = 8,
        // also stands for a junction
        directory_symlink = 42
    };

    struct file_status
    {
        explicit file_status(file_type type = file_type::none, perms permissions = perms::unknown) noexcept
            : m_type(type), m_permissions(permissions)
        {
        }

        file_type type() const noexcept { return m_type; }
        void type(file_type type) noexcept { m_type = type; }

        perms permissions() const noexcept { return m_permissions; }
        void permissions(perms perm) noexcept { m_permissions = perm; }

    private:
        file_type m_type;
        perms m_permissions;
    };

    struct SystemHandle
    {
        using type = intptr_t; // HANDLE
        type system_handle = -1;

        bool is_valid() const { return system_handle != -1; }
    };

#else

    using stdfs::file_type;
    // to set up ADL correctly on `file_status` objects, we are defining
    // this in our own namespace
    struct file_status : private stdfs::file_status
    {
        using stdfs::file_status::file_status;
        using stdfs::file_status::permissions;
        using stdfs::file_status::type;
    };

    struct SystemHandle
    {
        using type = int; // file descriptor
        type system_handle = -1;

        bool is_valid() const { return system_handle != -1; }
    };

#endif

    inline bool is_symlink(file_status s) noexcept
    {
#if defined(_WIN32)
        if (s.type() == file_type::directory_symlink) return true;
#endif
        return s.type() == file_type::symlink;
    }
    inline bool is_regular_file(file_status s) { return s.type() == file_type::regular; }
    inline bool is_directory(file_status s) { return s.type() == file_type::directory; }
    inline bool exists(file_status s) { return s.type() != file_type::not_found && s.type() != file_type::none; }
}

/*
    if someone attempts to use unqualified `symlink_status` or `is_symlink`,
    they might get the ADL version, which is broken.
    Therefore, put `(symlink_)?status` as deleted in the global namespace, so
    that they get an error.

    We also want to poison the ADL on the other functions, because
    we don't want people calling these functions on paths
*/
void status(const fs::path& p) = delete;
void status(const fs::path& p, std::error_code& ec) = delete;
void symlink_status(const fs::path& p) = delete;
void symlink_status(const fs::path& p, std::error_code& ec) = delete;
void is_symlink(const fs::path& p) = delete;
void is_symlink(const fs::path& p, std::error_code& ec) = delete;
void is_regular_file(const fs::path& p) = delete;
void is_regular_file(const fs::path& p, std::error_code& ec) = delete;
void is_directory(const fs::path& p) = delete;
void is_directory(const fs::path& p, std::error_code& ec) = delete;

namespace vcpkg::Files
{
    struct Filesystem
    {
        std::string read_contents(const fs::path& file_path, LineInfo linfo) const;
        virtual Expected<std::string> read_contents(const fs::path& file_path) const = 0;
        /// <summary>Read text lines from a file</summary>
        /// <remarks>Lines will have up to one trailing carriage-return character stripped (CRLF)</remarks>
        virtual Expected<std::vector<std::string>> read_lines(const fs::path& file_path) const = 0;
        virtual fs::path find_file_recursively_up(const fs::path& starting_dir, const fs::path& filename) const = 0;
        virtual std::vector<fs::path> get_files_recursive(const fs::path& dir) const = 0;
        virtual std::vector<fs::path> get_files_non_recursive(const fs::path& dir) const = 0;
        void write_lines(const fs::path& file_path, const std::vector<std::string>& lines, LineInfo linfo);
        virtual void write_lines(const fs::path& file_path,
                                 const std::vector<std::string>& lines,
                                 std::error_code& ec) = 0;
        void write_contents(const fs::path& path, const std::string& data, LineInfo linfo);
        virtual void write_contents(const fs::path& file_path, const std::string& data, std::error_code& ec) = 0;
        void rename(const fs::path& oldpath, const fs::path& newpath, LineInfo linfo);
        virtual void rename(const fs::path& oldpath, const fs::path& newpath, std::error_code& ec) = 0;
        virtual void rename_or_copy(const fs::path& oldpath,
                                    const fs::path& newpath,
                                    StringLiteral temp_suffix,
                                    std::error_code& ec) = 0;
        bool remove(const fs::path& path, LineInfo linfo);
        bool remove(const fs::path& path, ignore_errors_t);
        virtual bool remove(const fs::path& path, std::error_code& ec) = 0;

        virtual void remove_all(const fs::path& path, std::error_code& ec, fs::path& failure_point) = 0;
        void remove_all(const fs::path& path, LineInfo li);
        void remove_all(const fs::path& path, ignore_errors_t);
        virtual void remove_all_inside(const fs::path& path, std::error_code& ec, fs::path& failure_point) = 0;
        void remove_all_inside(const fs::path& path, LineInfo li);
        void remove_all_inside(const fs::path& path, ignore_errors_t);
        bool exists(const fs::path& path, std::error_code& ec) const;
        bool exists(LineInfo li, const fs::path& path) const;
        bool exists(const fs::path& path, ignore_errors_t = ignore_errors) const;
        virtual bool is_directory(const fs::path& path) const = 0;
        virtual bool is_regular_file(const fs::path& path) const = 0;
        virtual bool is_empty(const fs::path& path) const = 0;
        virtual bool create_directory(const fs::path& path, std::error_code& ec) = 0;
        bool create_directory(const fs::path& path, ignore_errors_t);
        bool create_directory(const fs::path& path, LineInfo li);
        virtual bool create_directories(const fs::path& path, std::error_code& ec) = 0;
        bool create_directories(const fs::path& path, ignore_errors_t);
        bool create_directories(const fs::path& path, LineInfo);
        virtual void copy(const fs::path& oldpath, const fs::path& newpath, fs::copy_options opts) = 0;
        virtual bool copy_file(const fs::path& oldpath,
                               const fs::path& newpath,
                               fs::copy_options opts,
                               std::error_code& ec) = 0;
        void copy_file(const fs::path& oldpath, const fs::path& newpath, fs::copy_options opts, LineInfo li);
        virtual void copy_symlink(const fs::path& oldpath, const fs::path& newpath, std::error_code& ec) = 0;
        virtual fs::file_status status(const fs::path& path, std::error_code& ec) const = 0;
        virtual fs::file_status symlink_status(const fs::path& path, std::error_code& ec) const = 0;
        fs::file_status status(LineInfo li, const fs::path& p) const noexcept;
        fs::file_status status(const fs::path& p, ignore_errors_t) const noexcept;
        fs::file_status symlink_status(LineInfo li, const fs::path& p) const noexcept;
        fs::file_status symlink_status(const fs::path& p, ignore_errors_t) const noexcept;
        virtual fs::path absolute(const fs::path& path, std::error_code& ec) const = 0;
        fs::path absolute(LineInfo li, const fs::path& path) const;
        virtual fs::path canonical(const fs::path& path, std::error_code& ec) const = 0;
        fs::path canonical(LineInfo li, const fs::path& path) const;
        fs::path canonical(const fs::path& path, ignore_errors_t) const;
        virtual fs::path current_path(std::error_code&) const = 0;
        fs::path current_path(LineInfo li) const;
        virtual void current_path(const fs::path& path, std::error_code&) = 0;
        void current_path(const fs::path& path, LineInfo li);

        // waits forever for the file lock
        virtual fs::SystemHandle take_exclusive_file_lock(const fs::path& path, std::error_code&) = 0;
        // waits, at most, 1.5 seconds, for the file lock
        virtual fs::SystemHandle try_take_exclusive_file_lock(const fs::path& path, std::error_code&) = 0;
        virtual void unlock_file_lock(fs::SystemHandle handle, std::error_code&) = 0;

        virtual std::vector<fs::path> find_from_PATH(const std::string& name) const = 0;
    };

    Filesystem& get_real_filesystem();

    static constexpr const char* FILESYSTEM_INVALID_CHARACTERS = R"(\/:*?"<>|)";

    bool has_invalid_chars_for_filesystem(const std::string& s);

    void print_paths(const std::vector<fs::path>& paths);

    // Performs "lhs / rhs" according to the C++17 Filesystem Library Specification.
    // This function exists as a workaround for TS implementations.
    fs::path combine(const fs::path& lhs, const fs::path& rhs);

#if defined(_WIN32)
    constexpr char preferred_separator = '\\';
#else
    constexpr char preferred_separator = '/';
#endif // _WIN32

    // Adds file as a new path element to the end of base, with an additional slash if necessary
    std::string add_filename(StringView base, StringView file);

#if defined(_WIN32)
    fs::path win32_fix_path_case(const fs::path& source);
#endif // _WIN32
}
