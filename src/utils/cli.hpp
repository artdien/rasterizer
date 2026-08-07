#pragma once

#include <optional>
#include <string_view>

#include "platform/types.hpp"

namespace rasterizer::utils {

/// @brief Parses a string value from the command line arguments following a specific key.
///
/// Searches for the occurrence of the specified argument key in the CLI arguments.
/// If more than one argument key exists with the same value, only the first is found.
///
/// @param argc     The number of command-line arguments.
/// @param argv     The array of command-line argument strings.
/// @param argument The key to search for (e.g. "-file").
///
/// @return std::optional<std::string_view> The value following the key if found. Otherwise std::nullopt.
auto parse_cli_argument_str(i32 argc, c8* argv[], std::string_view argument) -> std::optional<std::string_view>;

/// @brief Parses an unsigned integer value from the command line arguments following a specific key.
///
/// Searches for the occurrence of the specified argument key in the CLI arguments.
/// If more than one argument key exists with the same value, only the first is found.
///
/// @param argc     The number of command-line arguments.
/// @param argv     The array of command-line argument strings.
/// @param argument The key to search for (e.g. "-width").
///
/// @return std::optional<u32> The value following the key if found. Otherwise std::nullopt.
auto parse_cli_argument_u32(i32 argc, c8* argv[], std::string_view argument) -> std::optional<u32>;

/// @brief Parses a floating point value from the command line arguments following a specific key.
///
/// Searches for the occurrence of the specified argument key in the CLI arguments.
/// If more than one argument key exists with the same value, only the first is found.
///
/// @param argc     The number of command-line arguments.
/// @param argv     The array of command-line argument strings.
/// @param argument The key to search for (e.g. "-x", "-y" or "-z").
///
/// @return std::optional<f32> The value following the key if found. Otherwise std::nullopt.
auto parse_cli_argument_f32(i32 argc, c8* argv[], std::string_view argument) -> std::optional<f32>;

/// @brief Checks for a boolean flag in the command-line arguments.
///
/// Searches for the occurrence of the specified flag in the CLI arguments.
/// If found, returns true. This is for flags that don't take a value.
///
/// @param argc The number of command-line arguments.
/// @param argv The array of command-line argument strings.
/// @param flag The flag to search for (e.g. "-info").
///
/// @return bool true if the flag is found, false otherwise.
auto parse_cli_flag(i32 argc, c8* argv[], std::string_view flag) -> bool;

} // namespace rasterizer::utils
