#include "common/json/json_loader.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "spdlog/spdlog.h"

// Do not let RapidJson leak outside of this file.
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/reader.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace Json {

/**
 * Implementation of Object.
 */
class ObjectImplBase : public Object {
public:
  ObjectImplBase(const rapidjson::Value& value, const std::string& name)
      : name_(name), value_(value) {}

  std::vector<ObjectPtr> asObjectArray() const override {
    if (!value_.IsArray()) {
      throw Exception(fmt::format("'{}' is not an array", name_));
    }

    std::vector<ObjectPtr> object_array;
    object_array.reserve(value_.Size());
    for (auto& array_value : value_.GetArray()) {
      object_array.emplace_back(new ObjectImplBase(array_value, name_ + " (array item)"));
    }
    return object_array;
  }

  bool getBoolean(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsBool()) {
      throw Exception(fmt::format("key '{}' missing or not a boolean in '{}'", name, name_));
    }
    return member_itr->value.GetBool();
  }

  bool getBoolean(const std::string& name, bool default_value) const override {
    if (!value_.HasMember(name.c_str())) {
      return default_value;
    } else {
      return getBoolean(name);
    }
  }

  int64_t getInteger(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsInt64()) {
      throw Exception(fmt::format("key '{}' missing or not an integer in '{}'", name, name_));
    }
    return member_itr->value.GetInt64();
  }

  int64_t getInteger(const std::string& name, int64_t default_value) const override {
    if (!value_.HasMember(name.c_str())) {
      return default_value;
    } else {
      return getInteger(name);
    }
  }

  ObjectPtr getObject(const std::string& name, bool allow_empty) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() && !allow_empty) {
      throw Exception(fmt::format("key '{}' missing or not an integer in '{}'", name, name_));
    } else if (member_itr == value_.MemberEnd()) {
      return ObjectPtr{new ObjectImplBase(empty_rapid_json_value_, name)};
    }
    return ObjectPtr{new ObjectImplBase(member_itr->value, name)};
  }

  std::vector<ObjectPtr> getObjectArray(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsArray()) {
      throw Exception(fmt::format("key '{}' missing or not a array in '{}'", name, name_));
    }

    std::vector<ObjectPtr> object_array;
    object_array.reserve(member_itr->value.Size());
    for (auto& array_value : member_itr->value.GetArray()) {
      object_array.emplace_back(new ObjectImplBase(array_value, name + " (array item)"));
    }
    return object_array;
  }

  std::string getString(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsString()) {
      throw Exception(fmt::format("key '{}' missing or not a string in '{}'", name, name_));
    }
    return member_itr->value.GetString();
  }

  std::string getString(const std::string& name, const std::string& default_value) const override {
    if (!value_.HasMember(name.c_str())) {
      return default_value;
    } else {
      return getString(name);
    }
  }

  std::vector<std::string> getStringArray(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsArray()) {
      throw Exception(fmt::format("key '{}' missing or not an array in '{}'", name, name_));
    }

    std::vector<std::string> string_array;
    string_array.reserve(member_itr->value.Size());
    for (auto& array_value : member_itr->value.GetArray()) {
      if (!array_value.IsString()) {
        throw Exception(fmt::format("array '{}' does not contain all strings", name));
      }
      string_array.push_back(array_value.GetString());
    }
    return string_array;
  }

  double getDouble(const std::string& name) const override {
    rapidjson::Value::ConstMemberIterator member_itr = value_.FindMember(name.c_str());
    if (member_itr == value_.MemberEnd() || !member_itr->value.IsDouble()) {
      throw Exception(fmt::format("key '{}' missing or not a double in '{}'", name, name_));
    }
    return member_itr->value.GetDouble();
  }

  double getDouble(const std::string& name, double default_value) const override {
    if (!value_.HasMember(name.c_str())) {
      return default_value;
    } else {
      return getDouble(name);
    }
  }

  uint64_t hash() const override {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value_.Accept(writer);
    return std::hash<std::string>{}(buffer.GetString());
  }

  void iterate(const ObjectCallback& callback) const override {
    for (auto& member : value_.GetObject()) {
      ObjectImplBase object(member.value, member.name.GetString());
      bool need_continue = callback(member.name.GetString(), object);
      if (!need_continue) {
        break;
      }
    }
  }

  bool hasObject(const std::string& name) const override { return value_.HasMember(name.c_str()); }

  void validateSchema(const std::string& schema) const override {
    rapidjson::Document document;
    if (document.Parse<0>(schema.c_str()).HasParseError()) {
      throw std::invalid_argument(fmt::format("invalid schema \n Error(offset {}) : {}\n",
                                              document.GetErrorOffset(),
                                              GetParseError_En(document.GetParseError())));
    }

    rapidjson::SchemaDocument schema_document(document);
    rapidjson::SchemaValidator schema_validator(schema_document);

    if (!value_.Accept(schema_validator)) {
      // TODO(mattklein123): Improve errors by switching to SAX API.
      rapidjson::StringBuffer schema_string_buffer;
      rapidjson::StringBuffer document_string_buffer;

      schema_validator.GetInvalidSchemaPointer().StringifyUriFragment(schema_string_buffer);
      schema_validator.GetInvalidDocumentPointer().StringifyUriFragment(document_string_buffer);

      throw Exception(fmt::format(
          "JSON object doesn't conform to schema.\n Invalid schema: {}.\n Invalid keyword: "
          "{}.\n Invalid document key: {}",
          schema_string_buffer.GetString(), schema_validator.GetInvalidSchemaKeyword(),
          document_string_buffer.GetString()));
    }
  }

  std::string asString() const override {
    if (!value_.IsString()) {
      throw Exception(fmt::format("'{}' is not a string", name_));
    }
    return value_.GetString();
  }

  bool empty() const override { return value_.IsObject() && value_.ObjectEmpty(); }

private:
  const std::string name_;
  const rapidjson::Value& value_;

  static const rapidjson::Value empty_rapid_json_value_;
};

const rapidjson::Value ObjectImplBase::empty_rapid_json_value_(rapidjson::kObjectType);

/**
 * Holds the root Object reference.
 */
class ObjectImplRoot : public ObjectImplBase {
public:
  ObjectImplRoot(rapidjson::Document&& document)
      : ObjectImplBase(root_, "root"), root_(std::move(document)) {}

private:
  rapidjson::Document root_;
};

ObjectPtr Factory::LoadFromFile(const std::string& file_path) {
  rapidjson::Document document;
  std::ifstream file_stream(file_path);
  rapidjson::IStreamWrapper stream_wrapper(file_stream);
  if (document.ParseStream(stream_wrapper).HasParseError()) {
    throw Exception(fmt::format("Error(offset {}): {}\n", document.GetErrorOffset(),
                                GetParseError_En(document.GetParseError())));
  }
  return ObjectPtr{new ObjectImplRoot(std::move(document))};
}

struct PrintHandler {
    // Using for testing
    bool Null() { printf("Null\n"); return true; }
    bool Bool(bool) { printf("Bool\n"); return true; }
    bool Int(int) { printf("Int\n"); return true; }
    bool Uint(unsigned) { printf("Uint\n"); return true; }
    bool Int64(int64_t) { printf("Int64\n"); return true; }
    bool Uint64(uint64_t) { printf("Uint64\n"); return true; }
    bool Double(double) { printf("Double\n"); return true; }
    bool RawNumber(const char*, rapidjson::SizeType, bool) { printf("RawNumber\n"); return true; }
    bool String(const char*, rapidjson::SizeType, bool) { printf("String\n"); return true; }
    bool StartObject() { printf("StartObject"); return true; }
    bool Key(const char*, rapidjson::SizeType, bool) { printf("Key\n"); return true; }
    bool EndObject(rapidjson::SizeType) { printf("EndObject\n"); return true; }
    bool StartArray() {  printf("StartArray\n"); return true; }
    bool EndArray(rapidjson::SizeType) {  printf("EndArray\n"); return true; }
};

ObjectPtr Factory::LoadFromString(const std::string& json) {

  PrintHandler handler;

  rapidjson::Reader reader;
  reader.IterativeParseInit();

  rapidjson::StringStream json_stream(json.c_str());
  while (!reader.IterativeParseComplete()) {
      if (!reader.IterativeParseNext<rapidjson::kParseDefaultFlags>(json_stream, handler)) {
          // throw parse error
          break;
      }
  }

  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    printf("Parse error");
    throw Exception(fmt::format("Error(offset {}): {}\n", document.GetErrorOffset(),
                                GetParseError_En(document.GetParseError())));
  }

  return ObjectPtr{new ObjectImplRoot(std::move(document))};
}

} // Json
