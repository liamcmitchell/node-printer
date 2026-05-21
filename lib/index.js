import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const printerHelper = require("./node_printer.node");

export const getSupportedPrintFormats = printerHelper.getSupportedPrintFormats;
export const getSupportedJobCommands = printerHelper.getSupportedJobCommands;

export function getDefaultPrinterName() {
  const printerName = printerHelper.getDefaultPrinterName();
  if (printerName) {
    return printerName;
  }

  const printers = getPrinters();
  if (!printers?.length) {
    return undefined;
  }

  for (const printer of printers) {
    if (printer.isDefault === true) {
      return printer.name;
    }
  }

  return undefined;
}

export function getPrinter(printerName) {
  const selectedPrinter = printerName || getDefaultPrinterName();
  const printer = printerHelper.getPrinter(selectedPrinter);
  correctPrinterInfo(printer);
  return printer;
}

export function getPrinterDriverOptions(printerName) {
  const selectedPrinter = printerName || getDefaultPrinterName();
  return printerHelper.getPrinterDriverOptions(selectedPrinter);
}

export function getSelectedPaperSize(printerName) {
  const driverOptions = getPrinterDriverOptions(printerName);
  let selectedSize = "";
  if (driverOptions?.PageSize) {
    for (const key of Object.keys(driverOptions.PageSize)) {
      if (driverOptions.PageSize[key]) {
        selectedSize = key;
      }
    }
  }
  return selectedSize;
}

export function getJob(printerName, jobId) {
  return printerHelper.getJob(printerName, jobId);
}

export function setJob(printerName, jobId, command) {
  return printerHelper.setJob(printerName, jobId, command);
}

export function getPrinters() {
  const printers = printerHelper.getPrinters();
  if (printers?.length) {
    for (const printer of printers) {
      correctPrinterInfo(printer);
    }
  }
  return printers;
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

export function printDirect(parameters) {
  let data = parameters;
  let printer;
  let docname;
  let type;
  let options;
  let success;
  let error;

  if (arguments.length === 1) {
    data = parameters.data;
    printer = parameters.printer;
    docname = parameters.docname;
    type = parameters.type;
    options = parameters.options || {};
    success = parameters.success;
    error = parameters.error;
  } else {
    printer = arguments[1];
    type = arguments[2];
    docname = arguments[3];
    options = arguments[4];
    success = arguments[5];
    error = arguments[6];
  }

  if (!type) {
    type = "RAW";
  }

  if (!printer) {
    printer = getDefaultPrinterName();
  }

  type = type.toUpperCase();

  if (!docname) {
    docname = "node print job";
  }

  if (!options) {
    options = {};
  }

  if (!success) {
    success = function () {};
  }

  if (!error) {
    error = function (err) {
      throw err;
    };
  }

  if (printerHelper.printDirect) {
    try {
      const res = printerHelper.printDirect(data, printer, docname, type, options);
      if (res) {
        success(res);
      } else {
        error(new Error("Something wrong in printDirect"));
      }
    } catch (e) {
      error(e);
    }
  } else {
    error(new Error("Not supported"));
  }
}

export function printFile(parameters) {
  let filename;
  let docname;
  let printer;
  let options;
  let success;
  let error;

  if (arguments.length !== 1 || typeof parameters !== "object") {
    throw new Error("must provide arguments object");
  }

  filename = parameters.filename;
  docname = parameters.docname;
  printer = parameters.printer;
  options = parameters.options || {};
  success = parameters.success;
  error = parameters.error;

  if (!success) {
    success = function () {};
  }

  if (!error) {
    error = function (err) {
      throw err;
    };
  }

  if (!filename) {
    return error(new Error("must provide at least a filename"));
  }

  if (!printer) {
    printer = getDefaultPrinterName();
  }

  if (!printer) {
    return error(new Error("Printer parameter of default printer is not defined"));
  }

  if (!docname) {
    docname = filename;
  }

  if (printerHelper.printFile) {
    try {
      const res = printerHelper.printFile(filename, docname, printer, options);

      if (!Number.isNaN(Number.parseInt(res, 10))) {
        success(res);
      } else {
        error(new Error(res));
      }
    } catch (e) {
      error(e);
    }
  } else {
    error(new Error("Not supported"));
  }
}
