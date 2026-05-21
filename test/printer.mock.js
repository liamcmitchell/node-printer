import { EventEmitter } from "node:events";
import http from "node:http";

import ipp from "ipp";

const DEFAULT_FORMATS = ["application/vnd.cups-raw", "application/octet-stream", "text/plain"];
const DEFAULT_OPERATIONS = [
  "Print-Job",
  "Validate-Job",
  "Cancel-Job",
  "Get-Job-Attributes",
  "Get-Jobs",
  "Get-Printer-Attributes",
];

function nowSeconds() {
  return Math.floor(Date.now() / 1000);
}

function firstValue(value) {
  if (Array.isArray(value)) {
    return value[0];
  }
  return value;
}

function safeString(value, fallback = "") {
  return typeof value === "string" ? value : fallback;
}

function toBuffer(data) {
  if (!data) {
    return Buffer.alloc(0);
  }
  if (Buffer.isBuffer(data)) {
    return data;
  }
  return Buffer.from(String(data), "utf8");
}

export class MockIppPrinter extends EventEmitter {
  constructor(options = {}) {
    super();

    this.host = options.host || process.env.MOCK_IPP_HOST || "127.0.0.1";
    this.port = Number(options.port || process.env.MOCK_IPP_PORT || "8631");
    this.path = options.path || process.env.MOCK_IPP_PATH || "/ipp/print";
    this.name = options.name || process.env.MOCK_IPP_NAME || "NodePrinterMock";
    this.supportedFormats = options.supportedFormats || DEFAULT_FORMATS;
    this.supportedOperations = options.supportedOperations || DEFAULT_OPERATIONS;
    this.completionTimeoutMs = Number(
      options.completionTimeoutMs ?? process.env.MOCK_IPP_COMPLETE_MS ?? "50",
    );

    this.server = undefined;
    this.nextJobId = 1;
    this.jobs = new Map();
    this.timers = new Map();
  }

  get uri() {
    return `ipp://${this.host}:${this.port}${this.path}`;
  }

  isRunning() {
    return Boolean(this.server?.listening);
  }

  async start() {
    if (this.isRunning()) {
      return;
    }

    this.server = http.createServer((req, res) => {
      this.#handleHttp(req, res).catch((error) => {
        res.writeHead(500, { "content-type": "application/json" });
        res.end(JSON.stringify({ error: error.message }));
      });
    });

    await new Promise((resolve, reject) => {
      const onError = (error) => reject(error);
      this.server.once("error", onError);
      this.server.listen(this.port, this.host, () => {
        this.server.off("error", onError);
        resolve();
      });
    });
  }

  async stop() {
    if (!this.server) {
      return;
    }

    for (const jobId of this.timers.keys()) {
      this.#clearTimer(jobId);
    }

    const serverToClose = this.server;
    this.server = undefined;
    await new Promise((resolve) => serverToClose.close(() => resolve()));
  }

  reset() {
    for (const jobId of this.timers.keys()) {
      this.#clearTimer(jobId);
    }
    this.jobs.clear();
    this.nextJobId = 1;
  }

  setCompletionTimeout(ms) {
    this.completionTimeoutMs = Number(ms);
  }

  setJobStatus(jobId, status) {
    const normalized = String(status || "").toLowerCase();
    if (normalized === "pending") {
      return this.#setJobState(jobId, "pending", "none");
    }
    if (normalized === "processing") {
      return this.#setJobState(jobId, "processing", "job-printing");
    }
    if (normalized === "done" || normalized === "completed") {
      return this.#setJobState(jobId, "completed", "job-completed-successfully");
    }
    if (normalized === "cancel" || normalized === "canceled" || normalized === "cancelled") {
      return this.#setJobState(jobId, "canceled", "job-canceled-by-operator");
    }
    if (normalized === "aborted") {
      return this.#setJobState(jobId, "aborted", "aborted-by-system");
    }
    return false;
  }

  listJobs(options = {}) {
    const includeData = Boolean(options.includeData);
    return [...this.jobs.values()]
      .sort((a, b) => a.id - b.id)
      .map((job) => this.#formatJob(job, includeData));
  }

  waitForJob(predicate, options = {}) {
    const timeoutMs = Number(options.timeoutMs || 1000);

    const existing = this.listJobs({ includeData: true }).find((job) => predicate(job));
    if (existing) {
      return Promise.resolve(existing);
    }

    return new Promise((resolve, reject) => {
      const onChange = (job) => {
        if (!predicate(job)) {
          return;
        }
        cleanup();
        resolve(job);
      };

      const timeout = setTimeout(() => {
        cleanup();
        reject(new Error(`Timed out after ${timeoutMs}ms`));
      }, timeoutMs);

      const cleanup = () => {
        clearTimeout(timeout);
        this.off("job-change", onChange);
      };

      this.on("job-change", onChange);
    });
  }

  #opAttrs(reqMsg) {
    return reqMsg["operation-attributes-tag"] || {};
  }

  #jobIdFromOperationAttributes(operationAttributes) {
    const idValue = Number(firstValue(operationAttributes["job-id"]));
    if (Number.isFinite(idValue) && idValue > 0) {
      return idValue;
    }

    const uri = safeString(firstValue(operationAttributes["job-uri"]));
    if (!uri) {
      return NaN;
    }

    const match = uri.match(/\/(\d+)$/);
    if (!match) {
      return NaN;
    }

    return Number(match[1]);
  }

  #baseIppResponse(reqMsg, statusCode = "successful-ok", extraOperationAttrs = {}) {
    return {
      version: reqMsg.version || "2.0",
      id: reqMsg.id,
      statusCode,
      "operation-attributes-tag": {
        "attributes-charset": "utf-8",
        "attributes-natural-language": "en-us",
        ...extraOperationAttrs,
      },
    };
  }

  #jobAttributes(job) {
    return {
      "job-uri": `${this.uri}/${job.id}`,
      "job-id": job.id,
      "job-name": job.name,
      "job-originating-user-name": job.user,
      "job-state": job.state,
      "job-state-reasons": job.stateReason,
      "job-impressions-completed": job.state === "completed" ? 1 : 0,
      "job-media-sheets-completed": job.state === "completed" ? 1 : 0,
      "time-at-creation": job.createdAt,
      "time-at-processing": job.processingAt || 0,
      "time-at-completed": job.completedAt || 0,
    };
  }

  #printerAttributes() {
    const activeJob = [...this.jobs.values()].some((job) => job.state === "processing");

    return {
      "printer-uri-supported": this.uri,
      "printer-is-accepting-jobs": true,
      "queued-job-count": [...this.jobs.values()].filter(
        (job) =>
          job.state === "pending" || job.state === "pending-held" || job.state === "processing",
      ).length,
      "printer-state": activeJob ? "processing" : "idle",
      "printer-state-message": activeJob ? "Printing." : "Idle.",
      "printer-state-reasons": "none",
      "printer-name": this.name,
      "printer-info": "Node mock IPP printer",
      "printer-make-and-model": "node-printer mock ipp",
      "document-format-supported": this.supportedFormats,
      "operations-supported": this.supportedOperations,
      "compression-supported": ["none"],
      "copies-supported": [1, 1],
      "print-color-mode-supported": ["monochrome"],
    };
  }

  #clearTimer(jobId) {
    const timer = this.timers.get(jobId);
    if (!timer) {
      return;
    }
    clearTimeout(timer);
    this.timers.delete(jobId);
  }

  #scheduleCompletion(job) {
    if (this.completionTimeoutMs <= 0) {
      return;
    }

    this.#clearTimer(job.id);
    const timer = setTimeout(() => {
      this.#setJobState(job.id, "completed", "job-completed-successfully");
    }, this.completionTimeoutMs);
    this.timers.set(job.id, timer);
  }

  #emitJobChange(job) {
    this.emit("job-change", this.#formatJob(job, true));
  }

  #setJobState(jobId, newState, stateReason) {
    const job = this.jobs.get(jobId);
    if (!job) {
      return false;
    }

    job.state = newState;
    job.stateReason = stateReason || job.stateReason;

    if (newState === "processing") {
      job.processingAt = nowSeconds();
    }
    if (newState === "completed" || newState === "canceled" || newState === "aborted") {
      job.completedAt = nowSeconds();
      this.#clearTimer(jobId);
    }

    this.#emitJobChange(job);
    return true;
  }

  #createJob(reqMsg) {
    const attributes = this.#opAttrs(reqMsg);
    const jobId = this.nextJobId++;
    const jobName = safeString(firstValue(attributes["job-name"]), "node print job");
    const userName = safeString(firstValue(attributes["requesting-user-name"]), "unknown");

    const job = {
      id: jobId,
      name: jobName,
      user: userName,
      state: "pending",
      stateReason: "job-incoming",
      createdAt: nowSeconds(),
      processingAt: 0,
      completedAt: 0,
      data: Buffer.alloc(0),
    };

    this.jobs.set(jobId, job);
    this.#emitJobChange(job);
    return job;
  }

  #formatJob(job, includeData = false) {
    const base = {
      id: job.id,
      name: job.name,
      user: job.user,
      state: job.state,
      stateReason: job.stateReason,
      createdAt: job.createdAt,
      processingAt: job.processingAt,
      completedAt: job.completedAt,
      bytes: job.data.length,
    };

    if (includeData) {
      base.dataBase64 = job.data.toString("base64");
      base.dataUtf8 = job.data.toString("utf8");
    }

    return base;
  }

  async #parseIppRequest(req) {
    const chunks = [];
    for await (const chunk of req) {
      chunks.push(chunk);
    }

    return ipp.parse(Buffer.concat(chunks));
  }

  #sendIppResponse(res, payload) {
    const body = ipp.serialize(payload);
    res.writeHead(200, {
      "content-type": "application/ipp",
      "content-length": String(body.length),
    });
    res.end(body);
  }

  async #handleIpp(req, res) {
    let reqMsg;
    try {
      reqMsg = await this.#parseIppRequest(req);
    } catch {
      this.#sendIppResponse(
        res,
        this.#baseIppResponse({ id: 1, version: "2.0" }, "client-error-bad-request"),
      );
      return;
    }

    const operation = reqMsg.operation;
    const operationAttributes = this.#opAttrs(reqMsg);

    if (operation === "Get-Printer-Attributes") {
      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg),
        "printer-attributes-tag": this.#printerAttributes(),
      });
      return;
    }

    if (operation === "Validate-Job") {
      const format = safeString(
        firstValue(operationAttributes["document-format"]),
        "application/octet-stream",
      );
      if (!this.supportedFormats.includes(format)) {
        this.#sendIppResponse(
          res,
          this.#baseIppResponse(reqMsg, "client-error-document-format-not-supported"),
        );
        return;
      }
      this.#sendIppResponse(res, this.#baseIppResponse(reqMsg));
      return;
    }

    if (operation === "Create-Job") {
      const job = this.#createJob(reqMsg);
      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg, "successful-ok", {
          "job-uri": `${this.uri}/${job.id}`,
          "job-id": job.id,
        }),
        "job-attributes-tag": this.#jobAttributes(job),
      });
      return;
    }

    if (operation === "Send-Document") {
      const jobId = this.#jobIdFromOperationAttributes(operationAttributes);
      const job = this.jobs.get(jobId);
      if (!job) {
        this.#sendIppResponse(res, this.#baseIppResponse(reqMsg, "client-error-not-found"));
        return;
      }

      const incoming = toBuffer(reqMsg.data);
      job.data = Buffer.concat([job.data, incoming]);
      this.#setJobState(job.id, "processing", "job-printing");

      if (firstValue(operationAttributes["last-document"]) !== false) {
        this.#scheduleCompletion(job);
      }

      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg),
        "job-attributes-tag": this.#jobAttributes(job),
      });
      return;
    }

    if (operation === "Print-Job") {
      const format = safeString(
        firstValue(operationAttributes["document-format"]),
        "application/octet-stream",
      );
      if (!this.supportedFormats.includes(format)) {
        this.#sendIppResponse(
          res,
          this.#baseIppResponse(reqMsg, "client-error-document-format-not-supported"),
        );
        return;
      }

      const job = this.#createJob(reqMsg);
      job.data = toBuffer(reqMsg.data);
      this.#setJobState(job.id, "processing", "job-printing");
      this.#scheduleCompletion(job);

      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg, "successful-ok", {
          "job-uri": `${this.uri}/${job.id}`,
          "job-id": job.id,
        }),
        "job-attributes-tag": this.#jobAttributes(job),
      });
      return;
    }

    if (operation === "Get-Job-Attributes") {
      const jobId = this.#jobIdFromOperationAttributes(operationAttributes);
      const job = this.jobs.get(jobId);
      if (!job) {
        this.#sendIppResponse(res, this.#baseIppResponse(reqMsg, "client-error-not-found"));
        return;
      }

      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg),
        "job-attributes-tag": this.#jobAttributes(job),
      });
      return;
    }

    if (operation === "Get-Jobs") {
      const firstJob = this.jobs.values().next().value;
      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg),
        "job-attributes-tag": firstJob ? this.#jobAttributes(firstJob) : {},
      });
      return;
    }

    if (operation === "Cancel-Job") {
      const jobId = this.#jobIdFromOperationAttributes(operationAttributes);
      const job = this.jobs.get(jobId);
      if (!job) {
        this.#sendIppResponse(res, this.#baseIppResponse(reqMsg, "client-error-not-found"));
        return;
      }

      this.#setJobState(jobId, "canceled", "job-canceled-by-user");
      this.#sendIppResponse(res, {
        ...this.#baseIppResponse(reqMsg),
        "job-attributes-tag": this.#jobAttributes(job),
      });
      return;
    }

    this.#sendIppResponse(
      res,
      this.#baseIppResponse(reqMsg, "server-error-operation-not-supported"),
    );
  }

  async #handleHttp(req, res) {
    const url = new URL(
      req.url || "/",
      `http://${req.headers.host || `${this.host}:${this.port}`}`,
    );
    if (url.pathname !== this.path) {
      res.writeHead(404, { "content-type": "text/plain" });
      res.end("Not found");
      return;
    }

    await this.#handleIpp(req, res);
  }
}

export function createMockIppPrinter(options) {
  return new MockIppPrinter(options);
}
