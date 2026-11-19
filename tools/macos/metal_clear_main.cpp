#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "metal_clear_app.hpp"

namespace {

void printUsage(std::ostream& output) {
    output << "Usage: natureview_metal [--smoke] [--frames N] "
              "[--width N] [--height N]\n";
}

}  // namespace

int main(int argc, char** argv) {
    natureview::platform::MetalClearOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            printUsage(std::cout);
            return EXIT_SUCCESS;
        }
        if (argument == "--smoke") {
            if (options.frameLimit == 0) {
                options.frameLimit = 3;
            }
            continue;
        }
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n';
            return 2;
        }

        try {
            if (argument == "--frames") {
                options.frameLimit = static_cast<std::uint32_t>(std::stoul(argv[++index]));
            } else if (argument == "--width") {
                options.width = static_cast<std::uint32_t>(std::stoul(argv[++index]));
            } else if (argument == "--height") {
                options.height = static_cast<std::uint32_t>(std::stoul(argv[++index]));
            } else {
                std::cerr << "Unknown argument: " << argument << '\n';
                return 2;
            }
        } catch (const std::exception&) {
            std::cerr << "Invalid value for " << argument << '\n';
            return 2;
        }
    }

    return natureview::platform::runMetalClearApp(options);
}
