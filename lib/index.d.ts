export function getPrinters(): PrinterDetails[];
export function getPrinter(printer: string): PrinterDetails;
export function print(options: PrintOptions): number;
export function getSupportedPrintFormats(): string[];
export function getJob(printer: string, jobId: number): JobDetails;
export function setJob(printer: string, jobId: number, command: string): boolean;
export function getSupportedJobCommands(): string[];

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
  options: Record<string, string>;
  status?: string;
  jobs?: JobDetails[];
}

export interface JobDetails {
  id: number;
  name: string;
  printerName: string;
  user: string;
  format: string;
  priority: number;
  size: number;
  status: string[];
  completedTime: Date;
  creationTime: Date;
  processingTime: Date;
}
