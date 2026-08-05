#include "node_printer.hpp"
#include "printer_model.hpp"

#include <map>
#include <node_version.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cups/cups.h>

namespace {
typedef std::map<std::string, int> StatusMapType;
typedef std::map<std::string, std::string> FormatMapType;

const StatusMapType &getJobStatusMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
#define STATUS_PRINTER_ADD(value, type)                                        \
  result.insert(std::make_pair(value, type))
  STATUS_PRINTER_ADD("pending", IPP_JOB_PENDING);
  STATUS_PRINTER_ADD("pending-held", IPP_JOB_HELD);
  STATUS_PRINTER_ADD("processing", IPP_JOB_PROCESSING);
  STATUS_PRINTER_ADD("processing-stopped", IPP_JOB_STOPPED);
  STATUS_PRINTER_ADD("canceled", IPP_JOB_CANCELLED);
  STATUS_PRINTER_ADD("aborted", IPP_JOB_ABORTED);
  STATUS_PRINTER_ADD("completed", IPP_JOB_COMPLETED);
#undef STATUS_PRINTER_ADD
  return result;
}

const FormatMapType &getPrinterFormatMap() {
  static FormatMapType result;
  if (!result.empty()) {
    return result;
  }
  result.insert(std::make_pair("RAW", CUPS_FORMAT_RAW));
  result.insert(std::make_pair("TEXT", CUPS_FORMAT_TEXT));
#ifdef CUPS_FORMAT_PDF
  result.insert(std::make_pair("PDF", CUPS_FORMAT_PDF));
#endif
#ifdef CUPS_FORMAT_JPEG
  result.insert(std::make_pair("JPEG", CUPS_FORMAT_JPEG));
#endif
#ifdef CUPS_FORMAT_POSTSCRIPT
  result.insert(std::make_pair("POSTSCRIPT", CUPS_FORMAT_POSTSCRIPT));
#endif
#ifdef CUPS_FORMAT_COMMAND
  result.insert(std::make_pair("COMMAND", CUPS_FORMAT_COMMAND));
#endif
#ifdef CUPS_FORMAT_AUTO
  result.insert(std::make_pair("AUTO", CUPS_FORMAT_AUTO));
#endif
  return result;
}

std::string getJobStateString(int stateCode) {
  for (auto &entry : getJobStatusMap()) {
    if (stateCode == entry.second) {
      return entry.first;
    }
  }
  return "pending";
}

printer_model::JobModel toJobData(const cups_job_t *job) {
  printer_model::JobModel out;
  out.id = job->id;
  out.name = job->title ? job->title : "";
  out.printerName = job->dest ? job->dest : "";
  out.user = job->user ? job->user : "";
  out.size = job->size;
  out.state = getJobStateString(job->state);
  out.createdAt = static_cast<double>(job->creation_time);
  out.processingAt = static_cast<double>(job->processing_time);
  out.completedAt = static_cast<double>(job->completed_time);
  out.raw["format"] =
      printer_model::RawValue::FromString(job->format ? job->format : "");
  out.raw["priority"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->priority));
  out.raw["stateCode"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->state));
  return out;
}

printer_model::PrinterModel toPrinterData(const cups_dest_t *printer,
                                          bool includeJobs) {
  printer_model::PrinterModel out;
  out.name = printer->name ? printer->name : "";
  out.isDefault = static_cast<bool>(printer->is_default);

  const char *state_val =
      cupsGetOption("printer-state", printer->num_options, printer->options);
  if (state_val) {
    switch (state_val[0]) {
    case '4':
      out.state = "processing";
      break;
    case '5':
      out.state = "stopped";
      break;
    default:
      break;
    }
  }

  const char *reasons_val = cupsGetOption(
      "printer-state-reasons", printer->num_options, printer->options);
  if (reasons_val) {
    std::string reasons(reasons_val);
    size_t pos = 0;
    while (pos < reasons.size()) {
      size_t comma = reasons.find(',', pos);
      if (comma == std::string::npos) {
        comma = reasons.size();
      }
      std::string reason = reasons.substr(pos, comma - pos);
      size_t dash = reason.rfind('-');
      if (dash != std::string::npos) {
        std::string suffix = reason.substr(dash);
        if (suffix == "-report" || suffix == "-warning" || suffix == "-error") {
          reason = reason.substr(0, dash);
        }
      }
      out.stateReasons.push_back(reason);
      pos = comma + 1;
    }
  }

  cups_option_t *dest_option = printer->options;
  for (int j = 0; j < printer->num_options; ++j, ++dest_option) {
    out.raw[dest_option->name ? dest_option->name : ""] =
        printer_model::RawValue::FromString(
            dest_option->value ? dest_option->value : "");
  }
  if (printer->instance) {
    out.raw["instance"] =
        printer_model::RawValue::FromString(printer->instance);
  }

  if (includeJobs) {
    cups_job_t *jobs = nullptr;
    int totalJobs = cupsGetJobs(&jobs, printer->name, 0, CUPS_WHICHJOBS_ACTIVE);
    for (int i = 0; i < totalJobs; ++i) {
      out.jobs.push_back(toJobData(&jobs[i]));
    }
    cupsFreeJobs(totalJobs, jobs);
  }

  return out;
}

class GetAllPrinterDetailsWorker : public PromiseWorker {
public:
  explicit GetAllPrinterDetailsWorker(Napi::Env env) : PromiseWorker(env) {}

  void Execute() override {
    cups_dest_t *printers = nullptr;
    int printers_size = cupsGetDests(&printers);
    for (int i = 0; i < printers_size; ++i) {
      result_.push_back(toPrinterData(&printers[i], true));
    }
    cupsFreeDests(printers_size, printers);
  }

  void OnOK() override {
    Napi::Env env = Env();
    Napi::Array arr = Napi::Array::New(env, result_.size());
    for (size_t i = 0; i < result_.size(); ++i) {
      arr.Set(static_cast<uint32_t>(i),
              printer_model::SerializePrinterModel(env, result_[i]));
    }
    deferred_.Resolve(arr);
  }

private:
  std::vector<printer_model::PrinterModel> result_;
};

class GetPrinterDetailsWorker : public PromiseWorker {
public:
  GetPrinterDetailsWorker(Napi::Env env, std::string printerName)
      : PromiseWorker(env), printerName_(std::move(printerName)) {}

  void Execute() override {
    cups_dest_t *printers = nullptr;
    int printers_size = cupsGetDests(&printers);
    cups_dest_t *printer =
        cupsGetDest(printerName_.c_str(), NULL, printers_size, printers);
    if (printer != nullptr) {
      found_ = true;
      result_ = toPrinterData(printer, true);
    }
    cupsFreeDests(printers_size, printers);
  }

  void OnOK() override {
    if (!found_) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(printer_model::SerializePrinterModel(Env(), result_));
  }

private:
  std::string printerName_;
  bool found_ = false;
  printer_model::PrinterModel result_;
};

class HasPrinterWorker : public PromiseWorker {
public:
  HasPrinterWorker(Napi::Env env, std::string printerName)
      : PromiseWorker(env), printerName_(std::move(printerName)) {}

  void Execute() override {
    cups_dest_t *printers = nullptr;
    int printers_size = cupsGetDests(&printers);
    cups_dest_t *printer =
        cupsGetDest(printerName_.c_str(), NULL, printers_size, printers);
    found_ = (printer != nullptr);
    cupsFreeDests(printers_size, printers);
  }

  void OnOK() override { deferred_.Resolve(Napi::Boolean::New(Env(), found_)); }

private:
  std::string printerName_;
  bool found_ = false;
};

class GetDefaultPrinterNameWorker : public PromiseWorker {
public:
  explicit GetDefaultPrinterNameWorker(Napi::Env env) : PromiseWorker(env) {}

  void Execute() override {
    cups_dest_t *printers = nullptr;
    int printers_size = cupsGetDests(&printers);
    for (int i = 0; i < printers_size; ++i) {
      if (printers[i].is_default) {
        found_ = true;
        name_ = printers[i].name ? printers[i].name : "";
        break;
      }
    }
    cupsFreeDests(printers_size, printers);
  }

  void OnOK() override {
    if (!found_) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(Napi::String::New(Env(), name_));
  }

private:
  bool found_ = false;
  std::string name_;
};

/// cups option class to automatically free memory.
/// Built from plain C++ data so it can safely be used from a worker thread
/// (Napi::Object must only be touched on the main thread).
class CupsOptions {
protected:
  cups_option_t *_value = nullptr;
  int num_options = 0;

public:
  CupsOptions() = default;
  ~CupsOptions() {
    if (_value != nullptr) {
      cupsFreeOptions(num_options, _value);
    }
  }

  CupsOptions(const CupsOptions &) = delete;
  CupsOptions &operator=(const CupsOptions &) = delete;

  explicit CupsOptions(
      const std::vector<std::pair<std::string, std::string>> &options) {
    for (const auto &entry : options) {
      num_options = cupsAddOption(entry.first.c_str(), entry.second.c_str(),
                                  num_options, &_value);
    }
  }

  const int &getNumOptions() { return num_options; }
  cups_option_t *get() { return _value; }
};

/// Extract a JS options object into plain C++ data on the main thread so it
/// can be handed off to an AsyncWorker.
std::vector<std::pair<std::string, std::string>>
extractOptions(Napi::Object iV8Options) {
  std::vector<std::pair<std::string, std::string>> result;
  Napi::Array props = iV8Options.GetPropertyNames();
  for (uint32_t i = 0; i < props.Length(); ++i) {
    Napi::Value key = props.Get(i);
    std::string keyStr = key.As<Napi::String>().Utf8Value();
    std::string valStr = iV8Options.Get(key).As<Napi::String>().Utf8Value();
    result.emplace_back(std::move(keyStr), std::move(valStr));
  }
  return result;
}

class GetJobWorker : public PromiseWorker {
public:
  GetJobWorker(Napi::Env env, std::string printerName, int jobId)
      : PromiseWorker(env), printerName_(std::move(printerName)),
        jobId_(jobId) {}

  void Execute() override {
    cups_job_t *jobs = nullptr;
    int totalJobs = cupsGetJobs(&jobs, printerName_.c_str(),
                                0 /*0 means all users*/, CUPS_WHICHJOBS_ALL);
    for (int i = 0; i < totalJobs; ++i) {
      if (jobs[i].id == jobId_) {
        found_ = true;
        result_ = toJobData(&jobs[i]);
        break;
      }
    }
    cupsFreeJobs(totalJobs, jobs);
  }

  void OnOK() override {
    if (!found_) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(printer_model::SerializeJobModel(Env(), result_));
  }

private:
  std::string printerName_;
  int jobId_;
  bool found_ = false;
  printer_model::JobModel result_;
};

class CancelJobWorker : public PromiseWorker {
public:
  CancelJobWorker(Napi::Env env, std::string printerName, int jobId)
      : PromiseWorker(env), printerName_(std::move(printerName)),
        jobId_(jobId) {}

  void Execute() override {
    // Ignore return value: cupsCancelJob returns 0 if the job no longer
    // exists, which we treat as a no-op.
    cupsCancelJob(printerName_.c_str(), jobId_);
  }

  void OnOK() override { deferred_.Resolve(Env().Undefined()); }

private:
  std::string printerName_;
  int jobId_;
};

class PrintDirectWorker : public PromiseWorker {
public:
  PrintDirectWorker(Napi::Env env, std::string data, std::string printerName,
                    std::string docName, std::string type,
                    std::vector<std::pair<std::string, std::string>> options)
      : PromiseWorker(env), data_(std::move(data)),
        printerName_(std::move(printerName)), docName_(std::move(docName)),
        type_(std::move(type)), options_(std::move(options)) {}

  void Execute() override {
    std::string type_str = type_;
    auto itFormat = getPrinterFormatMap().find(type_str);
    // Known aliases (RAW, PDF, etc.) are translated to MIME types.
    // Anything else is passed directly to CUPS as a MIME type — let CUPS
    // reject it if unsupported, matching the Windows spooler behaviour.
    if (itFormat != getPrinterFormatMap().end()) {
      type_str = itFormat->second;
    }

    CupsOptions options(options_);

    int job_id =
        cupsCreateJob(CUPS_HTTP_DEFAULT, printerName_.c_str(), docName_.c_str(),
                      options.getNumOptions(), options.get());
    if (job_id == 0) {
      SetError(cupsLastErrorString());
      return;
    }

    if (HTTP_CONTINUE != cupsStartDocument(CUPS_HTTP_DEFAULT,
                                           printerName_.c_str(), job_id,
                                           docName_.c_str(), type_str.c_str(),
                                           1 /*last document*/)) {
      SetError(cupsLastErrorString());
      return;
    }

    /* cupsWriteRequestData can be called as many times as needed */
    // TODO: to split big buffer
    if (HTTP_CONTINUE !=
        cupsWriteRequestData(CUPS_HTTP_DEFAULT, data_.c_str(), data_.size())) {
      cupsFinishDocument(CUPS_HTTP_DEFAULT, printerName_.c_str());
      SetError(cupsLastErrorString());
      return;
    }

    cupsFinishDocument(CUPS_HTTP_DEFAULT, printerName_.c_str());
    jobId_ = job_id;
  }

  void OnOK() override { deferred_.Resolve(Napi::Number::New(Env(), jobId_)); }

private:
  std::string data_;
  std::string printerName_;
  std::string docName_;
  std::string type_;
  std::vector<std::pair<std::string, std::string>> options_;
  int jobId_ = 0;
};

class PrintFileWorker : public PromiseWorker {
public:
  PrintFileWorker(Napi::Env env, std::string filename, std::string docName,
                  std::string printerName,
                  std::vector<std::pair<std::string, std::string>> options)
      : PromiseWorker(env), filename_(std::move(filename)),
        docName_(std::move(docName)), printerName_(std::move(printerName)),
        options_(std::move(options)) {}

  void Execute() override {
    CupsOptions options(options_);
    int job_id =
        cupsPrintFile(printerName_.c_str(), filename_.c_str(), docName_.c_str(),
                      options.getNumOptions(), options.get());
    if (job_id == 0) {
      SetError(cupsLastErrorString());
      return;
    }
    jobId_ = job_id;
  }

  void OnOK() override { deferred_.Resolve(Napi::Number::New(Env(), jobId_)); }

private:
  std::string filename_;
  std::string docName_;
  std::string printerName_;
  std::vector<std::pair<std::string, std::string>> options_;
  int jobId_ = 0;
};
} // namespace

Napi::Value getAllPrinterDetails(const Napi::CallbackInfo &iArgs) {
  auto *worker = new GetAllPrinterDetailsWorker(iArgs.Env());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value getPrinterDetails(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 1) {
    Napi::Error::New(env, "Expected 1 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  auto *worker =
      new GetPrinterDetailsWorker(env, iArgs[0].As<Napi::String>().Utf8Value());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value hasPrinter(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 1) {
    Napi::Error::New(env, "Expected 1 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto *worker =
      new HasPrinterWorker(env, iArgs[0].As<Napi::String>().Utf8Value());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value getDefaultPrinterName(const Napi::CallbackInfo &iArgs) {
  auto *worker = new GetDefaultPrinterNameWorker(iArgs.Env());
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value getJob(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 2) {
    Napi::Error::New(env, "Expected 2 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf8Value();
  int jobId;
  if (!iArgs[1].IsNumber()) {
    Napi::Error::New(env, "Job ID must be a number")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  jobId = iArgs[1].As<Napi::Number>().Int32Value();

  auto *worker = new GetJobWorker(env, std::move(printername), jobId);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value cancelJob(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 2) {
    Napi::Error::New(env, "Expected 2 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf8Value();
  int jobId;
  if (!iArgs[1].IsNumber()) {
    Napi::Error::New(env, "Job ID must be a number")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  jobId = iArgs[1].As<Napi::Number>().Int32Value();

  auto *worker = new CancelJobWorker(env, std::move(printername), jobId);
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Array result = Napi::Array::New(env);
  int i = 0;
  for (FormatMapType::const_iterator itFormat = getPrinterFormatMap().begin();
       itFormat != getPrinterFormatMap().end(); ++itFormat) {
    result.Set(i++, Napi::String::New(env, itFormat->first.c_str()));
  }
  // No CUPS I/O here (just a static in-memory map), so there's nothing to
  // offload to an AsyncWorker. Still return a real Promise so every native
  // export has a consistent async signature across platforms.
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(result);
  return deferred.Promise();
}

Napi::Value PrintDirect(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 5) {
    Napi::Error::New(env, "Expected 5 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string data;
  Napi::Value arg0(iArgs[0]);
  if (arg0.IsString()) {
    data = arg0.As<Napi::String>().Utf8Value();
  } else if (arg0.IsBuffer()) {
    Napi::Buffer<char> buffer = arg0.As<Napi::Buffer<char>>();
    data.assign(buffer.Data(), buffer.Length());
  } else {
    Napi::Error::New(env, "Data must be a string or Buffer")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string printername;
  if (!iArgs[1].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[1].As<Napi::String>().Utf8Value();
  std::string docname;
  if (!iArgs[2].IsString()) {
    Napi::Error::New(env, "Document name must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  docname = iArgs[2].As<Napi::String>().Utf8Value();
  std::string type;
  if (!iArgs[3].IsString()) {
    Napi::Error::New(env, "Type must be a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  type = iArgs[3].As<Napi::String>().Utf8Value();
  Napi::Object print_options;
  if (!iArgs[4].IsObject()) {
    Napi::Error::New(env, "Print options must be an object")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  print_options = iArgs[4].As<Napi::Object>();

  auto *worker = new PrintDirectWorker(
      env, std::move(data), std::move(printername), std::move(docname),
      std::move(type), extractOptions(print_options));
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value PrintFile(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 4) {
    Napi::Error::New(env, "Expected 4 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string filename;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Filename must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  filename = iArgs[0].As<Napi::String>().Utf8Value();
  std::string docname;
  if (!iArgs[1].IsString()) {
    Napi::Error::New(env, "Document name must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  docname = iArgs[1].As<Napi::String>().Utf8Value();
  std::string printer;
  if (!iArgs[2].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printer = iArgs[2].As<Napi::String>().Utf8Value();
  Napi::Object print_options;
  if (!iArgs[3].IsObject()) {
    Napi::Error::New(env, "Print options must be an object")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  print_options = iArgs[3].As<Napi::Object>();

  auto *worker =
      new PrintFileWorker(env, std::move(filename), std::move(docname),
                          std::move(printer), extractOptions(print_options));
  worker->Queue();
  return worker->GetPromise();
}
