#include "Editor/Vehicle/VehicleEditorApp.hpp"

namespace Editor {

float *VehicleEditorApp::selectedStatPtr() {
  switch (m_selectedStat) {
  case 0:
    return &m_vehicle.mass;
  case 1:
    return &m_vehicle.maxSpeed;
  case 2:
    return &m_vehicle.acceleration;
  case 3:
    return &m_vehicle.turnRate;
  case 4:
    return &m_vehicle.grip;
  case 5:
    return &m_vehicle.maxHealth;
  default:
    return nullptr;
  }
}

void VehicleEditorApp::saveVehicle() {
  ensureVehiclesDirectory();

  if (m_vehicle.id.empty()) {
    m_vehicle.id = "vehicle_new";
  }

  m_currentPath = vehiclesDirectory() + "/" + m_vehicle.id + ".rrveh";

  if (m_vehicle.saveToFile(m_currentPath)) {
    m_dirty = false;
    m_status = "Сохранено: " + m_currentPath;
    refreshList();
  } else {
    m_status = "Ошибка сохранения";
  }
}

} // namespace Editor