#include "Editor/Map/MapData.hpp"
#include "Core/Logging.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Editor {

namespace {

std::string trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string escapeJson(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result += ch;
      break;
    }
  }
  return result;
}

std::string readFileToString(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Scope-based helper: collects the substring of `body` that belongs to the
// value of the top-level key `key`. Returns empty string when key is missing.
// This scopes further searches inside the matched block — the previous version
// searched the entire JSON and could pick up `"name"` from a nested object.
std::string extractBlock(const std::string &body, const std::string &key) {
  const std::string pattern = "\"" + key + "\"";
  size_t pos = body.find(pattern);
  if (pos == std::string::npos) {
    return {};
  }
  pos = body.find(':', pos);
  if (pos == std::string::npos) {
    return {};
  }
  ++pos;
  while (pos < body.size() &&
         std::isspace(static_cast<unsigned char>(body[pos]))) {
    ++pos;
  }
  if (pos >= body.size()) {
    return {};
  }
  char open = body[pos];
  if (open == '[' || open == '{') {
    // Bracket-balanced slice.
    char close = (open == '[') ? ']' : '}';
    int depth = 0;
    bool inString = false;
    bool escape = false;
    size_t start = pos;
    for (size_t i = pos; i < body.size(); ++i) {
      char ch = body[i];
      if (inString) {
        if (escape) {
          escape = false;
          continue;
        }
        if (ch == '\\') {
          escape = true;
          continue;
        }
        if (ch == '"')
          inString = false;
      } else {
        if (ch == '"')
          inString = true;
        else if (ch == open)
          ++depth;
        else if (ch == close) {
          --depth;
          if (depth == 0) {
            return body.substr(start, i - start + 1);
          }
        }
      }
    }
    return body.substr(start);
  }
  if (open == '"') {
    std::string result;
    ++pos;
    bool escape = false;
    while (pos < body.size()) {
      char ch = body[pos++];
      if (escape) {
        if (ch == 'n')
          result += '\n';
        else if (ch == 't')
          result += '\t';
        else if (ch == 'r')
          result += '\r';
        else
          result += ch;
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        return result;
      } else {
        result += ch;
      }
    }
  }
  return {};
}

std::string extractStringField(const std::string &body,
                               const std::string &key) {
  return extractBlock(body, key);
}

int extractIntField(const std::string &body, const std::string &key,
                    int fallback = 0) {
  const std::string value = extractBlock(body, key);
  if (value.empty())
    return fallback;
  try {
    return std::stoi(trim(value));
  } catch (...) {
    return fallback;
  }
}

float extractFloatField(const std::string &body, const std::string &key,
                        float fallback = 0.0f) {
  const std::string value = extractBlock(body, key);
  if (value.empty())
    return fallback;
  try {
    return std::stof(trim(value));
  } catch (...) {
    return fallback;
  }
}

bool extractBoolField(const std::string &body, const std::string &key,
                      bool fallback = false) {
  const std::string value = trim(extractBlock(body, key));
  if (value == "true")
    return true;
  if (value == "false")
    return false;
  return fallback;
}

glm::vec3 extractVec3Field(const std::string &body, const std::string &key,
                           const glm::vec3 &fallback = {}) {
  const std::string value = extractBlock(body, key);
  if (value.empty() || value.front() != '[')
    return fallback;
  std::string stripped = value.substr(1, value.size() - 2);
  std::replace(stripped.begin(), stripped.end(), ',', ' ');
  std::istringstream stream(stripped);
  glm::vec3 result = fallback;
  if (!(stream >> result.x >> result.y >> result.z)) {
    return fallback;
  }
  return result;
}

glm::quat extractQuatField(const std::string &body, const std::string &key,
                           const glm::quat &fallback = {}) {
  const std::string value = extractBlock(body, key);
  if (value.empty() || value.front() != '[')
    return fallback;
  std::string stripped = value.substr(1, value.size() - 2);
  std::replace(stripped.begin(), stripped.end(), ',', ' ');
  std::istringstream stream(stripped);
  glm::quat result = fallback;
  if (!(stream >> result.x >> result.y >> result.z >> result.w)) {
    return fallback;
  }
  return result;
}

// Splits the body of a JSON array (without the surrounding brackets) into a
// list of inner objects/arrays, respecting brace depth and string literals.
std::vector<std::string> splitTopLevelBlocks(const std::string &arrayContent) {
  std::vector<std::string> items;
  int depth = 0;
  bool inString = false;
  bool escape = false;
  size_t start = std::string::npos;
  for (size_t i = 0; i < arrayContent.size(); ++i) {
    char ch = arrayContent[i];
    if (inString) {
      if (escape) {
        escape = false;
        continue;
      }
      if (ch == '\\') {
        escape = true;
        continue;
      }
      if (ch == '"')
        inString = false;
      continue;
    }
    if (ch == '"') {
      inString = true;
      continue;
    }
    if (ch == '{' || ch == '[') {
      if (depth == 0)
        start = i;
      ++depth;
    } else if (ch == '}' || ch == ']') {
      --depth;
      if (depth == 0 && start != std::string::npos) {
        items.push_back(arrayContent.substr(start, i - start + 1));
        start = std::string::npos;
      }
    }
  }
  return items;
}

MapObject parseObject(const std::string &json) {
  MapObject object;
  object.id = extractStringField(json, "id");
  const std::string typeStr = extractStringField(json, "type");
  object.type = MapData::typeFromString(typeStr);
  object.position = extractVec3Field(json, "position");
  object.rotation = extractQuatField(json, "rotation");
  object.row = extractIntField(json, "row", 1);
  object.subtype = extractIntField(json, "subtype", 0);
  object.modelPath = extractStringField(json, "model");
  object.radius = extractFloatField(json, "radius", 10.0f);
  object.isFinishLine = extractBoolField(json, "is_finish", false);
  object.health = extractFloatField(json, "health", 50.0f);
  return object;
}

TrackPoint parseTrackPoint(const std::string &json) {
  TrackPoint point;
  point.position = extractVec3Field(json, "position", glm::vec3(0.0f));
  point.width = extractFloatField(json, "width", 12.0f);
  point.isFinish = extractBoolField(json, "is_finish", false);
  point.banking = extractFloatField(json, "banking", 0.0f);
  return point;
}

void writeVec3(std::ostringstream &out, const glm::vec3 &value) {
  out << std::fixed << std::setprecision(3) << "[" << value.x << ", " << value.y
      << ", " << value.z << "]";
}

void writeQuat(std::ostringstream &out, const glm::quat &value) {
  out << std::fixed << std::setprecision(4) << "[" << value.x << ", " << value.y
      << ", " << value.z << ", " << value.w << "]";
}

void writeBool(std::ostringstream &out, bool value) {
  out << (value ? "true" : "false");
}

} // namespace

void MapData::createDefault(uint32_t &idCounter) {
  name = "New Map";
  version = CURRENT_VERSION;
  author = "editor";
  mode = MapMode::Both;
  lapCount = 3;
  terrainMesh.clear();
  boundaryMin = glm::vec3(-500.0f, -50.0f, -500.0f);
  boundaryMax = glm::vec3(500.0f, 200.0f, 500.0f);
  objects.clear();
  trackPoints.clear();
  trackClosed = false;
  trackHeight = 0.0f;
  isApproved = false;
  moderationStatus = "pending";

  // Default oval: 8 points forming a closed ring 40m wide.
  trackPoints.push_back(TrackPoint(glm::vec3(40.0f, 0.0f, 0.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(28.0f, 0.0f, 28.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(0.0f, 0.0f, 40.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(-28.0f, 0.0f, 28.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(-40.0f, 0.0f, 0.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(-28.0f, 0.0f, -28.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(0.0f, 0.0f, -40.0f), 14.0f));
  trackPoints.push_back(TrackPoint(glm::vec3(28.0f, 0.0f, -28.0f), 14.0f));
  trackClosed = true;

  const float rowSpacing = 8.0f;
  const float columnSpacing = 6.0f;
  for (int row = 1; row <= 2; ++row) {
    for (int column = 0; column < 4; ++column) {
      MapObject spawn;
      spawn.id = generateObjectId(MapObjectType::SpawnPoint, idCounter);
      spawn.type = MapObjectType::SpawnPoint;
      spawn.row = row;
      spawn.position = glm::vec3(-9.0f + column * columnSpacing, 0.0f,
                                 row == 1 ? -5.0f : 5.0f);
      objects.push_back(spawn);
    }
  }

  MapObject checkpoint;
  checkpoint.id = generateObjectId(MapObjectType::Checkpoint, idCounter);
  checkpoint.type = MapObjectType::Checkpoint;
  checkpoint.position = glm::vec3(40.0f, 0.0f, 0.0f);
  checkpoint.radius = 12.0f;
  checkpoint.isFinishLine = true;
  objects.push_back(checkpoint);
}

void MapData::addObject(const MapObject &object) { objects.push_back(object); }

bool MapData::removeObject(const std::string &id) {
  auto it =
      std::find_if(objects.begin(), objects.end(),
                   [&](const MapObject &object) { return object.id == id; });
  if (it == objects.end()) {
    return false;
  }
  objects.erase(it);
  return true;
}

MapObject *MapData::findObject(const std::string &id) {
  auto it = std::find_if(objects.begin(), objects.end(),
                         [&](MapObject &object) { return object.id == id; });
  return it != objects.end() ? &(*it) : nullptr;
}

const MapObject *MapData::findObject(const std::string &id) const {
  auto it =
      std::find_if(objects.begin(), objects.end(),
                   [&](const MapObject &object) { return object.id == id; });
  return it != objects.end() ? &(*it) : nullptr;
}

std::string MapData::generateObjectId(MapObjectType type,
                                      uint32_t &counter) const {
  static const char *prefixes[] = {"spawn",        "checkpoint", "powerup",
                                   "destructible", "obstacle",   "decoration"};
  int index = static_cast<int>(type);
  if (index < 0 || index >= 6) {
    index = 5;
  }
  return prefixes[index] + std::string("_") + std::to_string(counter++);
}

std::string MapData::typeToString(MapObjectType type) {
  switch (type) {
  case MapObjectType::SpawnPoint:
    return "spawn_point";
  case MapObjectType::Checkpoint:
    return "checkpoint";
  case MapObjectType::PowerUpSpawn:
    return "powerup_spawn";
  case MapObjectType::Destructible:
    return "destructible";
  case MapObjectType::Obstacle:
    return "obstacle";
  case MapObjectType::Decoration:
    return "decoration";
  default:
    return "decoration";
  }
}

MapObjectType MapData::typeFromString(const std::string &value) {
  if (value == "spawn_point")
    return MapObjectType::SpawnPoint;
  if (value == "checkpoint")
    return MapObjectType::Checkpoint;
  if (value == "powerup_spawn")
    return MapObjectType::PowerUpSpawn;
  if (value == "destructible")
    return MapObjectType::Destructible;
  if (value == "obstacle")
    return MapObjectType::Obstacle;
  return MapObjectType::Decoration;
}

bool MapData::saveToFile(const std::string &path) const {
  std::ostringstream out;
  out << "{\n";
  out << "  \"name\": \"" << escapeJson(name) << "\",\n";
  out << "  \"version\": " << version << ",\n";
  out << "  \"author\": \"" << escapeJson(author) << "\",\n";
  out << "  \"mode\": \"" << mapModeToString(mode) << "\",\n";
  out << "  \"lap_count\": " << lapCount << ",\n";
  out << "  \"terrain\": {\n";
  out << "    \"mesh\": \"" << escapeJson(terrainMesh) << "\",\n";
  out << "    \"collision\": true\n";
  out << "  },\n";
  out << "  \"boundaries\": {\n";
  out << "    \"min\": ";
  writeVec3(out, boundaryMin);
  out << ",\n    \"max\": ";
  writeVec3(out, boundaryMax);
  out << "\n  },\n";
  out << "  \"track\": {\n";
  out << "    \"closed\": ";
  writeBool(out, trackClosed);
  out << ",\n    \"height\": " << trackHeight << ",\n";
  out << "    \"points\": [";
  for (size_t i = 0; i < trackPoints.size(); ++i) {
    const TrackPoint &p = trackPoints[i];
    out << (i == 0 ? "\n" : ",\n");
    out << "      {\n";
    out << "        \"position\": ";
    writeVec3(out, p.position);
    out << ",\n        \"width\": " << p.width;
    out << ",\n        \"is_finish\": ";
    writeBool(out, p.isFinish);
    out << ",\n        \"banking\": " << p.banking << "\n";
    out << "      }";
  }
  if (!trackPoints.empty())
    out << "\n    ";
  out << "]";
  out << "\n  },\n";
  out << "  \"moderation\": {\n";
  out << "    \"status\": \"" << escapeJson(moderationStatus) << "\",\n";
  out << "    \"approved\": ";
  writeBool(out, isApproved);
  out << "\n  },\n";
  out << "  \"objects\": [\n";

  for (size_t i = 0; i < objects.size(); ++i) {
    const MapObject &object = objects[i];
    out << "    {\n";
    out << "      \"id\": \"" << escapeJson(object.id) << "\",\n";
    out << "      \"type\": \"" << typeToString(object.type) << "\",\n";
    out << "      \"position\": ";
    writeVec3(out, object.position);
    out << ",\n      \"rotation\": ";
    writeQuat(out, object.rotation);
    out << ",\n      \"row\": " << object.row << ",\n";
    out << "      \"subtype\": " << object.subtype << ",\n";
    out << "      \"model\": \"" << escapeJson(object.modelPath) << "\",\n";
    out << "      \"radius\": " << object.radius << ",\n";
    out << "      \"is_finish\": ";
    writeBool(out, object.isFinishLine);
    out << ",\n      \"health\": " << object.health << "\n";
    out << "    }";
    if (i + 1 < objects.size()) {
      out << ",";
    }
    out << "\n";
  }

  out << "  ]\n";
  out << "}\n";

  std::filesystem::path filePath(path);
  if (filePath.has_parent_path()) {
    std::filesystem::create_directories(filePath.parent_path());
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG_ERRORF("Failed to save map: %s", path.c_str());
    return false;
  }
  file << out.str();
  LOG_INFOF("Saved map to %s", path.c_str());
  return true;
}

bool MapData::loadFromFile(const std::string &path) {
  const std::string json = readFileToString(path);
  if (json.empty()) {
    LOG_ERRORF("Failed to read map file: %s", path.c_str());
    return false;
  }

  lapCount = extractIntField(json, "lap_count", 3);
  if (lapCount < 1)
    lapCount = 1;
  if (lapCount > 20)
    lapCount = 20;

  name = extractStringField(json, "name");
  if (name.empty()) {
    name = "Loaded Map";
  }
  version = extractIntField(json, "version", CURRENT_VERSION);
  author = extractStringField(json, "author");
  {
    const std::string modeStr = extractStringField(json, "mode");
    mode = mapModeFromString(modeStr.empty() ? "both" : modeStr);
  }

  const std::string terrainBlock = extractBlock(json, "terrain");
  if (!terrainBlock.empty()) {
    terrainMesh = extractStringField(terrainBlock, "mesh");
  }

  const std::string boundariesBlock = extractBlock(json, "boundaries");
  if (!boundariesBlock.empty()) {
    boundaryMin = extractVec3Field(boundariesBlock, "min", boundaryMin);
    boundaryMax = extractVec3Field(boundariesBlock, "max", boundaryMax);
  }

  trackPoints.clear();
  const std::string trackBlock = extractBlock(json, "track");
  if (!trackBlock.empty()) {
    trackClosed = extractBoolField(trackBlock, "closed", false);
    trackHeight = extractFloatField(trackBlock, "height", 0.0f);
    const std::string pointsBlock = extractBlock(trackBlock, "points");
    if (!pointsBlock.empty() && pointsBlock.front() == '[') {
      const std::string inner = pointsBlock.substr(1, pointsBlock.size() - 2);
      for (const std::string &item : splitTopLevelBlocks(inner)) {
        trackPoints.push_back(parseTrackPoint(item));
      }
    }
  }

  objects.clear();
  const std::string moderationBlock = extractBlock(json, "moderation");
  if (!moderationBlock.empty()) {
    moderationStatus = extractStringField(moderationBlock, "status");
    if (moderationStatus != "pending" && moderationStatus != "approved" &&
        moderationStatus != "rejected") {
      moderationStatus = "pending";
    }
    isApproved = extractBoolField(moderationBlock, "approved", false);
    // Self-heal: keep isApproved consistent with the string flag.
    isApproved = (moderationStatus == "approved");
  } else if (version < 2) {
    // Pre-v2 maps never carried moderation fields; treat as pending.
    isApproved = false;
    moderationStatus = "pending";
  }

  const std::string objectsBlock = extractBlock(json, "objects");
  if (!objectsBlock.empty() && objectsBlock.front() == '[') {
    const std::string inner = objectsBlock.substr(1, objectsBlock.size() - 2);
    for (const std::string &item : splitTopLevelBlocks(inner)) {
      MapObject object = parseObject(item);
      if (!object.id.empty()) {
        objects.push_back(object);
      }
    }
  }

  LOG_INFOF("Loaded map '%s' with %zu objects and %zu track points from %s",
            name.c_str(), objects.size(), trackPoints.size(), path.c_str());
  return true;
}

} // namespace Editor