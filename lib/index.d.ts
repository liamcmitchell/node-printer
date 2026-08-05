export function getAllPrinterDetails(): Promise<PrinterDetails[]>;
export function getPrinterDetails(printer: string): Promise<PrinterDetails | null>;
export function hasPrinter(printer: string): Promise<boolean>;
export function getDefaultPrinterName(): Promise<string | null>;
export function print(options: PrintOptions): Promise<number>;
export function getSupportedPrintFormats(): Promise<string[]>;
export function getJob(printer: string, jobId: number): Promise<JobDetails | null>;
export function cancelJob(printer: string, jobId: number): Promise<void>;

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
