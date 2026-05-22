#if _MSC_VER
#pragma warning(disable : 4018)
#endif

#include "node_printer.hpp"

#include <node_buffer.h>

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getPrinters", Napi::Function::New(env, getPrinters));
  exports.Set("getPrinter", Napi::Function::New(env, getPrinter));
  exports.Set("getJob", Napi::Function::New(env, getJob));
  exports.Set("setJob", Napi::Function::New(env, setJob));
  exports.Set("printDirect", Napi::Function::New(env, PrintDirect));
  exports.Set("printFile", Napi::Function::New(env, PrintFile));
  exports.Set("getSupportedPrintFormats",
              Napi::Function::New(env, getSupportedPrintFormats));
  exports.Set("getSupportedJobCommands",
              Napi::Function::New(env, getSupportedJobCommands));
  return exports;
}

NODE_API_MODULE(node_printer, Init)
