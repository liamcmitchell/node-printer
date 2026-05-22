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

/** Retrieve all printers and jobs
 * posix: minimum version: CUPS 1.1.21/OS X 10.4
 */
Napi::Value getPrinters(const Napi::CallbackInfo &iArgs);

/** Retrieve printer info and jobs
 * @param printer name String
 */
Napi::Value getPrinter(const Napi::CallbackInfo &iArgs);

/** Retrieve job info
 *  @param printer name String
 *  @param job id Number
 */
Napi::Value getJob(const Napi::CallbackInfo &iArgs);

/** Set job command.
 * @param printer name String
 * @param job id Number
 * @param job command String
 */
Napi::Value setJob(const Napi::CallbackInfo &iArgs);

/** Get supported print formats for printDirect. It depends on platform
 */
Napi::Value getSupportedPrintFormats(const Napi::CallbackInfo &iArgs);

/** Get supported job commands for setJob method
 */
Napi::Value getSupportedJobCommands(const Napi::CallbackInfo &iArgs);

#endif
