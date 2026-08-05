import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const native = require("./node_printer.node");

export function getAllPrinterDetails() {
  return native.getAllPrinterDetails();
}

export function getPrinterDetails(printer) {
  if (!printer) {
    throw new Error("printer name is required");
  }
  return native.getPrinterDetails(printer);
}

export function hasPrinter(printer) {
  if (!printer) {
    throw new Error("printer name is required");
  }
  return native.hasPrinter(printer);
}

export function getDefaultPrinterName() {
  return native.getDefaultPrinterName();
}

export function print({ data, filename, printer, format = "RAW", docname, options = {} }) {
  if (!printer) {
    throw new Error("printer is required");
  }
  if (data && filename) {
    throw new Error("provide either data or filename, not both");
  }
  if (!data && !filename) {
    throw new Error("data or filename is required");
  }
  if (filename) {
    return native.printFile(filename, docname || filename, printer, options);
  }
  return native.printDirect(
    data,
    printer,
    docname || "node print job",
    format.toUpperCase(),
    options,
  );
}

export function getJob(printerName, jobId) {
  return native.getJob(printerName, jobId);
}

export function cancelJob(printerName, jobId) {
  return native.cancelJob(printerName, jobId);
}

export function getSupportedPrintFormats() {
  return native.getSupportedPrintFormats();
}
