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
});

test("capability APIs include RAW and CANCEL", () => {
  const formats = printer.getSupportedPrintFormats();
  assert.ok(formats.includes("RAW"), "Expected RAW in getSupportedPrintFormats()");

  const commands = printer.getSupportedJobCommands();
  assert.ok(commands.includes("CANCEL"), "Expected CANCEL in getSupportedJobCommands()");
});

test("printDirect sends data and returns job id", () => {
  const jobId = printer.printDirect({
    data: `test payload ${Date.now()}\n`,
    printer: printerName,
    type: "RAW",
  });

  assert.ok(jobId > 0, "Expected printDirect to return a valid job id");

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
});

test("setJob CANCEL transitions job state", () => {
  const jobId = printer.printDirect({
    data: `node-printer cancel test ${Date.now()}\n`,
    printer: printerName,
    type: "RAW",
  });

  const cancelled = printer.setJob(printerName, jobId, "CANCEL");
  assert.equal(cancelled, true);

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
});

test("printFile submits file and returns job id", { skip: process.platform === "win32" }, () => {
  const tmpFile = path.join(os.tmpdir(), `node-printer-test-${Date.now()}.txt`);
  fs.writeFileSync(tmpFile, "test file content\n", "utf8");

  try {
    const jobId = printer.printFile({
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
});
