#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rr::world {

enum class MapDiagnosticSeverity : std::uint8_t {
    Warning = 0,
    Error = 1,
};

struct MapDiagnostic {
    MapDiagnosticSeverity severity{MapDiagnosticSeverity::Error};
    std::string code{};
    std::string path{};
    std::string message{};
};

struct MapValidationResult {
    std::vector<MapDiagnostic> diagnostics{};

    [[nodiscard]] bool ok() const noexcept {
        return error_count() == 0;
    }

    [[nodiscard]] std::size_t error_count() const noexcept {
        std::size_t count = 0;
        for (const MapDiagnostic& diagnostic : diagnostics) {
            if (diagnostic.severity == MapDiagnosticSeverity::Error) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t warning_count() const noexcept {
        std::size_t count = 0;
        for (const MapDiagnostic& diagnostic : diagnostics) {
            if (diagnostic.severity == MapDiagnosticSeverity::Warning) {
                ++count;
            }
        }
        return count;
    }
};

} // namespace rr::world
