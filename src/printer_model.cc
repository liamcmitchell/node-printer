#include "printer_model.hpp"

namespace printer_model {

RawValue RawValue::FromString(std::string value) {
  RawValue out;
  out.type = RawValueType::String;
  out.stringValue = std::move(value);
  return out;
}

RawValue RawValue::FromNumber(double value) {
  RawValue out;
  out.type = RawValueType::Number;
  out.numberValue = value;
  return out;
}

RawValue RawValue::FromStringArray(std::vector<std::string> value) {
  RawValue out;
  out.type = RawValueType::StringArray;
  out.stringArrayValue = std::move(value);
  return out;
}

Napi::Value SerializeRawValue(Napi::Env env, const RawValue &value) {
  switch (value.type) {
  case RawValueType::Number:
    return Napi::Number::New(env, value.numberValue);
  case RawValueType::StringArray: {
    Napi::Array arr = Napi::Array::New(env, value.stringArrayValue.size());
    for (size_t i = 0; i < value.stringArrayValue.size(); ++i) {
      arr.Set(static_cast<uint32_t>(i),
              Napi::String::New(env, value.stringArrayValue[i]));
    }
    return arr;
  }
  case RawValueType::String:
  default:
    return Napi::String::New(env, value.stringValue);
  }
}

Napi::Object SerializeJobModel(Napi::Env env, const JobModel &job) {
  Napi::Object out = Napi::Object::New(env);
  out.Set("id", Napi::Number::New(env, job.id));
  out.Set("name", Napi::String::New(env, job.name));
  out.Set("printerName", Napi::String::New(env, job.printerName));
  out.Set("user", Napi::String::New(env, job.user));
  out.Set("state", Napi::String::New(env, job.state));
  out.Set("size", Napi::Number::New(env, job.size));
  out.Set("createdAt", Napi::Number::New(env, job.createdAt));
  out.Set("processingAt", Napi::Number::New(env, job.processingAt));
  out.Set("completedAt", Napi::Number::New(env, job.completedAt));

  Napi::Object raw = Napi::Object::New(env);
  for (const auto &entry : job.raw) {
    raw.Set(entry.first, SerializeRawValue(env, entry.second));
  }
  out.Set("raw", raw);
  return out;
}

Napi::Object SerializePrinterModel(Napi::Env env, const PrinterModel &printer) {
  Napi::Object out = Napi::Object::New(env);
  out.Set("name", Napi::String::New(env, printer.name));
  out.Set("isDefault", Napi::Boolean::New(env, printer.isDefault));
  out.Set("state", Napi::String::New(env, printer.state));

  Napi::Array stateReasons = Napi::Array::New(env, printer.stateReasons.size());
  for (size_t i = 0; i < printer.stateReasons.size(); ++i) {
    stateReasons.Set(static_cast<uint32_t>(i),
                     Napi::String::New(env, printer.stateReasons[i]));
  }
  out.Set("stateReasons", stateReasons);

  Napi::Array jobs = Napi::Array::New(env, printer.jobs.size());
  for (size_t i = 0; i < printer.jobs.size(); ++i) {
    jobs.Set(static_cast<uint32_t>(i), SerializeJobModel(env, printer.jobs[i]));
  }
  out.Set("jobs", jobs);

  Napi::Object raw = Napi::Object::New(env);
  for (const auto &entry : printer.raw) {
    raw.Set(entry.first, SerializeRawValue(env, entry.second));
  }
  out.Set("raw", raw);
  return out;
}

} // namespace printer_model
