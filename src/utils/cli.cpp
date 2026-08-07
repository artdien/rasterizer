#include "utils/cli.hpp"

#include <algorithm>
#include <charconv>
#include <ranges>
#include <span>
#include <system_error>

namespace rasterizer::utils {

auto parse_cli_argument_str(i32 argc, c8* argv[], std::string_view argument) -> std::optional<std::string_view> {
  const auto arguments {std::span {argv, argv + argc}};
  const auto arguments_end {std::ranges::end(arguments)};

  if (auto it {std::ranges::find(arguments, argument)}; it != arguments_end && ++it != arguments_end) {
    return {*it};
  }

  return {};
}

auto parse_cli_argument_u32(i32 argc, c8* argv[], std::string_view argument) -> std::optional<u32> {
  const auto arguments {std::span {argv, argv + argc}};
  const auto arguments_end {std::ranges::end(arguments)};

  if (auto it {std::ranges::find(arguments, argument)}; it != arguments_end && ++it != arguments_end) {
    const auto as_string {std::string_view {*it}};
    auto parsed {u32 {}};

    if (const auto result {std::from_chars(as_string.data(), as_string.data() + as_string.size(), parsed)}; result.ec != std::errc::invalid_argument) {
      return parsed;
    }
  }

  return {};
}

} // namespace rasterizer::utils
