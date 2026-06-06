#pragma once

#include <string>

namespace ghostpad::ui {

std::string pickFile(const std::string& title = "Select File", const std::string& filter_desc = "All Files", const std::string& filter_ext = "*.*");

} // namespace ghostpad::ui
