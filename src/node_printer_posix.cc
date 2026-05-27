#include "node_printer.hpp"

#include <map>
#include <node_version.h>
#include <sstream>
#include <string>
#include <utility>

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

/** Parse job info object.
 * @return error string. if empty, then no error
 */
std::string parseJobObject(const cups_job_t *job,
                           Napi::Object result_printer_job) {
  Napi::Env env = result_printer_job.Env();

  // Standardized fields
  result_printer_job.Set(Napi::String::New(env, "id"),
                         Napi::Number::New(env, job->id));
  result_printer_job.Set(Napi::String::New(env, "name"),
                         Napi::String::New(env, job->title));
  result_printer_job.Set(Napi::String::New(env, "printerName"),
                         Napi::String::New(env, job->dest));
  result_printer_job.Set(Napi::String::New(env, "user"),
                         Napi::String::New(env, job->user));
  result_printer_job.Set(Napi::String::New(env, "size"),
                         Napi::Number::New(env, job->size));

  // state (IPP job-state keyword)
  const char *state_str = "pending";
  for (auto &entry : getJobStatusMap()) {
    if (job->state == entry.second) {
      state_str = entry.first.c_str();
      break;
    }
  }
  result_printer_job.Set(Napi::String::New(env, "state"),
                         Napi::String::New(env, state_str));

  // Timestamps as epoch seconds
  result_printer_job.Set(Napi::String::New(env, "createdAt"),
                         Napi::Number::New(env, (double)job->creation_time));
  result_printer_job.Set(Napi::String::New(env, "processingAt"),
                         Napi::Number::New(env, (double)job->processing_time));
  result_printer_job.Set(Napi::String::New(env, "completedAt"),
                         Napi::Number::New(env, (double)job->completed_time));

  // Platform-specific raw fields
  Napi::Object raw = Napi::Object::New(env);
  raw.Set(Napi::String::New(env, "format"),
          Napi::String::New(env, job->format));
  raw.Set(Napi::String::New(env, "priority"),
          Napi::Number::New(env, job->priority));
  raw.Set(Napi::String::New(env, "stateCode"),
          Napi::Number::New(env, job->state));
  result_printer_job.Set(Napi::String::New(env, "raw"), raw);

  return "";
}

/** Parse printer info object
 * @return error string.
 */
std::string parsePrinterInfo(const cups_dest_t *printer,
                             Napi::Object result_printer) {
  Napi::Env env = result_printer.Env();
  result_printer.Set(Napi::String::New(env, "name"),
                     Napi::String::New(env, printer->name));
  result_printer.Set(
      Napi::String::New(env, "isDefault"),
      Napi::Boolean::New(env, static_cast<bool>(printer->is_default)));

  // Map printer-state to standardized state
  const char *state_val = cupsGetOption("printer-state", printer->num_options,
                                        printer->options);
  const char *state_str = "idle";
  if (state_val) {
    switch (state_val[0]) {
    case '4':
      state_str = "processing";
      break;
    case '5':
      state_str = "stopped";
      break;
    }
  }
  result_printer.Set(Napi::String::New(env, "state"),
                     Napi::String::New(env, state_str));

  // Parse printer-state-reasons into stateReasons array
  Napi::Array state_reasons = Napi::Array::New(env);
  const char *reasons_val = cupsGetOption(
      "printer-state-reasons", printer->num_options, printer->options);
  if (reasons_val) {
    std::string reasons(reasons_val);
    uint32_t idx = 0;
    size_t pos = 0;
    while (pos < reasons.size()) {
      size_t comma = reasons.find(',', pos);
      if (comma == std::string::npos)
        comma = reasons.size();
      std::string reason = reasons.substr(pos, comma - pos);
      // Strip severity suffix (-report, -warning, -error)
      size_t dash = reason.rfind('-');
      if (dash != std::string::npos) {
        std::string suffix = reason.substr(dash);
        if (suffix == "-report" || suffix == "-warning" || suffix == "-error") {
          reason = reason.substr(0, dash);
        }
      }
      state_reasons.Set(idx++, Napi::String::New(env, reason));
      pos = comma + 1;
    }
  }
  result_printer.Set(Napi::String::New(env, "stateReasons"), state_reasons);

  // All CUPS options go into raw
  Napi::Object raw = Napi::Object::New(env);
  cups_option_t *dest_option = printer->options;
  for (int j = 0; j < printer->num_options; ++j, ++dest_option) {
    raw.Set(Napi::String::New(env, dest_option->name),
            Napi::String::New(env, dest_option->value));
  }
  if (printer->instance) {
    raw.Set(Napi::String::New(env, "instance"),
            Napi::String::New(env, printer->instance));
  }
  result_printer.Set(Napi::String::New(env, "raw"), raw);

  // Get printer jobs
  Napi::Array result_priner_jobs = Napi::Array::New(env);
  cups_job_t *jobs;
  int totalJobs = cupsGetJobs(&jobs, printer->name, 0 /*0 means all users*/,
                              CUPS_WHICHJOBS_ACTIVE);
  std::string error_str;
  if (totalJobs > 0) {
    int jobi = 0;
    cups_job_t *job = jobs;
    for (; jobi < totalJobs; ++jobi, ++job) {
      Napi::Object result_printer_job = Napi::Object::New(env);
      error_str = parseJobObject(job, result_printer_job);
      if (!error_str.empty()) {
        break;
      }
      result_priner_jobs.Set(jobi, result_printer_job);
    }
  }
  result_printer.Set(Napi::String::New(env, "jobs"), result_priner_jobs);
  cupsFreeJobs(totalJobs, jobs);
  return error_str;
}

/// cups option class to automatically free memory.
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

  /// Add options from v8 object
  CupsOptions(Napi::Object iV8Options) : num_options(0) {
    Napi::Array props = iV8Options.GetPropertyNames();

    for (uint32_t i = 0; i < props.Length(); ++i) {
      Napi::Value key = props.Get(i);
      std::string keyStr = key.As<Napi::String>().Utf8Value();
      std::string valStr = iV8Options.Get(key).As<Napi::String>().Utf8Value();

      num_options =
          cupsAddOption(keyStr.c_str(), valStr.c_str(), num_options, &_value);
    }
  }

  const int &getNumOptions() { return num_options; }
  cups_option_t *get() { return _value; }
};
} // namespace

Napi::Value getPrinters(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();

  cups_dest_t *printers = NULL;
  int printers_size = cupsGetDests(&printers);
  Napi::Array result = Napi::Array::New(env, printers_size);
  cups_dest_t *printer = printers;
  std::string error_str;
  for (int i = 0; i < printers_size; ++i, ++printer) {
    Napi::Object result_printer = Napi::Object::New(env);
    error_str = parsePrinterInfo(printer, result_printer);
    if (!error_str.empty()) {
      // got an error? break then
      break;
    }
    result.Set(i, result_printer);
  }
  cupsFreeDests(printers_size, printers);
  if (!error_str.empty()) {
    // got an error? return the error then
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return result;
}

Napi::Value getPrinter(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 1) {
    Napi::Error::New(env, "Expected 1 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf8Value();

  cups_dest_t *printers = NULL, *printer = NULL;
  int printers_size = cupsGetDests(&printers);
  printer = cupsGetDest(printername.c_str(), NULL, printers_size, printers);
  Napi::Object result_printer = Napi::Object::New(env);
  if (printer != NULL) {
    parsePrinterInfo(printer, result_printer);
  }
  cupsFreeDests(printers_size, printers);
  if (printer == NULL) {
    return env.Null();
  }
  return result_printer;
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
    Napi::Error::New(env, "Job id must be a number")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  jobId = iArgs[1].As<Napi::Number>().Int32Value();

  Napi::Object result_printer_job = Napi::Object::New(env);
  // Get printer jobs
  cups_job_t *jobs = NULL, *jobFound = NULL;
  int totalJobs = cupsGetJobs(&jobs, printername.c_str(),
                              0 /*0 means all users*/, CUPS_WHICHJOBS_ALL);
  if (totalJobs > 0) {
    int jobi = 0;
    cups_job_t *job = jobs;
    for (; jobi < totalJobs; ++jobi, ++job) {
      if (job->id != jobId) {
        continue;
      }
      // Job Found
      jobFound = job;
      parseJobObject(job, result_printer_job);
      break;
    }
  }
  cupsFreeJobs(totalJobs, jobs);
  if (jobFound == NULL) {
    return env.Null();
  }
  return result_printer_job;
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
    Napi::Error::New(env, "Job id must be a number")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  jobId = iArgs[1].As<Napi::Number>().Int32Value();
  if (jobId < 0) {
    Napi::Error::New(env, "Wrong job number").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  // Ignore return value: cupsCancelJob returns 0 if the job no longer exists,
  // which we treat as a no-op.
  cupsCancelJob(printername.c_str(), jobId);
  return env.Undefined();
}

Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Array result = Napi::Array::New(env);
  int i = 0;
  for (FormatMapType::const_iterator itFormat = getPrinterFormatMap().begin();
       itFormat != getPrinterFormatMap().end(); ++itFormat) {
    result.Set(i++, Napi::String::New(env, itFormat->first.c_str()));
  }
  return result;
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

  std::string type_str(type);
  auto itFormat = getPrinterFormatMap().find(type_str);
  // Known aliases (RAW, PDF, etc.) are translated to MIME types.
  // Anything else is passed directly to CUPS as a MIME type — let CUPS
  // reject it if unsupported, matching the Windows spooler behaviour.
  if (itFormat != getPrinterFormatMap().end()) {
    type_str = itFormat->second;
  }

  CupsOptions options(print_options);

  int job_id =
      cupsCreateJob(CUPS_HTTP_DEFAULT, printername.c_str(), docname.c_str(),
                    options.getNumOptions(), options.get());
  if (job_id == 0) {
    Napi::Error::New(env, cupsLastErrorString()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (HTTP_CONTINUE != cupsStartDocument(CUPS_HTTP_DEFAULT, printername.c_str(),
                                         job_id, docname.c_str(),
                                         type_str.c_str(),
                                         1 /*last document*/)) {
    Napi::Error::New(env, cupsLastErrorString()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  /* cupsWriteRequestData can be called as many times as needed */
  // TODO: to split big buffer
  if (HTTP_CONTINUE !=
      cupsWriteRequestData(CUPS_HTTP_DEFAULT, data.c_str(), data.size())) {
    cupsFinishDocument(CUPS_HTTP_DEFAULT, printername.c_str());
    Napi::Error::New(env, cupsLastErrorString()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  cupsFinishDocument(CUPS_HTTP_DEFAULT, printername.c_str());

  return Napi::Number::New(env, job_id);
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

  CupsOptions options(print_options);

  int job_id = cupsPrintFile(printer.c_str(), filename.c_str(), docname.c_str(),
                             options.getNumOptions(), options.get());

  if (job_id == 0) {
    Napi::Error::New(env, cupsLastErrorString()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return Napi::Number::New(env, job_id);
}
