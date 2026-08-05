#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Winspool.h>
#pragma comment(lib, "Winspool.lib")
#endif

#include "node_printer.hpp"
#include "printer_model.hpp"

#include <map>
#include <node_version.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <memory>

namespace {
typedef std::map<std::string, DWORD> StatusMapType;

std::string getLastErrorCodeAndMessage();

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
  static const StatusMapType result = [] {
    StatusMapType map;
    map.insert(std::make_pair("BUSY", PRINTER_STATUS_BUSY));
    map.insert(std::make_pair("DOOR-OPEN", PRINTER_STATUS_DOOR_OPEN));
    map.insert(std::make_pair("DRIVER_UPDATE_NEEDED",
                              PRINTER_STATUS_DRIVER_UPDATE_NEEDED));
    map.insert(std::make_pair("ERROR", PRINTER_STATUS_ERROR));
    map.insert(std::make_pair("INITIALIZING", PRINTER_STATUS_INITIALIZING));
    map.insert(std::make_pair("IO-ACTIVE", PRINTER_STATUS_IO_ACTIVE));
    map.insert(std::make_pair("MANUAL-FEED", PRINTER_STATUS_MANUAL_FEED));
    map.insert(std::make_pair("NO-TONER", PRINTER_STATUS_NO_TONER));
    map.insert(std::make_pair("NOT-AVAILABLE", PRINTER_STATUS_NOT_AVAILABLE));
    map.insert(std::make_pair("OFFLINE", PRINTER_STATUS_OFFLINE));
    map.insert(std::make_pair("OUT-OF-MEMORY", PRINTER_STATUS_OUT_OF_MEMORY));
    map.insert(
        std::make_pair("OUTPUT-BIN-FULL", PRINTER_STATUS_OUTPUT_BIN_FULL));
    map.insert(std::make_pair("PAGE-PUNT", PRINTER_STATUS_PAGE_PUNT));
    map.insert(std::make_pair("PAPER-JAM", PRINTER_STATUS_PAPER_JAM));
    map.insert(std::make_pair("PAPER-OUT", PRINTER_STATUS_PAPER_OUT));
    map.insert(std::make_pair("PAPER-PROBLEM", PRINTER_STATUS_PAPER_PROBLEM));
    map.insert(std::make_pair("PAUSED", PRINTER_STATUS_PAUSED));
    map.insert(
        std::make_pair("PENDING-DELETION", PRINTER_STATUS_PENDING_DELETION));
    map.insert(std::make_pair("POWER-SAVE", PRINTER_STATUS_POWER_SAVE));
    map.insert(std::make_pair("PRINTING", PRINTER_STATUS_PRINTING));
    map.insert(std::make_pair("PROCESSING", PRINTER_STATUS_PROCESSING));
    map.insert(std::make_pair("SERVER-OFFLINE", PRINTER_STATUS_SERVER_OFFLINE));
    map.insert(std::make_pair("SERVER-UNKNOWN", PRINTER_STATUS_SERVER_UNKNOWN));
    map.insert(std::make_pair("TONER-LOW", PRINTER_STATUS_TONER_LOW));
    map.insert(
        std::make_pair("USER-INTERVENTION", PRINTER_STATUS_USER_INTERVENTION));
    map.insert(std::make_pair("WAITING", PRINTER_STATUS_WAITING));
    map.insert(std::make_pair("WARMING-UP", PRINTER_STATUS_WARMING_UP));
    return map;
  }();
  return result;
}

/// Map Windows status bits to IPP printer-state-reasons keywords
typedef std::map<DWORD, std::string> IppReasonMapType;

const IppReasonMapType &getIppReasonMap() {
  static const IppReasonMapType result = [] {
    IppReasonMapType map;
    map[PRINTER_STATUS_PAPER_JAM] = "media-jam";
    map[PRINTER_STATUS_PAPER_OUT] = "media-empty";
    map[PRINTER_STATUS_PAPER_PROBLEM] = "media-empty";
    map[PRINTER_STATUS_MANUAL_FEED] = "media-needed";
    map[PRINTER_STATUS_NO_TONER] = "toner-empty";
    map[PRINTER_STATUS_TONER_LOW] = "toner-low";
    map[PRINTER_STATUS_DOOR_OPEN] = "door-open";
    map[PRINTER_STATUS_OUTPUT_BIN_FULL] = "output-area-full";
    map[PRINTER_STATUS_OFFLINE] = "offline";
    map[PRINTER_STATUS_NOT_AVAILABLE] = "offline";
    map[PRINTER_STATUS_SERVER_OFFLINE] = "offline";
    map[PRINTER_STATUS_PAUSED] = "paused";
    map[PRINTER_STATUS_ERROR] = "other";
    map[PRINTER_STATUS_USER_INTERVENTION] = "other";
    map[PRINTER_STATUS_OUT_OF_MEMORY] = "other";
    map[PRINTER_STATUS_PAGE_PUNT] = "other";
    return map;
  }();
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
std::u16string getDefaultPrinterNameUtf16() {
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
  static const StatusMapType result = [] {
    StatusMapType map;
    // Common statuses
    map.insert(std::make_pair("PRINTING", JOB_STATUS_PRINTING));
    map.insert(std::make_pair("PRINTED", JOB_STATUS_PRINTED));
    map.insert(std::make_pair("PAUSED", JOB_STATUS_PAUSED));

    // Specific statuses
    map.insert(std::make_pair("BLOCKED-DEVQ", JOB_STATUS_BLOCKED_DEVQ));
    map.insert(std::make_pair("DELETED", JOB_STATUS_DELETED));
    map.insert(std::make_pair("DELETING", JOB_STATUS_DELETING));
    map.insert(std::make_pair("ERROR", JOB_STATUS_ERROR));
    map.insert(std::make_pair("OFFLINE", JOB_STATUS_OFFLINE));
    map.insert(std::make_pair("PAPEROUT", JOB_STATUS_PAPEROUT));
    map.insert(std::make_pair("RESTART", JOB_STATUS_RESTART));
    map.insert(std::make_pair("SPOOLING", JOB_STATUS_SPOOLING));
    map.insert(
        std::make_pair("USER-INTERVENTION", JOB_STATUS_USER_INTERVENTION));

    // XP and later
#ifdef JOB_STATUS_COMPLETE
    map.insert(std::make_pair("COMPLETE", JOB_STATUS_COMPLETE));
#endif
#ifdef JOB_STATUS_RETAINED
    map.insert(std::make_pair("RETAINED", JOB_STATUS_RETAINED));
#endif

    return map;
  }();
  return result;
}

const StatusMapType &getAttributeMap() {
  static const StatusMapType result = [] {
    StatusMapType map;
    map.insert(std::make_pair("DIRECT", PRINTER_ATTRIBUTE_DIRECT));
    map.insert(std::make_pair("DO-COMPLETE-FIRST",
                              PRINTER_ATTRIBUTE_DO_COMPLETE_FIRST));
    map.insert(std::make_pair("ENABLE-BIDI", PRINTER_ATTRIBUTE_ENABLE_BIDI));
    map.insert(std::make_pair("ENABLE-DEVQ", PRINTER_ATTRIBUTE_ENABLE_DEVQ));
    map.insert(std::make_pair("HIDDEN", PRINTER_ATTRIBUTE_HIDDEN));
    map.insert(
        std::make_pair("KEEPPRINTEDJOBS", PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS));
    map.insert(std::make_pair("LOCAL", PRINTER_ATTRIBUTE_LOCAL));
    map.insert(std::make_pair("NETWORK", PRINTER_ATTRIBUTE_NETWORK));
    map.insert(std::make_pair("PUBLISHED", PRINTER_ATTRIBUTE_PUBLISHED));
    map.insert(std::make_pair("QUEUED", PRINTER_ATTRIBUTE_QUEUED));
    map.insert(std::make_pair("RAW-ONLY", PRINTER_ATTRIBUTE_RAW_ONLY));
    map.insert(std::make_pair("SHARED", PRINTER_ATTRIBUTE_SHARED));
    map.insert(std::make_pair("OFFLINE", PRINTER_ATTRIBUTE_WORK_OFFLINE));
    // XP
#ifdef PRINTER_ATTRIBUTE_FAX
    map.insert(std::make_pair("FAX", PRINTER_ATTRIBUTE_FAX));
#endif
    // vista
#ifdef PRINTER_ATTRIBUTE_FRIENDLY_NAME
    map.insert(
        std::make_pair("FRIENDLY-NAME", PRINTER_ATTRIBUTE_FRIENDLY_NAME));
    map.insert(std::make_pair("MACHINE", PRINTER_ATTRIBUTE_MACHINE));
    map.insert(std::make_pair("PUSHED-USER", PRINTER_ATTRIBUTE_PUSHED_USER));
    map.insert(
        std::make_pair("PUSHED-MACHINE", PRINTER_ATTRIBUTE_PUSHED_MACHINE));
    map.insert(std::make_pair("TS_GENERIC_DRIVER",
                              PRINTER_ATTRIBUTE_TS_GENERIC_DRIVER));
#endif
    // server 2003
#ifdef PRINTER_ATTRIBUTE_TS
    map.insert(std::make_pair("TS", PRINTER_ATTRIBUTE_TS));
#endif
    return map;
  }();
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
  if (status &
      (JOB_STATUS_PAUSED | JOB_STATUS_BLOCKED_DEVQ | JOB_STATUS_OFFLINE |
       JOB_STATUS_PAPEROUT | JOB_STATUS_USER_INTERVENTION))
    return "processing-stopped";
  if (status & JOB_STATUS_SPOOLING)
    return "pending";
  return "pending";
}

std::string wstrToUtf8(LPCWSTR value) {
  if (!value || *value == L'\0') {
    return std::string();
  }
  int required =
      WideCharToMultiByte(CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL);
  if (required <= 1) {
    return std::string();
  }
  std::string out(static_cast<size_t>(required - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), required, NULL, NULL);
  return out;
}

using JobData = printer_model::JobModel;
using PrinterData = printer_model::PrinterModel;

JobData parseJobData(const JOB_INFO_2W *job) {
  JobData out;
  out.id = static_cast<int>(job->JobId);
  out.name = wstrToUtf8(job->pDocument);
  out.printerName = wstrToUtf8(job->pPrinterName);
  out.user = wstrToUtf8(job->pUserName);
  out.state = jobStatusToIppState(job->Status);
  out.size = static_cast<double>(job->Size);
  out.createdAt = systemTimeToEpoch(job->Submitted);
  out.processingAt = 0;
  out.completedAt = 0;

  std::vector<std::string> status;
  for (auto &entry : getJobStatusMap()) {
    if (job->Status & entry.second) {
      status.push_back(entry.first);
    }
  }
  if (job->pStatus && *job->pStatus != L'\0') {
    status.push_back(wstrToUtf8(job->pStatus));
  }
  out.raw["status"] =
      printer_model::RawValue::FromStringArray(std::move(status));
  out.raw["statusNumber"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->Status));
  if (job->pDatatype && *job->pDatatype != L'\0') {
    out.raw["datatype"] =
        printer_model::RawValue::FromString(wstrToUtf8(job->pDatatype));
  }
  out.raw["priority"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->Priority));
  out.raw["position"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->Position));
  out.raw["totalPages"] =
      printer_model::RawValue::FromNumber(static_cast<double>(job->TotalPages));
  out.raw["pagesPrinted"] = printer_model::RawValue::FromNumber(
      static_cast<double>(job->PagesPrinted));
  if (job->pMachineName && *job->pMachineName != L'\0') {
    out.raw["machineName"] =
        printer_model::RawValue::FromString(wstrToUtf8(job->pMachineName));
  }
  if (job->pDriverName && *job->pDriverName != L'\0') {
    out.raw["driverName"] =
        printer_model::RawValue::FromString(wstrToUtf8(job->pDriverName));
  }
  if (job->pPrintProcessor && *job->pPrintProcessor != L'\0') {
    out.raw["printProcessor"] =
        printer_model::RawValue::FromString(wstrToUtf8(job->pPrintProcessor));
  }
  if (job->pNotifyName && *job->pNotifyName != L'\0') {
    out.raw["notifyName"] =
        printer_model::RawValue::FromString(wstrToUtf8(job->pNotifyName));
  }
  return out;
}

std::string retrieveJobsData(const DWORD totalJobs,
                             PrinterHandle &printerHandle,
                             std::vector<JobData> &outJobs) {
  DWORD bytes_needed = 0, jobs_count = 0;
  EnumJobsW(*printerHandle, 0, totalJobs, 2, NULL, bytes_needed, &bytes_needed,
            &jobs_count);
  auto jobs = mallocValue<JOB_INFO_2W>(bytes_needed);
  if (!jobs) {
    return "Failed to allocate memory for jobs";
  }
  DWORD dummy_bytes = 0;
  BOOL ok = EnumJobsW(*printerHandle, 0, totalJobs, 2, (LPBYTE)jobs.get(),
                      bytes_needed, &dummy_bytes, &jobs_count);
  if (!ok) {
    std::string error_str("EnumJobsW failed: ");
    error_str += getLastErrorCodeAndMessage();
    return error_str;
  }
  JOB_INFO_2W *job = jobs.get();
  for (DWORD i = 0; i < jobs_count; ++i, ++job) {
    outJobs.push_back(parseJobData(job));
  }
  return "";
}

std::string parsePrinterData(const PRINTER_INFO_2W *printer,
                             PrinterHandle &printerHandle,
                             const std::u16string &defaultName,
                             PrinterData &out) {
  out.name = wstrToUtf8(printer->pPrinterName);
  out.isDefault = (printer->pPrinterName != nullptr) &&
                  (defaultName ==
                   reinterpret_cast<const char16_t *>(printer->pPrinterName));

  if (printer->Status & (PRINTER_STATUS_PRINTING | PRINTER_STATUS_PROCESSING)) {
    out.state = "processing";
  } else if (printer->Status & kStoppedMask) {
    out.state = "stopped";
  }

  if (printer->Status == 0) {
    out.stateReasons.push_back("none");
  } else {
    for (auto &entry : getIppReasonMap()) {
      if (printer->Status & entry.first) {
        out.stateReasons.push_back(entry.second);
      }
    }
    if (out.stateReasons.empty()) {
      out.stateReasons.push_back("none");
    }
  }

  if (printer->cJobs > 0) {
    std::string error =
        retrieveJobsData(printer->cJobs, printerHandle, out.jobs);
    if (!error.empty()) {
      return error;
    }
  }

  if (printer->pServerName && *printer->pServerName != L'\0') {
    out.raw["serverName"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pServerName));
  }
  if (printer->pShareName && *printer->pShareName != L'\0') {
    out.raw["shareName"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pShareName));
  }
  if (printer->pPortName && *printer->pPortName != L'\0') {
    out.raw["portName"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pPortName));
  }
  if (printer->pDriverName && *printer->pDriverName != L'\0') {
    out.raw["driverName"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pDriverName));
  }
  if (printer->pComment && *printer->pComment != L'\0') {
    out.raw["comment"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pComment));
  }
  if (printer->pLocation && *printer->pLocation != L'\0') {
    out.raw["location"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pLocation));
  }
  if (printer->pSepFile && *printer->pSepFile != L'\0') {
    out.raw["sepFile"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pSepFile));
  }
  if (printer->pPrintProcessor && *printer->pPrintProcessor != L'\0') {
    out.raw["printProcessor"] = printer_model::RawValue::FromString(
        wstrToUtf8(printer->pPrintProcessor));
  }
  if (printer->pDatatype && *printer->pDatatype != L'\0') {
    out.raw["datatype"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pDatatype));
  }
  if (printer->pParameters && *printer->pParameters != L'\0') {
    out.raw["parameters"] =
        printer_model::RawValue::FromString(wstrToUtf8(printer->pParameters));
  }

  std::vector<std::string> rawStatus;
  for (auto &entry : getStatusMap()) {
    if (printer->Status & entry.second) {
      rawStatus.push_back(entry.first);
    }
  }
  out.raw["status"] =
      printer_model::RawValue::FromStringArray(std::move(rawStatus));
  out.raw["statusNumber"] =
      printer_model::RawValue::FromNumber(static_cast<double>(printer->Status));

  std::vector<std::string> rawAttributes;
  for (auto &entry : getAttributeMap()) {
    if (printer->Attributes & entry.second) {
      rawAttributes.push_back(entry.first);
    }
  }
  out.raw["attributes"] =
      printer_model::RawValue::FromStringArray(std::move(rawAttributes));

  out.raw["priority"] = printer_model::RawValue::FromNumber(
      static_cast<double>(printer->Priority));
  out.raw["defaultPriority"] = printer_model::RawValue::FromNumber(
      static_cast<double>(printer->DefaultPriority));
  out.raw["averagePPM"] = printer_model::RawValue::FromNumber(
      static_cast<double>(printer->AveragePPM));
  if (printer->StartTime > 0) {
    out.raw["startTime"] = printer_model::RawValue::FromNumber(
        static_cast<double>(printer->StartTime));
  }
  if (printer->UntilTime > 0) {
    out.raw["untilTime"] = printer_model::RawValue::FromNumber(
        static_cast<double>(printer->UntilTime));
  }
  return "";
}

class GetAllPrinterDetailsWorker : public PromiseWorker {
public:
  explicit GetAllPrinterDetailsWorker(Napi::Env env) : PromiseWorker(env) {}

  void Execute() override {
    DWORD printers_size = 0;
    DWORD printers_size_bytes = 0, dummyBytes = 0;
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    EnumPrintersW(flags, NULL, 2, NULL, 0, &printers_size_bytes,
                  &printers_size);
    auto printers = mallocValue<PRINTER_INFO_2W>(printers_size_bytes);
    if (!printers) {
      SetError("Failed to allocate memory for printers");
      return;
    }
    BOOL ok = EnumPrintersW(flags, NULL, 2, (LPBYTE)printers.get(),
                            printers_size_bytes, &dummyBytes, &printers_size);
    if (!ok) {
      std::string err("EnumPrintersW failed: ");
      err += getLastErrorCodeAndMessage();
      SetError(err);
      return;
    }
    std::u16string defaultName = getDefaultPrinterNameUtf16();
    PRINTER_INFO_2W *printer = printers.get();
    for (DWORD i = 0; i < printers_size; ++i, ++printer) {
      PrinterHandle handle((LPWSTR)printer->pPrinterName);
      if (!handle) {
        continue;
      }
      PrinterData out;
      std::string err = parsePrinterData(printer, handle, defaultName, out);
      if (!err.empty()) {
        SetError(err);
        return;
      }
      result_.push_back(std::move(out));
    }
  }

  void OnOK() override {
    Napi::Array arr = Napi::Array::New(Env(), result_.size());
    for (size_t i = 0; i < result_.size(); ++i) {
      arr.Set(static_cast<uint32_t>(i),
              printer_model::SerializePrinterModel(Env(), result_[i]));
    }
    deferred_.Resolve(arr);
  }

private:
  std::vector<PrinterData> result_;
};

class GetPrinterDetailsWorker : public PromiseWorker {
public:
  GetPrinterDetailsWorker(Napi::Env env, std::u16string printerName)
      : PromiseWorker(env), printerName_(std::move(printerName)) {}

  void Execute() override {
    PrinterHandle handle(
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printerName_.c_str())));
    if (!handle) {
      found_ = false;
      return;
    }
    DWORD size = 0;
    GetPrinterW(*handle, 2, NULL, size, &size);
    auto printer = mallocValue<PRINTER_INFO_2W>(size);
    if (!printer) {
      SetError("Failed to allocate memory for printers");
      return;
    }
    BOOL ok = GetPrinterW(*handle, 2, (LPBYTE)printer.get(), size, &size);
    if (!ok) {
      std::string err("GetPrinterW failed: ");
      err += getLastErrorCodeAndMessage();
      SetError(err);
      return;
    }
    std::u16string defaultName = getDefaultPrinterNameUtf16();
    std::string err =
        parsePrinterData(printer.get(), handle, defaultName, result_);
    if (!err.empty()) {
      SetError(err);
      return;
    }
    found_ = true;
  }

  void OnOK() override {
    if (!found_) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(printer_model::SerializePrinterModel(Env(), result_));
  }

private:
  std::u16string printerName_;
  bool found_ = false;
  PrinterData result_;
};

class HasPrinterWorker : public PromiseWorker {
public:
  HasPrinterWorker(Napi::Env env, std::u16string printerName)
      : PromiseWorker(env), printerName_(std::move(printerName)) {}

  void Execute() override {
    PrinterHandle handle(
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printerName_.c_str())));
    exists_ = static_cast<bool>(handle);
  }

  void OnOK() override {
    deferred_.Resolve(Napi::Boolean::New(Env(), exists_));
  }

private:
  std::u16string printerName_;
  bool exists_ = false;
};

class GetDefaultPrinterNameWorker : public PromiseWorker {
public:
  explicit GetDefaultPrinterNameWorker(Napi::Env env) : PromiseWorker(env) {}

  void Execute() override { defaultName_ = getDefaultPrinterNameUtf16(); }

  void OnOK() override {
    if (defaultName_.empty()) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(Napi::String::New(Env(), defaultName_));
  }

private:
  std::u16string defaultName_;
};

class GetJobWorker : public PromiseWorker {
public:
  GetJobWorker(Napi::Env env, std::u16string printerName, int jobId)
      : PromiseWorker(env), printerName_(std::move(printerName)),
        jobId_(jobId) {}

  void Execute() override {
    PrinterHandle handle(
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printerName_.c_str())));
    if (!handle) {
      std::string error_str("OpenPrinterW failed: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }
    DWORD size_bytes = 0, dummyBytes = 0;
    GetJobW(*handle, static_cast<DWORD>(jobId_), 2, NULL, size_bytes,
            &size_bytes);
    auto job = mallocValue<JOB_INFO_2W>(size_bytes);
    if (!job) {
      SetError("Failed to allocate memory for printer job");
      return;
    }
    BOOL ok = GetJobW(*handle, static_cast<DWORD>(jobId_), 2, (LPBYTE)job.get(),
                      size_bytes, &dummyBytes);
    if (!ok) {
      found_ = false;
      return;
    }
    found_ = true;
    result_ = parseJobData(job.get());
  }

  void OnOK() override {
    if (!found_) {
      deferred_.Resolve(Env().Null());
      return;
    }
    deferred_.Resolve(printer_model::SerializeJobModel(Env(), result_));
  }

private:
  std::u16string printerName_;
  int jobId_;
  bool found_ = false;
  JobData result_;
};

class CancelJobWorker : public PromiseWorker {
public:
  CancelJobWorker(Napi::Env env, std::u16string printerName, int jobId)
      : PromiseWorker(env), printerName_(std::move(printerName)),
        jobId_(jobId) {}

  void Execute() override {
    PrinterHandle handle(
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printerName_.c_str())));
    if (!handle) {
      std::string error_str("OpenPrinterW failed: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }
    SetJobW(*handle, (DWORD)jobId_, 0, NULL, JOB_CONTROL_DELETE);
  }

  void OnOK() override { deferred_.Resolve(Env().Undefined()); }

private:
  std::u16string printerName_;
  int jobId_;
};

class GetSupportedPrintFormatsWorker : public PromiseWorker {
public:
  explicit GetSupportedPrintFormatsWorker(Napi::Env env) : PromiseWorker(env) {}

  void Execute() override {
    DWORD numBytes = 0, processorsNum = 0;
    LPWSTR nullVal = NULL;
    EnumPrintProcessorsW(nullVal, nullVal, 1, (LPBYTE)(NULL), numBytes,
                         &numBytes, &processorsNum);
    auto processors = mallocValue<_PRINTPROCESSOR_INFO_1W>(numBytes);
    BOOL isOK =
        EnumPrintProcessorsW(nullVal, nullVal, 1, (LPBYTE)(processors.get()),
                             numBytes, &numBytes, &processorsNum);
    if (!isOK) {
      std::string error_str("EnumPrintProcessorsW failed: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }

    _PRINTPROCESSOR_INFO_1W *pProcessor = processors.get();
    for (DWORD processor_i = 0; processor_i < processorsNum;
         ++processor_i, ++pProcessor) {
      numBytes = 0;
      DWORD dataTypesNum = 0;
      EnumPrintProcessorDatatypesW(nullVal, pProcessor->pName, 1,
                                   (LPBYTE)(NULL), numBytes, &numBytes,
                                   &dataTypesNum);
      auto dataTypes = mallocValue<_DATATYPES_INFO_1W>(numBytes);
      isOK = EnumPrintProcessorDatatypesW(nullVal, pProcessor->pName, 1,
                                          (LPBYTE)(dataTypes.get()), numBytes,
                                          &numBytes, &dataTypesNum);
      if (!isOK) {
        std::string error_str("EnumPrintProcessorDatatypesW failed: ");
        error_str += getLastErrorCodeAndMessage();
        SetError(error_str);
        return;
      }
      _DATATYPES_INFO_1W *pDataType = dataTypes.get();
      for (DWORD j = 0; j < dataTypesNum; ++j, ++pDataType) {
        formats_.push_back(wstrToUtf8(pDataType->pName));
      }
    }
  }

  void OnOK() override {
    Napi::Array result = Napi::Array::New(Env(), formats_.size());
    for (size_t i = 0; i < formats_.size(); ++i) {
      result.Set(static_cast<uint32_t>(i),
                 Napi::String::New(Env(), formats_[i]));
    }
    deferred_.Resolve(result);
  }

private:
  std::vector<std::string> formats_;
};

class PrintDirectWorker : public PromiseWorker {
public:
  PrintDirectWorker(Napi::Env env, std::string data, std::u16string printerName,
                    std::u16string docName, std::u16string type)
      : PromiseWorker(env), data_(std::move(data)),
        printerName_(std::move(printerName)), docName_(std::move(docName)),
        type_(std::move(type)) {}

  void Execute() override {
    PrinterHandle handle(
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(printerName_.c_str())));
    if (!handle) {
      std::string error_str("OpenPrinterW failed: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }

    DOC_INFO_1W docInfo;
    docInfo.pDocName =
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(docName_.c_str()));
    docInfo.pOutputFile = NULL;
    docInfo.pDatatype =
        reinterpret_cast<LPWSTR>(const_cast<char16_t *>(type_.c_str()));

    DWORD job = StartDocPrinterW(*handle, 1, (LPBYTE)&docInfo);
    if (job == 0) {
      std::string error_str("StartDocPrinterW error: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }

    BOOL status = StartPagePrinter(*handle);
    if (!status) {
      EndDocPrinter(*handle);
      std::string error_str("StartPagePrinter error: ");
      error_str += getLastErrorCodeAndMessage();
      SetError(error_str);
      return;
    }

    DWORD bytesWritten = 0;
    status = WritePrinter(*handle, (LPVOID)(data_.c_str()), (DWORD)data_.size(),
                          &bytesWritten);
    EndPagePrinter(*handle);
    EndDocPrinter(*handle);
    if (!status || bytesWritten != data_.size()) {
      SetError("Failed to send all bytes to printer");
      return;
    }
    jobId_ = job;
  }

  void OnOK() override { deferred_.Resolve(Napi::Number::New(Env(), jobId_)); }

private:
  std::string data_;
  std::u16string printerName_;
  std::u16string docName_;
  std::u16string type_;
  DWORD jobId_ = 0;
};

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
  auto *worker = new GetPrinterDetailsWorker(
      env, iArgs[0].As<Napi::String>().Utf16Value());
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
      new HasPrinterWorker(env, iArgs[0].As<Napi::String>().Utf16Value());
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
  std::u16string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf16Value();
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
  std::u16string printername;
  if (!iArgs[0].IsString()) {
    Napi::Error::New(env, "Printer must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  printername = iArgs[0].As<Napi::String>().Utf16Value();
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
  auto *worker = new GetSupportedPrintFormatsWorker(iArgs.Env());
  worker->Queue();
  return worker->GetPromise();
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

  auto *worker =
      new PrintDirectWorker(env, std::move(data), std::move(printername),
                            std::move(docname), std::move(type));
  worker->Queue();
  return worker->GetPromise();
}

Napi::Value PrintFile(const Napi::CallbackInfo &iArgs) {
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(iArgs.Env());
  deferred.Reject(
      Napi::Error::New(iArgs.Env(), "Not yet implemented on Windows").Value());
  return deferred.Promise();
}
