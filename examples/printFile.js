// use: node printFile.js [filePath printerName]
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import * as printer from "../lib/index.js";

var filename = process.argv[2] || fileURLToPath(import.meta.url);

console.log("platform:", process.platform);
console.log("try to print file: " + filename);

if (process.platform != "win32") {
  printer.printFile({
    filename: filename,
    printer: process.argv[3], // printer name, if missing then will print to default printer
    success: function (jobID) {
      console.log("sent to printer with ID: " + jobID);
    },
    error: function (err) {
      console.log(err);
    },
  });
} else {
  // not yet implemented, use printDirect and text
  printer.printDirect({
    data: readFileSync(filename),
    printer: process.argv[3], // printer name, if missing then will print to default printer
    success: function (jobID) {
      console.log("sent to printer with ID: " + jobID);
    },
    error: function (err) {
      console.log(err);
    },
  });
}
