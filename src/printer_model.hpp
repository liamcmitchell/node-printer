#ifndef PRINTER_MODEL_HPP
#define PRINTER_MODEL_HPP

#include <napi.h>

#include <map>
#include <string>
#include <vector>

namespace printer_model {

enum class RawValueType {
  String,
  Number,
  StringArray,
};

struct RawValue {
  RawValueType type = RawValueType::String;
  std::string stringValue;
  double numberValue = 0;
  std::vector<std::string> stringArrayValue;

  static RawValue FromString(std::string value);
  static RawValue FromNumber(double value);
  static RawValue FromStringArray(std::vector<std::string> value);
};

using RawMap = std::map<std::string, RawValue>;

struct JobModel {
  int id = 0;
  std::string name;
  std::string printerName;
  std::string user;
  std::string state = "pending";
  double size = 0;
  double createdAt = 0;
  double processingAt = 0;
  double completedAt = 0;
  RawMap raw;
};

struct PrinterModel {
  std::string name;
  bool isDefault = false;
  std::string state = "idle";
  std::vector<std::string> stateReasons;
  std::vector<JobModel> jobs;
  RawMap raw;
};

Napi::Value SerializeRawValue(Napi::Env env, const RawValue &value);
Napi::Object SerializeJobModel(Napi::Env env, const JobModel &job);
Napi::Object SerializePrinterModel(Napi::Env env, const PrinterModel &printer);

} // namespace printer_model

#endif
