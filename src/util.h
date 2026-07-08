#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mybarrier {

std::string trim(std::string value);
bool starts_with(const std::string& value, const std::string& prefix);
std::vector<std::string> split(const std::string& value, char delimiter);
std::string random_hex(std::size_t bytes);
std::string fnv1a64_hex(const std::string& data);
std::string fnv1a64_hex(const std::vector<char>& data);
std::string escape_field(const std::string& value);
std::string unescape_field(const std::string& value);
std::string sanitize_filename(const std::string& name);
std::string read_text_file(const std::filesystem::path& path);
void write_text_file(const std::filesystem::path& path, const std::string& data);
std::vector<char> read_binary_file(const std::filesystem::path& path);
void write_binary_file(const std::filesystem::path& path, const std::vector<char>& data);
std::string hostname();
std::string now_iso8601();
uint16_t parse_port(const std::string& value, uint16_t fallback);

}  // namespace mybarrier
