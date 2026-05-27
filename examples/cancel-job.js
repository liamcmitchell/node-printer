import { getPrinters, print, getJob, cancelJob } from "../lib/index.js";

const printer =
  process.argv[2] || process.env.PRINTER_NAME || getPrinters().find((p) => p.isDefault)?.name;

const jobId = print({ printer, data: "test print for cancellation\n" });
console.log("submitted job:", jobId);
console.log("job info:", getJob(printer, jobId));

cancelJob(printer, jobId);
console.log("cancelled job", jobId);
