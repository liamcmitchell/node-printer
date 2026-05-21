// Windows does not support PDF formats, but you can use imagemagick-native to achieve conversion from PDF to EMF.

import { readFileSync } from "node:fs";
import * as printer from "../lib/index.js";

let imagemagick; // will be loaded later with proper error.
var filename = process.argv[2],
  printername = process.argv[3];

if (process.platform !== "win32") {
  throw "This application can be run only on win32 as a demo of print PDF image";
}

if (!filename) {
  throw "PDF file name is missing. Please use the following params: <filename> [printername]";
}

try {
  const imagemagickModule = await import("imagemagick-native");
  imagemagick = imagemagickModule.default || imagemagickModule;
} catch {
  throw "please install imagemagick-native: `npm install imagemagick-native`";
}

var data = readFileSync(filename);

console.log("data: " + data.toString().substr(0, 20));

//console.log(imagemagick.identify({srcData: data}));

// First convert PDF into
imagemagick.convert(
  {
    srcData: data,
    srcFormat: "PDF",
    format: "EMF",
  },
  function (err, buffer) {
    if (err) {
      throw "something went wrong on converting to EMF: " + err;
    }

    // Now we have EMF file, send it to printer as EMF format
    printer.printDirect({
      data: buffer,
      printer: printername,
      type: "EMF",
      success: function (id) {
        console.log("printed with id " + id);
      },
      error: function (err) {
        console.error("error on printing: " + err);
      },
    });
  },
);
