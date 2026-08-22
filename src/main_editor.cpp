#include "Core/Logging.hpp"
#include "Editor/Map/MapEditorApp.hpp"

#include <iostream>
#include <memory>

int main(int /*argc*/, char * /*argv*/[]) {
  try {
    Core::Logger::getInstance().setLogLevel(Core::Logger::Level::Info);
    Core::Logger::getInstance().setLogToConsole(true);
    Core::Logger::getInstance().setLogToFile(true, "editor.log");

    LOG_INFO("Starting RustyRage Map Editor");

    Editor::MapEditorApp editor(1600, 900, "RustyRage — Редактор карт");
    editor.run();

    LOG_INFO("Map editor shutdown complete");
  } catch (const std::exception &e) {
    LOG_CRITICALF("Fatal editor error: %s", e.what());
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
