import * as printer from "../lib/index.js";
import { inspect } from "node:util";

var printers = printer.getPrinters();

printers.forEach(function (iPrinter, i) {
  console.log(
    "" +
      i +
      'ppd for printer "' +
      iPrinter.name +
      '":' +
      inspect(printer.getPrinterDriverOptions(iPrinter.name), { colors: true, depth: 10 }),
  );
  console.log("\tselected page size:" + printer.getSelectedPaperSize(iPrinter.name) + "\n");
});
