import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const native = require("./node_printer.node");

export function getPrinters() {
  const printers = native.getPrinters();
  printers.forEach(correctPrinterInfo);
  return printers;
}

export function getPrinter(printer) {
  if (!printer) {
    throw new Error("printer name is required");
  }
  const printerInfo = native.getPrinter(printer);
  correctPrinterInfo(printerInfo);
  return printerInfo;
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

function correctPrinterInfo(printer) {
  if (printer.status || !printer.options || !printer.options["printer-state"]) {
    return;
  }

  let status = printer.options["printer-state"];
  if (status === "3") {
    status = "IDLE";
  } else if (status === "4") {
    status = "PRINTING";
  } else if (status === "5") {
    status = "STOPPED";
  }

  for (const key in printer.options) {
    if (key.endsWith("time") && printer.options[key] && !(printer.options[key] instanceof Date)) {
      printer.options[key] = new Date(printer.options[key] * 1000);
    }
  }

  printer.status = status;
}
