// Windows does not support PDF formats, but you can use imagemagick-native to achieve conversion from PDF to EMF.

import { readFile } from "node:fs";
import path from "node:path";
import * as printer from "../lib/index.js";

var filename = process.argv[2],
  printername = process.argv[3];

if (process.platform == "win32") {
  throw "Not yet supported for win32";
}

if (!filename || filename == "-h") {
  throw "PDF file name is missing. Please use the following params: <filename> [printername]";
}

filename = path.resolve(process.cwd(), filename);
console.log("printing file name " + filename);

readFile(filename, function (err, data) {
  if (err) {
    console.error("err:" + err);
    return;
  }
  console.log("data type is: " + typeof data + ", is buffer: " + Buffer.isBuffer(data));
  printer.printDirect({
    data: data,
    printer: printername,
    type: "PDF",
    success: function (id) {
      console.log("printed with id " + id);
    },
    error: function (err) {
      console.error("error on printing: " + err);
    },
  });
});
