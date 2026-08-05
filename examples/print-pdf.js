import { readFileSync } from "node:fs";
import { getAllPrinterDetails, print } from "../lib/index.js";

const filename = process.argv[2] || new URL("test.pdf", import.meta.url).pathname;
const printer =
  process.argv[3] ||
  process.env.PRINTER_NAME ||
  (await getAllPrinterDetails()).find((p) => p.isDefault)?.name;

const jobId = await print({
  printer,
  data: readFileSync(filename),
  format: "PDF",
});

console.log("sent to printer, job id:", jobId);
