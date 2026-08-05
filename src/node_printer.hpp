#ifndef NODE_PRINTER_HPP
#define NODE_PRINTER_HPP

#include <napi.h>

#include <string>

/** Base class for async workers that resolve/reject a JS Promise.
 * Subclasses just need to implement Execute() and OnOK() (calling
 * deferred_.Resolve(...) with the result).
 */
class PromiseWorker : public Napi::AsyncWorker {
public:
  explicit PromiseWorker(Napi::Env env)
      : Napi::AsyncWorker(env), deferred_(Napi::Promise::Deferred::New(env)) {}

  Napi::Promise GetPromise() { return deferred_.Promise(); }

protected:
  void OnError(const Napi::Error &error) override {
    deferred_.Reject(error.Value());
  }

  Napi::Promise::Deferred deferred_;
};

/**
 * Send data to printer
 *
 * @param data String/NativeBuffer, mandatory, raw data bytes
 * @param printername String, mandatory, specifying printer name
 * @param docname String, mandatory, specifying document name
 * @param type String, mandatory, specifying data type. E.G.: RAW, TEXT, ...
 *
 * @returns job id number on success, throws on failure.
 */
Napi::Value PrintDirect(const Napi::CallbackInfo &iArgs);

/**
 * Send file to printer
 *
 * @param filename String, mandatory, specifying filename to print
 * @param docname String, mandatory, specifying document name
 * @param printer String, mandatory, specifying printer name
 *
 * @returns job id number on success, throws on failure.
 */
Napi::Value PrintFile(const Napi::CallbackInfo &iArgs);

/** Async retrieve all printers and jobs */
Napi::Value getAllPrinterDetails(const Napi::CallbackInfo &iArgs);

/** Async retrieve printer info and jobs
 * @param printer name String
 */
Napi::Value getPrinterDetails(const Napi::CallbackInfo &iArgs);

/** Check if a printer exists
 * @param printer name String
 */
Napi::Value hasPrinter(const Napi::CallbackInfo &iArgs);

/** Retrieve default printer name, or null if no default is configured */
Napi::Value getDefaultPrinterName(const Napi::CallbackInfo &iArgs);

/** Retrieve job info
 * @param printer name String
 * @param job id Number
 */
Napi::Value getJob(const Napi::CallbackInfo &iArgs);

/** Cancel a print job. Silently ignores jobs that no longer exist.
 * @param printer name String
 * @param job id Number
 */
Napi::Value cancelJob(const Napi::CallbackInfo &iArgs);

/** Get supported print formats for printDirect. It depends on platform
 */
Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs);

#endif
