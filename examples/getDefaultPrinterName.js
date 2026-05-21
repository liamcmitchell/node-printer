import * as printer from "../lib/index.js";

console.log(
  "default printer name: " + (printer.getDefaultPrinterName() || "is not defined on your computer"),
);
