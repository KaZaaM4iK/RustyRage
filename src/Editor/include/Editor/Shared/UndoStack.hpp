#pragma once

#include "../Map/MapData.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Editor {

// Base class for editor commands. Each command knows how to apply itself
// to a MapData and how to revert. Reverting must be exact — full state
// restore, not a diff.
class Command {
public:
  virtual ~Command() = default;
  virtual void apply(MapData &data) = 0;
  virtual void revert(MapData &data) = 0;
  virtual std::string label() const = 0;
};

class UndoStack {
public:
  static constexpr size_t kMaxDepth = 128;

  // Executes a command and pushes it onto the stack.
  void execute(std::unique_ptr<Command> command, MapData &data);

  // Reverts the most recently executed command.
  bool undo(MapData &data);

  // Re-applies a previously reverted command.
  bool redo(MapData &data);

  void clear();

  bool canUndo() const { return !m_done.empty(); }
  bool canRedo() const { return !m_undone.empty(); }

  std::string undoLabel() const;
  std::string redoLabel() const;

  size_t undoDepth() const { return m_done.size(); }
  size_t redoDepth() const { return m_undone.size(); }

private:
  std::vector<std::unique_ptr<Command>> m_done;
  std::vector<std::unique_ptr<Command>> m_undone;
};

// Concrete commands for map editing.

class AddObjectCommand : public Command {
public:
  // The caller (editor) is responsible for assigning a unique ID before
  // constructing the command — keeps undo/redo deterministic.
  AddObjectCommand(MapObject object);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  MapObject m_object;
};

class RemoveObjectCommand : public Command {
public:
  explicit RemoveObjectCommand(std::string id);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  MapObject m_snapshot;
  std::string m_id;
  size_t m_index = 0;
};

class MoveObjectCommand : public Command {
public:
  MoveObjectCommand(std::string id, glm::vec3 oldPos, glm::vec3 newPos);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  std::string m_id;
  glm::vec3 m_oldPos;
  glm::vec3 m_newPos;
};

class SetObjectFieldCommand : public Command {
public:
  enum class Field { Radius, Health, Row, ModelPath, IsFinishLine, Subtype };

  SetObjectFieldCommand(std::string id, Field field, std::string oldValue,
                        std::string newValue);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  std::string m_id;
  Field m_field;
  std::string m_oldValue;
  std::string m_newValue;
};

class AddTrackPointCommand : public Command {
public:
  explicit AddTrackPointCommand(TrackPoint point);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  TrackPoint m_point;
  size_t m_index = 0;
};

class RemoveTrackPointCommand : public Command {
public:
  explicit RemoveTrackPointCommand(size_t index);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  TrackPoint m_snapshot;
  size_t m_index = 0;
};

class MoveTrackPointCommand : public Command {
public:
  MoveTrackPointCommand(size_t index, glm::vec3 oldPos, glm::vec3 newPos);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  size_t m_index;
  glm::vec3 m_oldPos;
  glm::vec3 m_newPos;
};

class SetTrackClosedCommand : public Command {
public:
  SetTrackClosedCommand(bool oldValue, bool newValue);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  bool m_oldValue;
  bool m_newValue;
};

class SetTrackPointWidthCommand : public Command {
public:
  SetTrackPointWidthCommand(size_t index, float oldWidth, float newWidth);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  size_t m_index;
  float m_oldWidth;
  float m_newWidth;
};

class InsertTrackPointCommand : public Command {
public:
  InsertTrackPointCommand(size_t index, TrackPoint point);
  void apply(MapData &data) override;
  void revert(MapData &data) override;
  std::string label() const override;

private:
  size_t m_index;
  TrackPoint m_point;
};

} // namespace Editor