import { print, getAllPrinterDetails } from "../lib/index.js";

const filename = process.argv[2];
const printer =
  process.argv[3] ||
  process.env.PRINTER_NAME ||
  (await getAllPrinterDetails()).find((p) => p.isDefault)?.name;

if (!filename || !printer) {
  console.error("Usage: node print-file.js <filename> <printer-name>");
  process.exit(1);
}

// Not supported on windows.
const jobId = await print({ printer, filename });

console.log("sent to printer, job id:", jobId);
