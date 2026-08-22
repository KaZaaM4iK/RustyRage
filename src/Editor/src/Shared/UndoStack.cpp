#include "Editor/Shared/UndoStack.hpp"

#include <sstream>
#include <utility>

namespace Editor {

namespace {

bool findObjectIndex(const MapData &data, const std::string &id,
                     size_t &outIndex) {
  for (size_t i = 0; i < data.objects.size(); ++i) {
    if (data.objects[i].id == id) {
      outIndex = i;
      return true;
    }
  }
  return false;
}

std::string fieldName(SetObjectFieldCommand::Field field) {
  switch (field) {
  case SetObjectFieldCommand::Field::Radius:
    return "radius";
  case SetObjectFieldCommand::Field::Health:
    return "health";
  case SetObjectFieldCommand::Field::Row:
    return "row";
  case SetObjectFieldCommand::Field::ModelPath:
    return "model";
  case SetObjectFieldCommand::Field::IsFinishLine:
    return "is_finish";
  case SetObjectFieldCommand::Field::Subtype:
    return "subtype";
  }
  return "?";
}

void applyFieldValue(MapObject &object, SetObjectFieldCommand::Field field,
                     const std::string &value) {
  switch (field) {
  case SetObjectFieldCommand::Field::Radius:
    object.radius = std::stof(value);
    break;
  case SetObjectFieldCommand::Field::Health:
    object.health = std::stof(value);
    break;
  case SetObjectFieldCommand::Field::Row:
    object.row = std::stoi(value);
    break;
  case SetObjectFieldCommand::Field::ModelPath:
    object.modelPath = value;
    break;
  case SetObjectFieldCommand::Field::IsFinishLine:
    object.isFinishLine = (value == "true" || value == "1");
    break;
  case SetObjectFieldCommand::Field::Subtype:
    object.subtype = std::stoi(value);
    break;
  }
}

std::string readFieldValue(const MapObject &object,
                           SetObjectFieldCommand::Field field) {
  switch (field) {
  case SetObjectFieldCommand::Field::Radius:
    return std::to_string(object.radius);
  case SetObjectFieldCommand::Field::Health:
    return std::to_string(object.health);
  case SetObjectFieldCommand::Field::Row:
    return std::to_string(object.row);
  case SetObjectFieldCommand::Field::ModelPath:
    return object.modelPath;
  case SetObjectFieldCommand::Field::IsFinishLine:
    return object.isFinishLine ? "true" : "false";
  case SetObjectFieldCommand::Field::Subtype:
    return std::to_string(object.subtype);
  }
  return {};
}

} // namespace

void UndoStack::execute(std::unique_ptr<Command> command, MapData &data) {
  if (!command)
    return;
  command->apply(data);
  m_undone.clear();
  m_done.push_back(std::move(command));
  while (m_done.size() > kMaxDepth) {
    m_done.erase(m_done.begin());
  }
}

bool UndoStack::undo(MapData &data) {
  if (m_done.empty())
    return false;
  auto command = std::move(m_done.back());
  m_done.pop_back();
  command->revert(data);
  m_undone.push_back(std::move(command));
  return true;
}

bool UndoStack::redo(MapData &data) {
  if (m_undone.empty())
    return false;
  auto command = std::move(m_undone.back());
  m_undone.pop_back();
  command->apply(data);
  m_done.push_back(std::move(command));
  return true;
}

void UndoStack::clear() {
  m_done.clear();
  m_undone.clear();
}

std::string UndoStack::undoLabel() const {
  return m_done.empty() ? std::string() : m_done.back()->label();
}

std::string UndoStack::redoLabel() const {
  return m_undone.empty() ? std::string() : m_undone.back()->label();
}

// ---------- AddObjectCommand ----------

AddObjectCommand::AddObjectCommand(MapObject object)
    : m_object(std::move(object)) {}

void AddObjectCommand::apply(MapData &data) {
  data.objects.push_back(m_object);
}

void AddObjectCommand::revert(MapData &data) { data.removeObject(m_object.id); }

std::string AddObjectCommand::label() const {
  return "Add object " + m_object.id;
}

// ---------- RemoveObjectCommand ----------

RemoveObjectCommand::RemoveObjectCommand(std::string id)
    : m_id(std::move(id)) {}

void RemoveObjectCommand::apply(MapData &data) {
  if (!findObjectIndex(data, m_id, m_index))
    return;
  m_snapshot = data.objects[m_index];
  data.objects.erase(data.objects.begin() + m_index);
}

void RemoveObjectCommand::revert(MapData &data) {
  if (m_index > data.objects.size())
    m_index = data.objects.size();
  data.objects.insert(data.objects.begin() + m_index, m_snapshot);
}

std::string RemoveObjectCommand::label() const {
  return "Remove object " + m_id;
}

// ---------- MoveObjectCommand ----------

MoveObjectCommand::MoveObjectCommand(std::string id, glm::vec3 oldPos,
                                     glm::vec3 newPos)
    : m_id(std::move(id)), m_oldPos(oldPos), m_newPos(newPos) {}

void MoveObjectCommand::apply(MapData &data) {
  size_t idx = 0;
  if (!findObjectIndex(data, m_id, idx))
    return;
  data.objects[idx].position = m_newPos;
}

void MoveObjectCommand::revert(MapData &data) {
  size_t idx = 0;
  if (!findObjectIndex(data, m_id, idx))
    return;
  data.objects[idx].position = m_oldPos;
}

std::string MoveObjectCommand::label() const { return "Move " + m_id; }

// ---------- SetObjectFieldCommand ----------

SetObjectFieldCommand::SetObjectFieldCommand(std::string id, Field field,
                                             std::string oldValue,
                                             std::string newValue)
    : m_id(std::move(id)), m_field(field), m_oldValue(std::move(oldValue)),
      m_newValue(std::move(newValue)) {}

void SetObjectFieldCommand::apply(MapData &data) {
  size_t idx = 0;
  if (!findObjectIndex(data, m_id, idx))
    return;
  applyFieldValue(data.objects[idx], m_field, m_newValue);
}

void SetObjectFieldCommand::revert(MapData &data) {
  size_t idx = 0;
  if (!findObjectIndex(data, m_id, idx))
    return;
  applyFieldValue(data.objects[idx], m_field, m_oldValue);
}

std::string SetObjectFieldCommand::label() const {
  return std::string("Set ") + fieldName(m_field) + " on " + m_id;
}

// ---------- AddTrackPointCommand ----------

AddTrackPointCommand::AddTrackPointCommand(TrackPoint point)
    : m_point(std::move(point)) {}

void AddTrackPointCommand::apply(MapData &data) {
  m_index = data.trackPoints.size();
  data.trackPoints.push_back(m_point);
}

void AddTrackPointCommand::revert(MapData &data) {
  if (m_index < data.trackPoints.size()) {
    data.trackPoints.erase(data.trackPoints.begin() + m_index);
  }
}

std::string AddTrackPointCommand::label() const {
  std::ostringstream s;
  s << "Add track point #" << m_index;
  return s.str();
}

// ---------- RemoveTrackPointCommand ----------

RemoveTrackPointCommand::RemoveTrackPointCommand(size_t index)
    : m_index(index) {}

void RemoveTrackPointCommand::apply(MapData &data) {
  if (m_index >= data.trackPoints.size())
    return;
  m_snapshot = data.trackPoints[m_index];
  data.trackPoints.erase(data.trackPoints.begin() + m_index);
}

void RemoveTrackPointCommand::revert(MapData &data) {
  if (m_index > data.trackPoints.size())
    m_index = data.trackPoints.size();
  data.trackPoints.insert(data.trackPoints.begin() + m_index, m_snapshot);
}

std::string RemoveTrackPointCommand::label() const {
  std::ostringstream s;
  s << "Remove track point #" << m_index;
  return s.str();
}

// ---------- MoveTrackPointCommand ----------

MoveTrackPointCommand::MoveTrackPointCommand(size_t index, glm::vec3 oldPos,
                                             glm::vec3 newPos)
    : m_index(index), m_oldPos(oldPos), m_newPos(newPos) {}

void MoveTrackPointCommand::apply(MapData &data) {
  if (m_index >= data.trackPoints.size())
    return;
  data.trackPoints[m_index].position = m_newPos;
}

void MoveTrackPointCommand::revert(MapData &data) {
  if (m_index >= data.trackPoints.size())
    return;
  data.trackPoints[m_index].position = m_oldPos;
}

std::string MoveTrackPointCommand::label() const {
  std::ostringstream s;
  s << "Move track point #" << m_index;
  return s.str();
}

// ---------- SetTrackClosedCommand ----------

SetTrackClosedCommand::SetTrackClosedCommand(bool oldValue, bool newValue)
    : m_oldValue(oldValue), m_newValue(newValue) {}

void SetTrackClosedCommand::apply(MapData &data) {
  data.trackClosed = m_newValue;
}

void SetTrackClosedCommand::revert(MapData &data) {
  data.trackClosed = m_oldValue;
}

std::string SetTrackClosedCommand::label() const {
  return m_newValue ? "Close track" : "Open track";
}

// ---------- SetTrackPointWidthCommand ----------
SetTrackPointWidthCommand::SetTrackPointWidthCommand(size_t index,
                                                     float oldWidth,
                                                     float newWidth)
    : m_index(index), m_oldWidth(oldWidth), m_newWidth(newWidth) {}

void SetTrackPointWidthCommand::apply(MapData &data) {
  if (m_index >= data.trackPoints.size())
    return;
  data.trackPoints[m_index].width = m_newWidth;
}

void SetTrackPointWidthCommand::revert(MapData &data) {
  if (m_index >= data.trackPoints.size())
    return;
  data.trackPoints[m_index].width = m_oldWidth;
}

std::string SetTrackPointWidthCommand::label() const {
  return "Set track width #" + std::to_string(m_index);
}

// ---------- InsertTrackPointCommand ----------
InsertTrackPointCommand::InsertTrackPointCommand(size_t index, TrackPoint point)
    : m_index(index), m_point(std::move(point)) {}

void InsertTrackPointCommand::apply(MapData &data) {
  if (m_index > data.trackPoints.size()) {
    m_index = data.trackPoints.size();
  }
  data.trackPoints.insert(
      data.trackPoints.begin() + static_cast<std::ptrdiff_t>(m_index), m_point);
}

void InsertTrackPointCommand::revert(MapData &data) {
  if (m_index < data.trackPoints.size()) {
    data.trackPoints.erase(data.trackPoints.begin() +
                           static_cast<std::ptrdiff_t>(m_index));
  }
}

std::string InsertTrackPointCommand::label() const {
  return "Insert track point #" + std::to_string(m_index);
}

} // namespace Editor