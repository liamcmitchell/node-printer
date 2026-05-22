import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const native = require("./node_printer.node");

export function getPrinters() {
  return native.getPrinters();
}

export function getPrinter(printer) {
  if (!printer) {
    throw new Error("printer name is required");
  }
  return native.getPrinter(printer);
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

export function setJob(printerName, jobId, command) {
  return native.setJob(printerName, jobId, command);
}

export function getSupportedPrintFormats() {
  return native.getSupportedPrintFormats();
}

export function getSupportedJobCommands() {
  return native.getSupportedJobCommands();
}
