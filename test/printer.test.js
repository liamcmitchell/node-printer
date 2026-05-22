import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test, { after, before, beforeEach } from "node:test";

import * as printer from "../lib/index.js";
import { createMockIppPrinter } from "./printer.mock.js";

const printerName = process.env.MOCK_PRINTER_NAME || "NodePrinterMock";
const mock = createMockIppPrinter({
  name: printerName,
  host: process.env.MOCK_IPP_HOST || "127.0.0.1",
  port: Number(process.env.MOCK_IPP_PORT || "8631"),
});

function printDirectAsync(options) {
  return new Promise((resolve, reject) => {
    printer.printDirect({
      ...options,
      success: (jobId) => resolve(jobId),
      error: (error) => reject(error),
    });
  });
}

function printFileAsync(options) {
  return new Promise((resolve, reject) => {
    printer.printFile({
      ...options,
      success: (jobId) => resolve(jobId),
      error: (error) => reject(error),
    });
  });
}

before(async () => {
  try {
    await mock.start();
  } catch (error) {
    if (error && error.code === "EADDRINUSE") {
      throw new Error(
        `Unable to start mock IPP server on ${mock.uri}: address already in use. Stop any existing mock server and retry.`,
      );
    }
    throw error;
  }
});

beforeEach(() => {
  mock.reset();
  mock.setCompletionTimeout(20);
});

after(async () => {
  await mock.stop();
});

test("mock IPP server is running", () => {
  assert.equal(mock.isRunning(), true);
});

test("discovery APIs return expected printer", () => {
  const printers = printer.getPrinters();
  const details = printers.find((entry) => entry.name === printerName);
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

test("printDirect sends payload to mock and getJob can read it", async () => {
  const payload = `node-printer mock direct ${Date.now()} ${Math.random()}\n`;
  const jobId = await printDirectAsync({
    data: payload,
    printer: printerName,
    type: "RAW",
  });

  const mockJob = await mock.waitForJob(
    (entry) => typeof entry.dataUtf8 === "string" && entry.dataUtf8.includes(payload),
    { timeoutMs: 5000 },
  );

  assert.ok(mockJob.bytes > 0, "Expected mock job bytes to be greater than 0");

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
});

test("setJob CANCEL transitions job state", async () => {
  mock.setCompletionTimeout(0);

  const payload = `node-printer mock cancel ${Date.now()} ${Math.random()}\n`;
  const jobId = await printDirectAsync({
    data: payload,
    printer: printerName,
    type: "RAW",
  });

  const mockJob = await mock.waitForJob(
    (entry) => typeof entry.dataUtf8 === "string" && entry.dataUtf8.includes(payload),
    { timeoutMs: 5000 },
  );
  assert.ok(mockJob.id > 0);

  const cancelled = printer.setJob(printerName, jobId, "CANCEL");
  assert.equal(cancelled, true);

  const job = printer.getJob(printerName, jobId);
  assert.equal(job.id, jobId);
});

test("printFile submits file content", { skip: process.platform === "win32" }, async () => {
  const payload = `node-printer mock file ${Date.now()} ${Math.random()}\n`;
  const tmpFile = path.join(os.tmpdir(), `node-printer-test-${Date.now()}.txt`);
  fs.writeFileSync(tmpFile, payload, "utf8");

  try {
    await printFileAsync({
      filename: tmpFile,
      printer: printerName,
      docname: "node-printer-file-test",
    });

    const fileJob = await mock.waitForJob(
      (entry) => typeof entry.dataUtf8 === "string" && entry.dataUtf8.includes(payload),
      { timeoutMs: 5000 },
    );

    assert.ok(fileJob.bytes > 0, "Expected printFile payload bytes in mock job");
  } finally {
    fs.unlinkSync(tmpFile);
  }
});
