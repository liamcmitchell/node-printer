import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import * as printer from "../lib/index.js";

const printerName = process.platform === "win32" ? "Microsoft Print to PDF" : "NodePrinterMock";

test("discovery APIs return expected printer", async () => {
  const printers = await printer.getAllPrinterDetails();
  const details = printers.find((entry) => entry.name === printerName);
  if (!details) console.log("Available printers:", printers);
  assert.ok(
    details,
    `Printer '${printerName}' was not found in getAllPrinterDetails(). Register the OS printer once before running tests.`,
  );

  const hasExpectedPrinter = await printer.hasPrinter(printerName);
  assert.equal(hasExpectedPrinter, true);

  const single = await printer.getPrinterDetails(printerName);
  assert.equal(single.name, printerName);
  assert.ok(
    ["idle", "processing", "stopped"].includes(single.state),
    `Unexpected state: ${single.state}`,
  );
  assert.ok(Array.isArray(single.stateReasons), "stateReasons should be an array");
  assert.ok(typeof single.raw === "object", "raw should be an object");

  const defaultPrinterName = await printer.getDefaultPrinterName();
  if (defaultPrinterName !== null) {
    assert.equal(typeof defaultPrinterName, "string");
  }
});

test("capability APIs include RAW", async () => {
  const formats = await printer.getSupportedPrintFormats();
  assert.ok(formats.includes("RAW"), "Expected RAW in getSupportedPrintFormats()");
});

test("print sends data and returns job id", async () => {
  const jobId = await printer.print({
    data: `test payload ${Date.now()}\n`,
    printer: printerName,
    format: "RAW",
  });

  assert.ok(jobId > 0, "Expected print to return a valid job id");

  const job = await printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
  assert.equal(typeof job.name, "string");
  assert.equal(typeof job.state, "string");
  assert.equal(typeof job.createdAt, "number");
  assert.ok(typeof job.raw === "object", "job.raw should be an object");
});

test("cancelJob removes the job", async () => {
  const jobId = await printer.print({
    data: `node-printer cancel test ${Date.now()}\n`,
    printer: printerName,
    format: "RAW",
  });

  await printer.cancelJob(printerName, jobId);

  // Cancelling an already-gone job should be a no-op
  await printer.cancelJob(printerName, jobId);
});

test(
  "print with filename submits file and returns job id",
  { skip: process.platform === "win32" },
  async () => {
    const tmpFile = path.join(os.tmpdir(), `node-printer-test-${Date.now()}.txt`);
    fs.writeFileSync(tmpFile, "test file content\n", "utf8");

    try {
      const jobId = await printer.print({
        filename: tmpFile,
        printer: printerName,
        docname: "node-printer-file-test",
      });
      assert.ok(jobId > 0, "Expected printFile to return a valid job id");

      const job = await printer.getJob(printerName, jobId);
      assert.equal(job.id, jobId);
    } finally {
      fs.unlinkSync(tmpFile);
    }
  },
);
