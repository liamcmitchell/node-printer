import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const native = require("./node_printer.node");

export function getPrinters() {
  const printers = native.getPrinters();
  if (printers?.length) {
    for (const printer of printers) {
      correctPrinterInfo(printer);
    }
  }
  return printers;
}

export function getPrinter(printerName) {
  const selectedPrinter = printerName || getDefaultPrinterName();
  const printer = native.getPrinter(selectedPrinter);
  correctPrinterInfo(printer);
  return printer;
}

export function getDefaultPrinterName() {
  const printers = getPrinters();
  if (!printers?.length) {
    return undefined;
  }
  const defaultPrinter = printers.find((p) => p.isDefault === true);
  return defaultPrinter?.name;
}

export function printDirect({
  data,
  printer,
  type = "RAW",
  docname = "node print job",
  options = {},
}) {
  if (!data) {
    throw new Error("data is required");
  }
  if (!printer) {
    printer = getDefaultPrinterName();
  }
  if (!printer) {
    throw new Error("printer is required (no default printer found)");
  }
  return native.printDirect(data, printer, docname, type.toUpperCase(), options);
}

export function printFile({ filename, printer, docname, options = {} }) {
  if (!filename) {
    throw new Error("filename is required");
  }
  if (!printer) {
    printer = getDefaultPrinterName();
  }
  if (!printer) {
    throw new Error("printer is required (no default printer found)");
  }
  if (!docname) {
    docname = filename;
  }
  return native.printFile(filename, docname, printer, options);
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
