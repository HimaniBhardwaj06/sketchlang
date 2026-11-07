#pragma once
#include "vm.hpp"
#include <string>
#include <vector>

namespace sk {

// Renders draw commands as a standalone SVG image (100 x 100 canvas units).
std::string toSvg(const std::vector<DrawCmd>& draws);

} // namespace sk
