import { getAllPrinterDetails, print } from "../lib/index.js";

const printer =
  process.argv[2] ||
  process.env.PRINTER_NAME ||
  (await getAllPrinterDetails()).find((p) => p.isDefault)?.name;

const template = 'N\nS4\nD15\nq400\nR\nB20,10,0,1,2,30,173,B,"{{BARCODE}}"\nP0\n';

const jobId = await print({
  printer,
  data: template.replace("{{BARCODE}}", "123456789"),
  format: "RAW",
});

console.log("sent to printer, job id:", jobId);
