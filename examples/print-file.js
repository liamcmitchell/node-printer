import { print, getPrinters } from "../lib/index.js";

const filename = process.argv[2];
const printer =
  process.argv[3] || process.env.PRINTER_NAME || getPrinters().find((p) => p.isDefault)?.name;

if (!filename || !printer) {
  console.error("Usage: node print-file.js <filename> <printer-name>");
  process.exit(1);
}

// Not supported on windows.
const jobId = print({ printer, filename });

console.log("sent to printer, job id:", jobId);
