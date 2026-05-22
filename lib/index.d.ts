export function getPrinters(): PrinterDetails[];
export function getPrinter(printerName?: string): PrinterDetails;
export function getDefaultPrinterName(): string | undefined;
export function printDirect(options: PrintDirectOptions): number;
export function printFile(options: PrintFileOptions): number;
export function getSupportedPrintFormats(): string[];
export function getJob(printerName: string, jobId: number): JobDetails;
export function setJob(printerName: string, jobId: number, command: string): boolean;
export function getSupportedJobCommands(): string[];

export interface PrintDirectOptions {
  data: string | Uint8Array;
  printer?: string;
  type?: string;
  docname?: string;
  options?: Record<string, string>;
}

export interface PrintFileOptions {
  filename: string;
  printer?: string;
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
