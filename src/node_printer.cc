#if _MSC_VER
#pragma warning(disable : 4018)
#endif

#include "node_printer.hpp"

#include <node_buffer.h>

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getAllPrinterDetails",
              Napi::Function::New(env, getAllPrinterDetails));
  exports.Set("getPrinterDetails", Napi::Function::New(env, getPrinterDetails));
  exports.Set("hasPrinter", Napi::Function::New(env, hasPrinter));
  exports.Set("getDefaultPrinterName",
              Napi::Function::New(env, getDefaultPrinterName));
  exports.Set("getJob", Napi::Function::New(env, getJob));
  exports.Set("cancelJob", Napi::Function::New(env, cancelJob));
  exports.Set("printDirect", Napi::Function::New(env, PrintDirect));
  exports.Set("printFile", Napi::Function::New(env, PrintFile));
  exports.Set("getSupportedPrintFormats",
              Napi::Function::New(env, getSupportedPrintFormats));
  return exports;
}

NODE_API_MODULE(node_printer, Init)
