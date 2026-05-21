#ifndef NODE_PRINTER_HPP
#define NODE_PRINTER_HPP

#include <napi.h>

#include <string>

/**
 * Send data to printer
 *
 * @param data String/NativeBuffer, mandatory, raw data bytes
 * @param printername String, mandatory, specifying printer name
 * @param docname String, mandatory, specifying document name
 * @param type String, mandatory, specifying data type. E.G.: RAW, TEXT, ...
 *
 * @returns true for success, false for failure.
 */
Napi::Value PrintDirect(const Napi::CallbackInfo &iArgs);

/**
 * Send file to printer
 *
 * @param filename String, mandatory, specifying filename to print
 * @param docname String, mandatory, specifying document name
 * @param printer String, mandatory, specifying printer name
 *
 * @returns jobId for success, or error message for failure.
 */
Napi::Value PrintFile(const Napi::CallbackInfo &iArgs);

/** Retrieve all printers and jobs
 * posix: minimum version: CUPS 1.1.21/OS X 10.4
 */
Napi::Value getPrinters(const Napi::CallbackInfo &iArgs);

/**
 * Return default printer name, if null then default printer is not set
 */
Napi::Value getDefaultPrinterName(const Napi::CallbackInfo &iArgs);

/** Retrieve printer info and jobs
 * @param printer name String
 */
Napi::Value getPrinter(const Napi::CallbackInfo &iArgs);

/** Retrieve printer driver info
 * @param printer name String
 */
Napi::Value getPrinterDriverOptions(const Napi::CallbackInfo &iArgs);

/** Retrieve job info
 *  @param printer name String
 *  @param job id Number
 */
Napi::Value getJob(const Napi::CallbackInfo &iArgs);

// TODO
/** Set job command.
 * arguments:
 * @param printer name String
 * @param job id Number
 * @param job command String
 * Possible commands:
 *      "CANCEL"
 *      "PAUSE"
 *      "RESTART"
 *      "RESUME"
 *      "DELETE"
 *      "SENT-TO-PRINTER"
 *      "LAST-PAGE-EJECTED"
 *      "RETAIN"
 *      "RELEASE"
 */
Napi::Value setJob(const Napi::CallbackInfo &iArgs);

/** Get supported print formats for printDirect. It depends on platform
 */
Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs);

/** Get supported job commands for setJob method
 */
Napi::Value getSupportedJobCommands(const Napi::CallbackInfo &iArgs);

// TODO:
//  optional ability to get printer spool

// util class

/** Memory value class management to avoid memory leak
 * TODO: move to std::unique_ptr on switching to C++11
 */
template <typename Type> class MemValueBase {
public:
  MemValueBase() : _value(NULL) {}

  /** Destructor. The allocated memory will be deallocated
   */
  virtual ~MemValueBase() {}

  Type *get() { return _value; }
  Type *operator->() { return &_value; }
  operator bool() const { return (_value != NULL); }

protected:
  Type *_value;

  virtual void free() {};
};

#endif
