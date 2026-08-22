#include "Game/RustyRageGame.hpp"
#include "Core/Logging.hpp"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    try {
        // Initialize logging
        Core::Logger::getInstance().setLogLevel(Core::Logger::Level::Info);
        Core::Logger::getInstance().setLogToConsole(true);
        Core::Logger::getInstance().setLogToFile(true, "rustyrage.log");
        
        LOG_INFO("Starting Rusty Rage...");
        
        // Create and run the game
        auto game = std::make_unique<Game::RustyRageGame>(1280, 720, "Rusty Rage");
        game->run();
        
        LOG_INFO("Rusty Rage shutdown complete");
    } catch (const std::exception& e) {
        LOG_CRITICALF("Fatal error: %s", e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}