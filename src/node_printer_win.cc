#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Winspool.h>
#pragma comment(lib, "Winspool.lib")
#endif

#include "node_printer.hpp"

#include <map>
#include <node_version.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <memory>

namespace {
typedef std::map<std::string, DWORD> StatusMapType;

struct FreeDeleter {
  void operator()(void *p) const { ::free(p); }
};

template <typename Type> using MallocPtr = std::unique_ptr<Type, FreeDeleter>;

template <typename Type> MallocPtr<Type> mallocValue(DWORD sizeBytes) {
  return MallocPtr<Type>(static_cast<Type *>(malloc(sizeBytes)));
}

struct PrinterHandle {
  PrinterHandle(LPWSTR iPrinterName) {
    _ok = OpenPrinterW(iPrinterName, &_printer, NULL);
  }
  ~PrinterHandle() {
    if (_ok) {
      ClosePrinter(_printer);
    }
  }
  operator HANDLE() { return _printer; }
  operator bool() { return (!!_ok); }
  HANDLE &operator*() { return _printer; }
  HANDLE *operator->() { return &_printer; }
  const HANDLE &operator->() const { return _printer; }
  HANDLE _printer;
  BOOL _ok;
};

const StatusMapType &getStatusMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
  // add only first time
#define STATUS_PRINTER_ADD(value, type)                                        \
  result.insert(std::make_pair(value, type))
  STATUS_PRINTER_ADD("BUSY", PRINTER_STATUS_BUSY);
  STATUS_PRINTER_ADD("DOOR-OPEN", PRINTER_STATUS_DOOR_OPEN);
  STATUS_PRINTER_ADD("DRIVER_UPDATE_NEEDED",
                     PRINTER_STATUS_DRIVER_UPDATE_NEEDED);
  STATUS_PRINTER_ADD("ERROR", PRINTER_STATUS_ERROR);
  STATUS_PRINTER_ADD("INITIALIZING", PRINTER_STATUS_INITIALIZING);
  STATUS_PRINTER_ADD("IO-ACTIVE", PRINTER_STATUS_IO_ACTIVE);
  STATUS_PRINTER_ADD("MANUAL-FEED", PRINTER_STATUS_MANUAL_FEED);
  STATUS_PRINTER_ADD("NO-TONER", PRINTER_STATUS_NO_TONER);
  STATUS_PRINTER_ADD("NOT-AVAILABLE", PRINTER_STATUS_NOT_AVAILABLE);
  STATUS_PRINTER_ADD("OFFLINE", PRINTER_STATUS_OFFLINE);
  STATUS_PRINTER_ADD("OUT-OF-MEMORY", PRINTER_STATUS_OUT_OF_MEMORY);
  STATUS_PRINTER_ADD("OUTPUT-BIN-FULL", PRINTER_STATUS_OUTPUT_BIN_FULL);
  STATUS_PRINTER_ADD("PAGE-PUNT", PRINTER_STATUS_PAGE_PUNT);
  STATUS_PRINTER_ADD("PAPER-JAM", PRINTER_STATUS_PAPER_JAM);
  STATUS_PRINTER_ADD("PAPER-OUT", PRINTER_STATUS_PAPER_OUT);
  STATUS_PRINTER_ADD("PAPER-PROBLEM", PRINTER_STATUS_PAPER_PROBLEM);
  STATUS_PRINTER_ADD("PAUSED", PRINTER_STATUS_PAUSED);
  STATUS_PRINTER_ADD("PENDING-DELETION", PRINTER_STATUS_PENDING_DELETION);
  STATUS_PRINTER_ADD("POWER-SAVE", PRINTER_STATUS_POWER_SAVE);
  STATUS_PRINTER_ADD("PRINTING", PRINTER_STATUS_PRINTING);
  STATUS_PRINTER_ADD("PROCESSING", PRINTER_STATUS_PROCESSING);
  STATUS_PRINTER_ADD("SERVER-OFFLINE", PRINTER_STATUS_SERVER_OFFLINE);
  STATUS_PRINTER_ADD("SERVER-UNKNOWN", PRINTER_STATUS_SERVER_UNKNOWN);
  STATUS_PRINTER_ADD("TONER-LOW", PRINTER_STATUS_TONER_LOW);
  STATUS_PRINTER_ADD("USER-INTERVENTION", PRINTER_STATUS_USER_INTERVENTION);
  STATUS_PRINTER_ADD("WAITING", PRINTER_STATUS_WAITING);
  STATUS_PRINTER_ADD("WARMING-UP", PRINTER_STATUS_WARMING_UP);
#undef STATUS_PRINTER_ADD
  return result;
}

/// Map Windows status bits to IPP printer-state-reasons keywords
typedef std::map<DWORD, std::string> IppReasonMapType;

const IppReasonMapType &getIppReasonMap() {
  static IppReasonMapType result;
  if (!result.empty()) {
    return result;
  }
  result[PRINTER_STATUS_PAPER_JAM] = "media-jam";
  result[PRINTER_STATUS_PAPER_OUT] = "media-empty";
  result[PRINTER_STATUS_PAPER_PROBLEM] = "media-empty";
  result[PRINTER_STATUS_MANUAL_FEED] = "media-needed";
  result[PRINTER_STATUS_NO_TONER] = "toner-empty";
  result[PRINTER_STATUS_TONER_LOW] = "toner-low";
  result[PRINTER_STATUS_DOOR_OPEN] = "door-open";
  result[PRINTER_STATUS_OUTPUT_BIN_FULL] = "output-area-full";
  result[PRINTER_STATUS_OFFLINE] = "offline";
  result[PRINTER_STATUS_NOT_AVAILABLE] = "offline";
  result[PRINTER_STATUS_SERVER_OFFLINE] = "offline";
  result[PRINTER_STATUS_PAUSED] = "paused";
  result[PRINTER_STATUS_ERROR] = "other";
  result[PRINTER_STATUS_USER_INTERVENTION] = "other";
  result[PRINTER_STATUS_OUT_OF_MEMORY] = "other";
  result[PRINTER_STATUS_PAGE_PUNT] = "other";
  return result;
}

/// Fault bits that indicate printer should be in "stopped" state
const DWORD kStoppedMask =
    PRINTER_STATUS_PAUSED | PRINTER_STATUS_ERROR | PRINTER_STATUS_OFFLINE |
    PRINTER_STATUS_PAPER_JAM | PRINTER_STATUS_PAPER_OUT |
    PRINTER_STATUS_PAPER_PROBLEM | PRINTER_STATUS_NO_TONER |
    PRINTER_STATUS_DOOR_OPEN | PRINTER_STATUS_USER_INTERVENTION |
    PRINTER_STATUS_OUT_OF_MEMORY | PRINTER_STATUS_OUTPUT_BIN_FULL |
    PRINTER_STATUS_NOT_AVAILABLE | PRINTER_STATUS_SERVER_OFFLINE |
    PRINTER_STATUS_PAGE_PUNT;

/// Get the system default printer name (UTF-16)
std::u16string getDefaultPrinterName() {
  DWORD size = 0;
  GetDefaultPrinterW(NULL, &size);
  if (size == 0)
    return std::u16string();
  std::vector<wchar_t> buf(size);
  if (!GetDefaultPrinterW(buf.data(), &size))
    return std::u16string();
  return std::u16string(reinterpret_cast<char16_t *>(buf.data()));
}

const StatusMapType &getJobStatusMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
  // add only first time
#define STATUS_PRINTER_ADD(value, type)                                        \
  result.insert(std::make_pair(value, type))
  // Common statuses
  STATUS_PRINTER_ADD("PRINTING", JOB_STATUS_PRINTING);
  STATUS_PRINTER_ADD("PRINTED", JOB_STATUS_PRINTED);
  STATUS_PRINTER_ADD("PAUSED", JOB_STATUS_PAUSED);

  // Specific statuses
  STATUS_PRINTER_ADD("BLOCKED-DEVQ", JOB_STATUS_BLOCKED_DEVQ);
  STATUS_PRINTER_ADD("DELETED", JOB_STATUS_DELETED);
  STATUS_PRINTER_ADD("DELETING", JOB_STATUS_DELETING);
  STATUS_PRINTER_ADD("ERROR", JOB_STATUS_ERROR);
  STATUS_PRINTER_ADD("OFFLINE", JOB_STATUS_OFFLINE);
  STATUS_PRINTER_ADD("PAPEROUT", JOB_STATUS_PAPEROUT);
  STATUS_PRINTER_ADD("RESTART", JOB_STATUS_RESTART);
  STATUS_PRINTER_ADD("SPOOLING", JOB_STATUS_SPOOLING);
  STATUS_PRINTER_ADD("USER-INTERVENTION", JOB_STATUS_USER_INTERVENTION);
  // XP and later
#ifdef JOB_STATUS_COMPLETE
  STATUS_PRINTER_ADD("COMPLETE", JOB_STATUS_COMPLETE);
#endif
#ifdef JOB_STATUS_RETAINED
  STATUS_PRINTER_ADD("RETAINED", JOB_STATUS_RETAINED);
#endif

#undef STATUS_PRINTER_ADD
  return result;
}

const StatusMapType &getAttributeMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
  // add only first time
#define ATTRIBUTE_PRINTER_ADD(value, type)                                     \
  result.insert(std::make_pair(value, type))
  ATTRIBUTE_PRINTER_ADD("DIRECT", PRINTER_ATTRIBUTE_DIRECT);
  ATTRIBUTE_PRINTER_ADD("DO-COMPLETE-FIRST",
                        PRINTER_ATTRIBUTE_DO_COMPLETE_FIRST);
  ATTRIBUTE_PRINTER_ADD("ENABLE-BIDI", PRINTER_ATTRIBUTE_ENABLE_BIDI);
  ATTRIBUTE_PRINTER_ADD("ENABLE-DEVQ", PRINTER_ATTRIBUTE_ENABLE_DEVQ);
  ATTRIBUTE_PRINTER_ADD("HIDDEN", PRINTER_ATTRIBUTE_HIDDEN);
  ATTRIBUTE_PRINTER_ADD("KEEPPRINTEDJOBS", PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS);
  ATTRIBUTE_PRINTER_ADD("LOCAL", PRINTER_ATTRIBUTE_LOCAL);
  ATTRIBUTE_PRINTER_ADD("NETWORK", PRINTER_ATTRIBUTE_NETWORK);
  ATTRIBUTE_PRINTER_ADD("PUBLISHED", PRINTER_ATTRIBUTE_PUBLISHED);
  ATTRIBUTE_PRINTER_ADD("QUEUED", PRINTER_ATTRIBUTE_QUEUED);
  ATTRIBUTE_PRINTER_ADD("RAW-ONLY", PRINTER_ATTRIBUTE_RAW_ONLY);
  ATTRIBUTE_PRINTER_ADD("SHARED", PRINTER_ATTRIBUTE_SHARED);
  ATTRIBUTE_PRINTER_ADD("OFFLINE", PRINTER_ATTRIBUTE_WORK_OFFLINE);
  // XP
#ifdef PRINTER_ATTRIBUTE_FAX
  ATTRIBUTE_PRINTER_ADD("FAX", PRINTER_ATTRIBUTE_FAX);
#endif
  // vista
#ifdef PRINTER_ATTRIBUTE_FRIENDLY_NAME
  ATTRIBUTE_PRINTER_ADD("FRIENDLY-NAME", PRINTER_ATTRIBUTE_FRIENDLY_NAME);
  ATTRIBUTE_PRINTER_ADD("MACHINE", PRINTER_ATTRIBUTE_MACHINE);
  ATTRIBUTE_PRINTER_ADD("PUSHED-USER", PRINTER_ATTRIBUTE_PUSHED_USER);
  ATTRIBUTE_PRINTER_ADD("PUSHED-MACHINE", PRINTER_ATTRIBUTE_PUSHED_MACHINE);
  ATTRIBUTE_PRINTER_ADD("TS_GENERIC_DRIVER",
                        PRINTER_ATTRIBUTE_TS_GENERIC_DRIVER);
#endif
  // server 2003
#ifdef PRINTER_ATTRIBUTE_TS
  ATTRIBUTE_PRINTER_ADD("TS", PRINTER_ATTRIBUTE_TS);
#endif
#undef ATTRIBUTE_PRINTER_ADD
  return result;
}

const StatusMapType &getJobCommandMap() {
  static StatusMapType result;
  if (!result.empty()) {
    return result;
  }
  // add only first time
#define COMMAND_JOB_ADD(value, type) result.insert(std::make_pair(value, type))
  COMMAND_JOB_ADD("CANCEL", JOB_CONTROL_CANCEL);
  COMMAND_JOB_ADD("PAUSE", JOB_CONTROL_PAUSE);
  COMMAND_JOB_ADD("RESTART", JOB_CONTROL_RESTART);
  COMMAND_JOB_ADD("RESUME", JOB_CONTROL_RESUME);
  COMMAND_JOB_ADD("DELETE", JOB_CONTROL_DELETE);
  COMMAND_JOB_ADD("SENT-TO-PRINTER", JOB_CONTROL_SENT_TO_PRINTER);
  COMMAND_JOB_ADD("LAST-PAGE-EJECTED", JOB_CONTROL_LAST_PAGE_EJECTED);
#ifdef JOB_CONTROL_RETAIN
  COMMAND_JOB_ADD("RETAIN", JOB_CONTROL_RETAIN);
#endif
#ifdef JOB_CONTROL_RELEASE
  COMMAND_JOB_ADD("RELEASE", JOB_CONTROL_RELEASE);
#endif
#undef COMMAND_JOB_ADD
  return result;
}

/// Convert SYSTEMTIME to Unix epoch seconds
double systemTimeToEpoch(const SYSTEMTIME &st) {
  FILETIME ft;
  if (!SystemTimeToFileTime(&st, &ft))
    return 0;
  // FILETIME is 100-nanosecond intervals since 1601-01-01
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  // Convert to seconds since Unix epoch (1970-01-01)
  return (double)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

/// Map Windows job status bitfield to IPP job-state keyword
const char *jobStatusToIppState(DWORD status) {
  if (status & (JOB_STATUS_DELETED | JOB_STATUS_DELETING))
    return "canceled";
  if (status & JOB_STATUS_ERROR)
    return "aborted";
#ifdef JOB_STATUS_COMPLETE
  if (status & JOB_STATUS_COMPLETE)
    return "completed";
#endif
  if (status & JOB_STATUS_PRINTED)
    return "completed";
  if (status & JOB_STATUS_PRINTING)
    return "processing";
  if (status & (JOB_STATUS_PAUSED | JOB_STATUS_BLOCKED_DEVQ |
                JOB_STATUS_OFFLINE | JOB_STATUS_PAPEROUT |
                JOB_STATUS_USER_INTERVENTION))
    return "processing-stopped";
  if (status & JOB_STATUS_SPOOLING)
    return "pending";
  return "pending";
}

void parseJobObject(JOB_INFO_2W *job, Napi::Object result_printer_job) {
  Napi::Env env = result_printer_job.Env();
  auto wstrToNapi = [&](LPCWSTR value) -> Napi::String {
    return Napi::String::New(env, reinterpret_cast<const char16_t *>(value));
  };

  // --- Standardized fields ---
  result_printer_job.Set(Napi::String::New(env, "id"),
                         Napi::Number::New(env, job->JobId));

  // name = document name (pDocument), not printer name
  if (job->pDocument && *job->pDocument != L'\0') {
    result_printer_job.Set(Napi::String::New(env, "name"),
                           wstrToNapi(job->pDocument));
  } else {
    result_printer_job.Set(Napi::String::New(env, "name"),
                           Napi::String::New(env, ""));
  }

  if (job->pPrinterName && *job->pPrinterName != L'\0') {
    result_printer_job.Set(Napi::String::New(env, "printerName"),
                           wstrToNapi(job->pPrinterName));
  }

  if (job->pUserName && *job->pUserName != L'\0') {
    result_printer_job.Set(Napi::String::New(env, "user"),
                           wstrToNapi(job->pUserName));
  } else {
    result_printer_job.Set(Napi::String::New(env, "user"),
                           Napi::String::New(env, ""));
  }

  result_printer_job.Set(Napi::String::New(env, "size"),
                         Napi::Number::New(env, job->Size));

  // state (IPP job-state keyword)
  result_printer_job.Set(Napi::String::New(env, "state"),
                         Napi::String::New(env, jobStatusToIppState(job->Status)));

  // Timestamps as epoch seconds
  double createdAt = systemTimeToEpoch(job->Submitted);
  result_printer_job.Set(Napi::String::New(env, "createdAt"),
                         Napi::Number::New(env, createdAt));
  result_printer_job.Set(Napi::String::New(env, "processingAt"),
                         Napi::Number::New(env, 0.0));
  result_printer_job.Set(Napi::String::New(env, "completedAt"),
                         Napi::Number::New(env, 0.0));

  // --- Platform-specific raw ---
  Napi::Object raw = Napi::Object::New(env);

  // Raw status strings
  Napi::Array raw_status = Napi::Array::New(env);
  int i_status = 0;
  for (auto &entry : getJobStatusMap()) {
    if (job->Status & entry.second) {
      raw_status.Set(i_status++,
                     Napi::String::New(env, entry.first.c_str()));
    }
  }
  if (job->pStatus && *job->pStatus != L'\0') {
    raw_status.Set(i_status++, wstrToNapi(job->pStatus));
  }
  raw.Set(Napi::String::New(env, "status"), raw_status);
  raw.Set(Napi::String::New(env, "statusNumber"),
          Napi::Number::New(env, job->Status));

  if (job->pDatatype && *job->pDatatype != L'\0') {
    raw.Set(Napi::String::New(env, "datatype"), wstrToNapi(job->pDatatype));
  }
  raw.Set(Napi::String::New(env, "priority"),
          Napi::Number::New(env, job->Priority));
  raw.Set(Napi::String::New(env, "position"),
          Napi::Number::New(env, job->Position));
  raw.Set(Napi::String::New(env, "totalPages"),
          Napi::Number::New(env, job->TotalPages));
  raw.Set(Napi::String::New(env, "pagesPrinted"),
          Napi::Number::New(env, job->PagesPrinted));

  auto addRawString = [&](const char *name, LPCWSTR value) {
    if (value && *value != L'\0') {
      raw.Set(name, wstrToNapi(value));
    }
  };
  addRawString("machineName", job->pMachineName);
  addRawString("driverName", job->pDriverName);
  addRawString("printProcessor", job->pPrintProcessor);
  addRawString("notifyName", job->pNotifyName);

  result_printer_job.Set(Napi::String::New(env, "raw"), raw);
}

/**
 * Returns last error code and message string
 */
std::string getLastErrorCodeAndMessage() {
  std::ostringstream s;
  DWORD erroCode = GetLastError();
  s << "code: " << erroCode;
  DWORD retSize;
  LPTSTR pTemp = NULL;
  retSize = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_ARGUMENT_ARRAY,
      NULL, erroCode, LANG_NEUTRAL, (LPTSTR)&pTemp, 0, NULL);
  if (retSize && pTemp != NULL) {
    // pTemp[strlen(pTemp)-2]='\0'; //remove cr and newline character
    // TODO: check if it is needed to convert c string to std::string
    std::string stringMessage(pTemp);
    s << ", message: " << stringMessage;
    LocalFree((HLOCAL)pTemp);
  }

  return s.str();
}

std::string retrieveAndParseJobs(const LPWSTR iPrinterName,
                                 const DWORD &iTotalJobs,
                                 Napi::Object result_printer_jobs,
                                 PrinterHandle &iPrinterHandle) {
  Napi::Env env = result_printer_jobs.Env();
  DWORD bytes_needed = 0, totalJobs = 0;
  BOOL bError = EnumJobsW(*iPrinterHandle, 0, iTotalJobs, 2, NULL, bytes_needed,
                          &bytes_needed, &totalJobs);
  auto jobs = mallocValue<JOB_INFO_2W>(bytes_needed);
  if (!jobs) {
    std::string error_str("Error on allocating memory for jobs: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Object result_printer_job = Napi::Object::New(env);
    result_printer_job.Set(Napi::String::New(env, "error"),
                           Napi::String::New(env, error_str.c_str()));
    result_printer_jobs.Set((uint32_t)0, result_printer_job);
    return std::string("");
  }
  DWORD dummy_bytes = 0;
  bError = EnumJobsW(*iPrinterHandle, 0, iTotalJobs, 2, (LPBYTE)jobs.get(),
                     bytes_needed, &dummy_bytes, &totalJobs);
  if (!bError) {
    std::string error_str("Error on EnumJobsW: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Object result_printer_job = Napi::Object::New(env);
    result_printer_job.Set(Napi::String::New(env, "error"),
                           Napi::String::New(env, error_str.c_str()));
    result_printer_jobs.Set((uint32_t)0, result_printer_job);
    return std::string("");
  }
  JOB_INFO_2W *job = jobs.get();
  for (DWORD i = 0; i < totalJobs; ++i, ++job) {
    Napi::Object result_printer_job = Napi::Object::New(env);
    parseJobObject(job, result_printer_job);
    result_printer_jobs.Set(i, result_printer_job);
  }
  return std::string("");
}

std::string parsePrinterInfo(const PRINTER_INFO_2W *printer,
                             Napi::Object result_printer,
                             PrinterHandle &iPrinterHandle) {
  Napi::Env env = result_printer.Env();
  auto wstrToNapi = [&](LPCWSTR value) -> Napi::String {
    return Napi::String::New(env, reinterpret_cast<const char16_t *>(value));
  };

  // --- Standardized fields ---

  // name
  if (printer->pPrinterName) {
    result_printer.Set(Napi::String::New(env, "name"),
                       wstrToNapi(printer->pPrinterName));
  }

  // isDefault
  std::u16string defaultName = getDefaultPrinterName();
  bool isDefault = (printer->pPrinterName != nullptr) &&
                   (defaultName == reinterpret_cast<const char16_t *>(
                                       printer->pPrinterName));
  result_printer.Set(Napi::String::New(env, "isDefault"),
                     Napi::Boolean::New(env, isDefault));

  // state
  const char *state_str = "idle";
  if (printer->Status & (PRINTER_STATUS_PRINTING | PRINTER_STATUS_PROCESSING)) {
    state_str = "processing";
  } else if (printer->Status & kStoppedMask) {
    state_str = "stopped";
  }
  result_printer.Set(Napi::String::New(env, "state"),
                     Napi::String::New(env, state_str));

  // stateReasons
  Napi::Array state_reasons = Napi::Array::New(env);
  uint32_t reason_idx = 0;
  if (printer->Status == 0) {
    state_reasons.Set(reason_idx++, Napi::String::New(env, "none"));
  } else {
    for (auto &entry : getIppReasonMap()) {
      if (printer->Status & entry.first) {
        state_reasons.Set(reason_idx++,
                          Napi::String::New(env, entry.second));
      }
    }
    if (reason_idx == 0) {
      // Has status bits set but none map to IPP reasons (e.g. only PRINTING)
      state_reasons.Set(reason_idx++, Napi::String::New(env, "none"));
    }
  }
  result_printer.Set(Napi::String::New(env, "stateReasons"), state_reasons);

  // jobs
  Napi::Array result_printer_jobs = Napi::Array::New(env);
  if (printer->cJobs > 0) {
    std::string error_str =
        retrieveAndParseJobs(printer->pPrinterName, printer->cJobs,
                             result_printer_jobs, iPrinterHandle);
    if (!error_str.empty()) {
      return error_str;
    }
  }
  result_printer.Set(Napi::String::New(env, "jobs"), result_printer_jobs);

  // --- Platform-specific raw fields ---
  Napi::Object raw = Napi::Object::New(env);

  auto addRawString = [&](const char *name, LPCWSTR value) {
    if ((value != NULL) && (*value != L'\0')) {
      raw.Set(name, wstrToNapi(value));
    }
  };

  addRawString("serverName", printer->pServerName);
  addRawString("shareName", printer->pShareName);
  addRawString("portName", printer->pPortName);
  addRawString("driverName", printer->pDriverName);
  addRawString("comment", printer->pComment);
  addRawString("location", printer->pLocation);
  addRawString("sepFile", printer->pSepFile);
  addRawString("printProcessor", printer->pPrintProcessor);
  addRawString("datatype", printer->pDatatype);
  addRawString("parameters", printer->pParameters);

  // Raw status info
  Napi::Array raw_status = Napi::Array::New(env);
  int i_status = 0;
  for (auto &itStatus : getStatusMap()) {
    if (printer->Status & itStatus.second) {
      raw_status.Set(i_status++,
                     Napi::String::New(env, itStatus.first.c_str()));
    }
  }
  raw.Set(Napi::String::New(env, "status"), raw_status);
  raw.Set(Napi::String::New(env, "statusNumber"),
          Napi::Number::New(env, printer->Status));

  // Attributes
  Napi::Array raw_attributes = Napi::Array::New(env);
  int i_attribute = 0;
  for (auto &itAttribute : getAttributeMap()) {
    if (printer->Attributes & itAttribute.second) {
      raw_attributes.Set(i_attribute++,
                         Napi::String::New(env, itAttribute.first.c_str()));
    }
  }
  raw.Set(Napi::String::New(env, "attributes"), raw_attributes);

  raw.Set(Napi::String::New(env, "priority"),
          Napi::Number::New(env, printer->Priority));
  raw.Set(Napi::String::New(env, "defaultPriority"),
          Napi::Number::New(env, printer->DefaultPriority));
  raw.Set(Napi::String::New(env, "averagePPM"),
          Napi::Number::New(env, printer->AveragePPM));

  if (printer->StartTime > 0) {
    raw.Set(Napi::String::New(env, "startTime"),
            Napi::Number::New(env, printer->StartTime));
  }
  if (printer->UntilTime > 0) {
    raw.Set(Napi::String::New(env, "untilTime"),
            Napi::Number::New(env, printer->UntilTime));
  }

  result_printer.Set(Napi::String::New(env, "raw"), raw);
  return "";
}
} // namespace

Napi::Value getPrinters(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  DWORD printers_size = 0;
  DWORD printers_size_bytes = 0, dummyBytes = 0;
  DWORD Level = 2;
  DWORD flags =
      PRINTER_ENUM_LOCAL |
      PRINTER_ENUM_CONNECTIONS; // https://msdn.microsoft.com/en-us/library/cc244669.aspx
  // First try to retrieve the number of printers
  BOOL bError = EnumPrintersW(flags, NULL, 2, NULL, 0, &printers_size_bytes,
                              &printers_size);
  // allocate the required memmory
  auto printers = mallocValue<PRINTER_INFO_2W>(printers_size_bytes);
  if (!printers) {
    Napi::Error::New(env, "Error on allocating memory for printers")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  bError = EnumPrintersW(flags, NULL, 2, (LPBYTE)(printers.get()),
                         printers_size_bytes, &dummyBytes, &printers_size);
  if (!bError) {
    std::string error_str("Error on EnumPrinters: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Array result = Napi::Array::New(env, printers_size);
  // http://msdn.microsoft.com/en-gb/library/windows/desktop/dd162845(v=vs.85).aspx
  PRINTER_INFO_2W *printer = printers.get();
  DWORD i = 0;
  for (; i < printers_size; ++i, ++printer) {
    Napi::Object result_printer = Napi::Object::New(env);
    PrinterHandle printerHandle((LPWSTR)(printer->pPrinterName));
    std::string error_str =
        parsePrinterInfo(printer, result_printer, printerHandle);
    if (!error_str.empty()) {
      Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
    result.Set(i, result_printer);
  }
  return result;
}

Napi::Value getPrinter(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 1) {
    Napi::Error::New(env, "Expected 1 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::u16string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf16Value();

  // Open a handle to the printer.
  PrinterHandle printerHandle(
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printername.c_str())));
  if (!printerHandle) {
    std::string error_str("error on PrinterHandle: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  DWORD printers_size_bytes = 0, dummyBytes = 0;
  GetPrinterW(*printerHandle, 2, NULL, printers_size_bytes,
              &printers_size_bytes);
  auto printer = mallocValue<PRINTER_INFO_2W>(printers_size_bytes);
  if (!printer) {
    Napi::Error::New(env, "Error on allocating memory for printers")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  BOOL bOK = GetPrinterW(*printerHandle, 2, (LPBYTE)(printer.get()),
                         printers_size_bytes, &printers_size_bytes);
  if (!bOK) {
    std::string error_str("Error on GetPrinter: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Object result_printer = Napi::Object::New(env);
  std::string error_str =
      parsePrinterInfo(printer.get(), result_printer, printerHandle);
  if (!error_str.empty()) {
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return result_printer;
}

Napi::Value getJob(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 2) {
    Napi::Error::New(env, "Expected 2 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::u16string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf16Value();
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
  // Open a handle to the printer.
  PrinterHandle printerHandle(
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printername.c_str())));
  if (!printerHandle) {
    std::string error_str("error on PrinterHandle: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  DWORD size_bytes = 0, dummyBytes = 0;
  GetJobW(*printerHandle, static_cast<DWORD>(jobId), 2, NULL, size_bytes,
          &size_bytes);
  auto job = mallocValue<JOB_INFO_2W>(size_bytes);
  if (!job) {
    Napi::Error::New(env, "Error on allocating memory for printers")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  BOOL bOK = GetJobW(*printerHandle, static_cast<DWORD>(jobId), 2,
                     (LPBYTE)job.get(), size_bytes, &dummyBytes);
  if (!bOK) {
    std::string error_str("Error on GetJob. Wrong job id or it was deleted: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Object result_printer_job = Napi::Object::New(env);
  parseJobObject(job.get(), result_printer_job);
  return result_printer_job;
}

Napi::Value setJob(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  if (iArgs.Length() < 3) {
    Napi::Error::New(env, "Expected 3 arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  std::u16string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf16Value();
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
  StatusMapType::const_iterator itJobCommand =
      getJobCommandMap().find(jobCommandStr);
  if (itJobCommand == getJobCommandMap().end()) {
    Napi::Error::New(env, "wrong job command. use getSupportedJobCommands to "
                          "see the possible commands")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  DWORD jobCommand = itJobCommand->second;
  // Open a handle to the printer.
  PrinterHandle printerHandle(
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printername.c_str())));
  if (!printerHandle) {
    std::string error_str("error on PrinterHandle: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  // TODO: add the possibility to set job properties
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd162978(v=vs.85).aspx
  BOOL ok = SetJobW(*printerHandle, (DWORD)jobId, 0, NULL, jobCommand);
  return Napi::Boolean::New(env, ok == TRUE);
}

Napi::Value getSupportedJobCommands(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Array result = Napi::Array::New(env);
  int i = 0;
  for (StatusMapType::const_iterator itJob = getJobCommandMap().begin();
       itJob != getJobCommandMap().end(); ++itJob) {
    result.Set(i++, Napi::String::New(env, itJob->first.c_str()));
  }
  return result;
}

Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Array result = Napi::Array::New(env);
  int format_i = 0;

  LPTSTR name = NULL;
  DWORD numBytes = 0, processorsNum = 0;

  // Check the amount of bytes required
  LPWSTR nullVal = NULL;
  EnumPrintProcessorsW(nullVal, nullVal, 1, (LPBYTE)(NULL), numBytes, &numBytes,
                       &processorsNum);
  auto processors = mallocValue<_PRINTPROCESSOR_INFO_1W>(numBytes);
  // Retrieve processors
  BOOL isOK =
      EnumPrintProcessorsW(nullVal, nullVal, 1, (LPBYTE)(processors.get()),
                           numBytes, &numBytes, &processorsNum);

  if (!isOK) {
    std::string error_str("error on EnumPrintProcessorsW: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  _PRINTPROCESSOR_INFO_1W *pProcessor = processors.get();

  for (DWORD processor_i = 0; processor_i < processorsNum;
       ++processor_i, ++pProcessor) {
    numBytes = 0;
    DWORD dataTypesNum = 0;
    EnumPrintProcessorDatatypesW(nullVal, pProcessor->pName, 1, (LPBYTE)(NULL),
                                 numBytes, &numBytes, &dataTypesNum);
    auto dataTypes = mallocValue<_DATATYPES_INFO_1W>(numBytes);
    isOK = EnumPrintProcessorDatatypesW(nullVal, pProcessor->pName, 1,
                                        (LPBYTE)(dataTypes.get()), numBytes,
                                        &numBytes, &dataTypesNum);

    if (!isOK) {
      std::string error_str("error on EnumPrintProcessorDatatypesW: ");
      error_str += getLastErrorCodeAndMessage();
      Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
      return env.Undefined();
    }

    _DATATYPES_INFO_1W *pDataType = dataTypes.get();
    for (DWORD j = 0; j < dataTypesNum; ++j, ++pDataType) {
      result.Set(format_i++,
                 Napi::String::New(env, reinterpret_cast<const char16_t *>(
                                            pDataType->pName)));
    }
  }

  return result;
}

Napi::Value PrintDirect(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  // TODO: to move in an unique place win and posix input parameters processing
  if (iArgs.Length() < 4) {
    Napi::Error::New(env, "Expected 4 arguments").ThrowAsJavaScriptException();
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

  std::u16string printername;
  if (!iArgs[1].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[1].As<Napi::String>().Utf16Value();
  std::u16string docname;
  if (!iArgs[2].IsString()) {
    Napi::Error::New(env, "Document name must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  docname = iArgs[2].As<Napi::String>().Utf16Value();
  std::u16string type;
  if (!iArgs[3].IsString()) {
    Napi::Error::New(env, "Type must be a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  type = iArgs[3].As<Napi::String>().Utf16Value();

  BOOL bStatus = true;
  // Open a handle to the printer.
  PrinterHandle printerHandle(
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printername.c_str())));
  DOC_INFO_1W DocInfo;
  DWORD dwJob = 0L;
  DWORD dwBytesWritten = 0L;

  if (!printerHandle) {
    std::string error_str("error on PrinterHandle: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Fill in the structure with info about this "document."
  DocInfo.pDocName =
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(docname.c_str()));
  DocInfo.pOutputFile = NULL;
  DocInfo.pDatatype =
      reinterpret_cast<LPWSTR>(const_cast<char16_t *>(type.c_str()));

  // Inform the spooler the document is beginning.
  dwJob = StartDocPrinterW(*printerHandle, 1, (LPBYTE)&DocInfo);
  if (dwJob > 0) {
    // Start a page.
    bStatus = StartPagePrinter(*printerHandle);
    if (bStatus) {
      // Send the data to the printer.
      // TODO: check with sizeof(LPTSTR) is the same as sizeof(char)
      bStatus = WritePrinter(*printerHandle, (LPVOID)(data.c_str()),
                             (DWORD)data.size(), &dwBytesWritten);
      EndPagePrinter(*printerHandle);
    } else {
      std::string error_str("StartPagePrinter error: ");
      error_str += getLastErrorCodeAndMessage();
      Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
    // Inform the spooler that the document is ending.
    EndDocPrinter(*printerHandle);
  } else {
    std::string error_str("StartDocPrinterW error: ");
    error_str += getLastErrorCodeAndMessage();
    Napi::Error::New(env, error_str.c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
  // Check to see if correct number of bytes were written.
  if (dwBytesWritten != data.size()) {
    Napi::Error::New(env, "not sent all bytes").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return Napi::Number::New(env, dwJob);
}

Napi::Value PrintFile(const Napi::CallbackInfo &iArgs) {
  Napi::Env env = iArgs.Env();
  Napi::Error::New(env, "Not yet implemented on Windows")
      .ThrowAsJavaScriptException();
  return env.Undefined();
}
