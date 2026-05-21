import * as printer from "../lib/index.js";

const printerName = process.argv[2] || process.env.PRINTER_NAME;

printer.printDirect({
  data: "print from Node.JS buffer", // or simple String: "some text"
  ...(printerName ? { printer: printerName } : {}),
  type: "RAW", // type: RAW, TEXT, PDF, JPEG, .. depends on platform
  success: function (jobID) {
    console.log("sent to printer with ID: " + jobID);
  },
  error: function (err) {
    console.log(err);
  },
});
