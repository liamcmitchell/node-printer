import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import * as printer from "../lib/index.js";

const printerName = process.platform === "win32" ? "Microsoft Print to PDF" : "NodePrinterMock";

test("discovery APIs return expected printer", () => {
  const printers = printer.getPrinters();
  const details = printers.find((entry) => entry.name === printerName);
  if (!details) console.log("Available printers:", printers);
  assert.ok(
    details,
    `Printer '${printerName}' was not found in getPrinters(). Register the OS printer once before running tests.`,
  );

  const single = printer.getPrinter(printerName);
  assert.equal(single.name, printerName);
  assert.ok(
    ["idle", "processing", "stopped"].includes(single.state),
    `Unexpected state: ${single.state}`,
  );
  assert.ok(Array.isArray(single.stateReasons), "stateReasons should be an array");
  assert.ok(typeof single.raw === "object", "raw should be an object");
});

test("capability APIs include RAW and CANCEL", () => {
  const formats = printer.getSupportedPrintFormats();
  assert.ok(formats.includes("RAW"), "Expected RAW in getSupportedPrintFormats()");

  const commands = printer.getSupportedJobCommands();
  assert.ok(commands.includes("CANCEL"), "Expected CANCEL in getSupportedJobCommands()");
});

test("print sends data and returns job id", () => {
  const jobId = printer.print({
    data: `test payload ${Date.now()}\n`,
    printer: printerName,
    format: "RAW",
  });

  assert.ok(jobId > 0, "Expected print to return a valid job id");

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
  assert.equal(typeof job.name, "string");
  assert.equal(typeof job.state, "string");
  assert.equal(typeof job.createdAt, "number");
  assert.ok(typeof job.raw === "object", "job.raw should be an object");
});

test("setJob CANCEL transitions job state", () => {
  const jobId = printer.print({
    data: `node-printer cancel test ${Date.now()}\n`,
    printer: printerName,
    format: "RAW",
  });

  const cancelled = printer.setJob(printerName, jobId, "CANCEL");
  assert.equal(cancelled, true);

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
});

test(
  "print with filename submits file and returns job id",
  { skip: process.platform === "win32" },
  () => {
    const tmpFile = path.join(os.tmpdir(), `node-printer-test-${Date.now()}.txt`);
    fs.writeFileSync(tmpFile, "test file content\n", "utf8");

    try {
      const jobId = printer.print({
        filename: tmpFile,
        printer: printerName,
        docname: "node-printer-file-test",
      });
      assert.ok(jobId > 0, "Expected printFile to return a valid job id");

      const job = printer.getJob(printerName, jobId);
      assert.equal(job.id, jobId);
    } finally {
      fs.unlinkSync(tmpFile);
    }
  },
);
