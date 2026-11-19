/*  Filesystem
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *
 *  We disllow direct use of "std::filesystem" because it's too easy to mess up
 *  Unicode support.
 *
 *  In this project, we define "char*" and "std::string" as UTF-8. But this is
 *  not what happens in Windows as it uses ANSI or whatever the locale is.
 *
 *  This applies to all standard library functions as well as Window's own APIs.
 *
 *  Therefore, we cannot pass "char*"/"std::string" into any libraries. But this
 *  is too easy to do accidentally.
 *
 *  Therefore, we ban the direct use of "std::filesystem" in our code base and
 *  wrap them instead.
 *
 */

#ifndef PokemonAutomation_Filesystem_Filesystem_H
#define PokemonAutomation_Filesystem_Filesystem_H

#include <string_view>
#include <cstdint>
#include <vector>
#include "FilePath.h"

namespace PokemonAutomation{
namespace Filesystem{


Path absolute(const Path& path);

//  Whether a path exists.
bool exists(const Path& path);

//  Create every missing directory along the path.
//  If all related directories already exist, return false. Otherwise return true.
bool create_directories(const Path& path);

//  Delete the contents of the path (if it is a directory) and the contents of all its subdirectories, recursively.
//  Then delete the file/directory of path itself. Symlinks are not followed (symlink is removed, not its target).
bool remove(const Path& path);
std::uintmax_t remove_all(const Path& path);

//  Copy a file.
bool copy_file(const Path& from, const Path& to);

//  Return the size of a file in bytes.
//  Throw std::filesystem::filesystem_error on underlying OS API errors
std::uintmax_t file_size(const Path& path);

//  Return the size of a file in bytes.
//  If an error occurs, set `ec`. Execute `ec.clear()` if no errors occur.
std::uintmax_t file_size(const Path& path, std::error_code& ec);

//  Rename a file or directory.
//  Throw std::filesystem::filesystem_error on underlying OS API errors
void rename(const Path& old_path, const Path& new_path);

//  Rename a file or directory.
//  If an error occurs, set `ec`. Execute `ec.clear()` if no errors occur.
void rename(const Path& old_path, const Path& new_path, std::error_code& ec);


//  The current working directory.
Path current_path();

//  Change the current working directory.
void set_current_path(const Path& path);
void set_current_path(const Path& path, std::error_code& ec);

//  The path relative to the current working directory.
//  If no relative path can be formed (e.g. different drives on Windows),
//  return the absolute path and set `ec`. Execute `ec.clear()` if a relative path was formed.
Path relative(const Path& path, std::error_code& ec);

//  Whether the path is a directory.
bool is_directory(const Path& path);

//  Whether the path is a regular file.
bool is_regular_file(const Path& path);

//  List the entries (files and directories) directly inside the directory at `path`.
//  Hidden entries (names starting with '.') are excluded. If `recursive` is true,
//  entries in all subdirectories are also listed.
//  Entries are returned sorted by name, case-insensitively.
//  Returns an empty list if `path` does not exist, is not a directory,
//  or cannot be read (matches Qt's QDir::entryInfoList behavior of failing silently).
std::vector<Path> list_directory(const Path& path, bool recursive = false);

//  Whether the current user can read the file/directory at `path`.
bool is_readable(const Path& path);

//  Whether the current user can write the file/directory at `path`.
bool is_writable(const Path& path);

//  A unique temporary file path in the same directory as `path`, for atomic writes
//  (write to the temporary path, then rename() onto `path`).
//  No file is created; the caller is responsible for creating it and for
//  deleting it if the operation fails.
Path unique_temp_path(const Path& path);

//  The path to the actual binary that is running.
Path application_binary_name();
Path application_binary_path();
Path application_binary_directory();

//  Path to where the application is installed.
//  All immutable resources are relative to this.
Path application_install_path();

//  Path to where the application saves its output/settings.
Path application_scratch_path();


#if defined(__APPLE__)
// Set a validated macOS settings profile before any settings paths are cached.
// GUI launch uses --profile NAME or the build-bound PA_LAB_PROFILE identity.
void set_startup_profile(std::string_view profile);
// Return the identity used for the window title and scratch directory.
const std::string& STARTUP_PROFILE();
#endif



}
}
#endif
