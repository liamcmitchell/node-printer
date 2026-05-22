import { getPrinters, print, getJob, setJob } from "../lib/index.js";

const printer =
  process.argv[2] || process.env.PRINTER_NAME || getPrinters().find((p) => p.isDefault)?.name;

const jobId = print({ printer, data: "test print for cancellation\n" });
console.log("submitted job:", jobId);
console.log("job info:", getJob(printer, jobId));

const ok = setJob(printer, jobId, "CANCEL");
console.log("cancelled:", ok);

try {
  console.log("job after cancel:", getJob(printer, jobId));
} catch (err) {
  console.log("job deleted:", err.message);
}
