import { getPrinters, print } from "../lib/index.js";

const printer =
  process.argv[2] || process.env.PRINTER_NAME || getPrinters().find((p) => p.isDefault)?.name;

const jobId = print({
  printer,
  data: "Hello from Node.js\n",
  format: "RAW",
});

console.log("sent to printer, job id:", jobId);
