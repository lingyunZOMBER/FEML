#include "feml/core/version.hpp"

#include <format>

namespace feml {

    std::string versionString() {
        return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
    }

} // namespace feml