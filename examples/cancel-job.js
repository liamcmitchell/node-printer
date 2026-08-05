import { getAllPrinterDetails, print, getJob, cancelJob } from "../lib/index.js";

const printer =
  process.argv[2] ||
  process.env.PRINTER_NAME ||
  (await getAllPrinterDetails()).find((p) => p.isDefault)?.name;

const jobId = await print({ printer, data: "test print for cancellation\n" });
console.log("submitted job:", jobId);
console.log("job info:", await getJob(printer, jobId));

await cancelJob(printer, jobId);
console.log("cancelled job", jobId);
