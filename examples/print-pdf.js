import { readFileSync } from "node:fs";
import { getPrinters, print } from "../lib/index.js";

const filename = process.argv[2] || new URL("test.pdf", import.meta.url).pathname;
const printer =
  process.argv[3] || process.env.PRINTER_NAME || getPrinters().find((p) => p.isDefault)?.name;

const jobId = print({
  printer,
  data: readFileSync(filename),
  format: "PDF",
});

console.log("sent to printer, job id:", jobId);
