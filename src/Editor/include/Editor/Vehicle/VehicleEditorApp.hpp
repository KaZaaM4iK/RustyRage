#pragma once

#include "Editor/Vehicle/VehicleDef.hpp"

#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct TTF_Font;

namespace Core {
class InputManager;
}

namespace Editor {

class VehicleEditorApp {
public:
  VehicleEditorApp(int width, int height, const std::string &title);
  ~VehicleEditorApp();

  void run();

private:
  int m_width;
  int m_height;
  std::string m_title;
  bool m_running = false;

  SDL_Window *m_window = nullptr;
  SDL_Renderer *m_renderer = nullptr;
  TTF_Font *m_font = nullptr;

  std::unique_ptr<Core::InputManager> m_input;

  VehicleDef m_vehicle;
  std::string m_currentPath;
  bool m_dirty = false;
  std::string m_status = "Готов";

  // 0 mass … 5 maxHealth — цикл [ ] / стрелки
  int m_selectedStat = 0;
  static constexpr int kStatCount = 6;

  std::vector<std::string> m_fileList;

  bool initialize();
  void shutdown();
  void processEvents();
  void update(float dt);
  void render();

  void newVehicle();
  void saveVehicle();
  void loadVehicle(const std::string &fileName);
  void refreshList();
  void adjustSelectedStat(float delta);
  float *selectedStatPtr();
  const char *selectedStatLabel() const;

  std::string vehiclesDirectory() const { return "vehicles"; }
  void ensureVehiclesDirectory() const;
};

} // namespace Editor