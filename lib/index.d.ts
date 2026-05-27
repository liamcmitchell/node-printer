export function getPrinters(): PrinterDetails[];
export function getPrinter(printer: string): PrinterDetails | null;
export function print(options: PrintOptions): number;
export function getSupportedPrintFormats(): string[];
export function getJob(printer: string, jobId: number): JobDetails | null;
export function cancelJob(printer: string, jobId: number): void;

export interface PrintOptions {
  printer: string;
  data?: string | Uint8Array;
  filename?: string;
  format?: string;
  docname?: string;
  options?: Record<string, string>;
}

export interface PrinterDetails {
  name: string;
  isDefault: boolean;
  state: "idle" | "processing" | "stopped";
  stateReasons: string[];
  jobs: JobDetails[];
  raw: Record<string, any>;
}

export interface JobDetails {
  id: number;
  name: string;
  printerName: string;
  user: string;
  state:
    | "pending"
    | "pending-held"
    | "processing"
    | "processing-stopped"
    | "canceled"
    | "aborted"
    | "completed";
  size: number;
  createdAt: number;
  processingAt: number;
  completedAt: number;
  raw: Record<string, any>;
}
