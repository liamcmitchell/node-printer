#include "node_printer.hpp"

#include <map>
#include <node_version.h>
#include <sstream>
#include <string>
#include <utility>

#include <cups/cups.h>
#include <cups/ppd.h>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace {
typedef std::map<std::string, int> StatusMapType;
typedef std::map<std::string, std::string> FormatMapType;

const StatusMapType &getJobStatusMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
  // add only first time
#define STATUS_PRINTER_ADD(value, type)                                        \
  result.insert(std::make_pair(value, type))
  // Common statuses
  STATUS_PRINTER_ADD("PRINTING", IPP_JOB_PROCESSING);
  STATUS_PRINTER_ADD("PRINTED", IPP_JOB_COMPLETED);
  STATUS_PRINTER_ADD("PAUSED", IPP_JOB_HELD);
  // Specific statuses
  STATUS_PRINTER_ADD("PENDING", IPP_JOB_PENDING);
  STATUS_PRINTER_ADD("PAUSED", IPP_JOB_STOPPED);
  STATUS_PRINTER_ADD("CANCELLED", IPP_JOB_CANCELLED);
  STATUS_PRINTER_ADD("ABORTED", IPP_JOB_ABORTED);

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
  // Common fields
  result_printer_job.Set(Napi::String::New(env, "id"),
                         Napi::Number::New(env, job->id));
  result_printer_job.Set(Napi::String::New(env, "name"),
                         Napi::String::New(env, job->title));
  result_printer_job.Set(Napi::String::New(env, "printerName"),
                         Napi::String::New(env, job->dest));
  result_printer_job.Set(Napi::String::New(env, "user"),
                         Napi::String::New(env, job->user));
  std::string job_format(job->format);

  // Try to parse the data format, otherwise will write the unformatted one
  for (FormatMapType::const_iterator itFormat = getPrinterFormatMap().begin();
       itFormat != getPrinterFormatMap().end(); ++itFormat) {
    if (itFormat->second == job_format) {
      job_format = itFormat->first;
      break;
    }
  }

  result_printer_job.Set(Napi::String::New(env, "format"),
                         Napi::String::New(env, job_format.c_str()));
  result_printer_job.Set(Napi::String::New(env, "priority"),
                         Napi::Number::New(env, job->priority));
  result_printer_job.Set(Napi::String::New(env, "size"),
                         Napi::Number::New(env, job->size));
  Napi::Array result_printer_job_status = Napi::Array::New(env);
  int i_status = 0;
  for (StatusMapType::const_iterator itStatus = getJobStatusMap().begin();
       itStatus != getJobStatusMap().end(); ++itStatus) {
    if (job->state == itStatus->second) {
      result_printer_job_status.Set(
          i_status++, Napi::String::New(env, itStatus->first.c_str()));
      // only one status could be on posix
      break;
    }
  }
  if (i_status == 0) {
    // A new status? report as unsupported
    std::ostringstream s;
    s << "unsupported job status: " << job->state;
    result_printer_job_status.Set(i_status++,
                                  Napi::String::New(env, s.str().c_str()));
  }

  result_printer_job.Set(Napi::String::New(env, "status"),
                         result_printer_job_status);

  // Specific fields
  //  Ecmascript store time in milliseconds, but time_t in seconds

  double creationTime = ((double)job->creation_time) * 1000;
  double completedTime = ((double)job->completed_time) * 1000;
  double processingTime = ((double)job->processing_time) * 1000;

  result_printer_job.Set(Napi::String::New(env, "completedTime"),
                         Napi::Date::New(env, completedTime));
  result_printer_job.Set(Napi::String::New(env, "creationTime"),
                         Napi::Date::New(env, creationTime));
  result_printer_job.Set(Napi::String::New(env, "processingTime"),
                         Napi::Date::New(env, processingTime));

  // No error. return an empty string
  return "";
}

/** Parses printer driver PPD options
 */
void populatePpdOptions(Napi::Object ppd_options, ppd_file_t *ppd,
                        ppd_group_t *group) {
  Napi::Env env = ppd_options.Env();
  int i, j;
  ppd_option_t *option;
  ppd_choice_t *choice;
  ppd_group_t *subgroup;

  for (i = group->num_options, option = group->options; i > 0; --i, ++option) {
    Napi::Object ppd_suboptions = Napi::Object::New(env);
    for (j = option->num_choices, choice = option->choices; j > 0;
         --j, ++choice) {
      ppd_suboptions.Set(
          Napi::String::New(env, choice->choice),
          Napi::Boolean::New(env, static_cast<bool>(choice->marked)));
    }

    ppd_options.Set(Napi::String::New(env, option->keyword), ppd_suboptions);
  }

  for (i = group->num_subgroups, subgroup = group->subgroups; i > 0;
       --i, ++subgroup) {
    populatePpdOptions(ppd_options, ppd, subgroup);
  }
}

/** Parse printer driver options
 * @return error string.
 */
std::string parseDriverOptions(const cups_dest_t *printer,
                               Napi::Object ppd_options) {
  const char *filename;
  ppd_file_t *ppd;
  ppd_group_t *group;
  int i;

  std::ostringstream error_str; // error string

  if ((filename = cupsGetPPD(printer->name)) != NULL) {
    if ((ppd = ppdOpenFile(filename)) != NULL) {
      ppdMarkDefaults(ppd);
      cupsMarkOptions(ppd, printer->num_options, printer->options);

      for (i = ppd->num_groups, group = ppd->groups; i > 0; --i, ++group) {
        populatePpdOptions(ppd_options, ppd, group);
      }
      ppdClose(ppd);
    } else {
      error_str << "Unable to open PPD filename " << filename << " ";
    }
    unlink(filename);
  } else {
    error_str << "Unable to get CUPS PPD driver file. ";
  }

  return error_str.str();
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

  if (printer->instance) {
    result_printer.Set(Napi::String::New(env, "instance"),
                       Napi::String::New(env, printer->instance));
  }

  Napi::Object result_printer_options = Napi::Object::New(env);
  cups_option_t *dest_option = printer->options;
  for (int j = 0; j < printer->num_options; ++j, ++dest_option) {
    result_printer_options.Set(Napi::String::New(env, dest_option->name),
                               Napi::String::New(env, dest_option->value));
  }
  result_printer.Set(Napi::String::New(env, "options"), result_printer_options);
  // Get printer jobs
  cups_job_t *jobs;
  int totalJobs = cupsGetJobs(&jobs, printer->name, 0 /*0 means all users*/,
                              CUPS_WHICHJOBS_ACTIVE);
  std::string error_str;
  if (totalJobs > 0) {
    Napi::Array result_priner_jobs = Napi::Array::New(env, totalJobs);
    int jobi = 0;
    cups_job_t *job = jobs;
    for (; jobi < totalJobs; ++jobi, ++job) {
      Napi::Object result_printer_job = Napi::Object::New(env);
      error_str = parseJobObject(job, result_printer_job);
      if (!error_str.empty()) {
        // got an error? break then.
        break;
      }
      result_priner_jobs.Set(jobi, result_printer_job);
    }
    result_printer.Set(Napi::String::New(env, "jobs"), result_priner_jobs);
  }
  cupsFreeJobs(totalJobs, jobs);
  return error_str;
}

/// cups option class to automatically free memory.
class CupsOptions : public MemValueBase<cups_option_t> {
protected:
  int num_options;
  virtual void free() {
    if (_value != NULL) {
      cupsFreeOptions(num_options, get());
      _value = NULL;
      num_options = 0;
    }
  }

public:
  CupsOptions() : num_options(0) {}
  ~CupsOptions() { free(); }

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

Napi::Value getDefaultPrinterName(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  // This does not return default user printer name according to
  // https://www.cups.org/documentation.php/doc-2.0/api-cups.html#cupsGetDefault2
  // so leave as undefined and JS implementation will loop in all printers

  const char *printerName = NULL; // cupsGetDefault();

  // return default printer name only if defined
  if (printerName != NULL) {
    return Napi::String::New(env, printerName);
  }

  return env.Undefined();
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
    // printer not found
    Napi::Error::New(env, "Printer not found").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return result_printer;
}

Napi::Value getPrinterDriverOptions(const Napi::CallbackInfo &iArgs) {
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
  Napi::Object driver_options = Napi::Object::New(env);
  if (printer != NULL) {
    parseDriverOptions(printer, driver_options);
  }
  cupsFreeDests(printers_size, printers);
  if (printer == NULL) {
    // printer not found
    Napi::Error::New(env, "Printer not found").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return driver_options;
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
    // printer not found
    Napi::Error::New(env, "Printer job not found").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return result_printer_job;
}

Napi::Value setJob(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 3) {
    Napi::Error::New(env, "Expected 3 arguments").ThrowAsJavaScriptException();
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
  std::string jobCommandV8;
  if (!iArgs[2].IsString()) {
    Napi::Error::New(env, "Job command must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  jobCommandV8 = iArgs[2].As<Napi::String>().Utf8Value();
  if (jobId < 0) {
    Napi::Error::New(env, "Wrong job number").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::string jobCommandStr(jobCommandV8);
  bool result_ok = false;
  if (jobCommandStr == "CANCEL") {
    result_ok = (cupsCancelJob(printername.c_str(), jobId) == 1);
  } else {
    Napi::Error::New(env, "wrong job command. use getSupportedJobCommands to "
                          "see the possible commands")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return Napi::Boolean::New(env, result_ok);
}

Napi::Value getSupportedJobCommands(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Array result = Napi::Array::New(env);
  int i = 0;
  result.Set(i++, Napi::String::New(env, "CANCEL"));
  return result;
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
  FormatMapType::const_iterator itFormat = getPrinterFormatMap().find(type_str);
  if (itFormat == getPrinterFormatMap().end()) {
    Napi::Error::New(env, "unsupported format type")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  type_str = itFormat->second;

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
    return Napi::String::New(env, cupsLastErrorString());
  } else {
    return Napi::Number::New(env, job_id);
  }
}
