import { getAllPrinterDetails, print } from "../lib/index.js";

const printer =
  process.argv[2] ||
  process.env.PRINTER_NAME ||
  (await getAllPrinterDetails()).find((p) => p.isDefault)?.name;

const jobId = await print({
  printer,
  data: "Hello from Node.js\n",
  format: "RAW",
});

console.log("sent to printer, job id:", jobId);
