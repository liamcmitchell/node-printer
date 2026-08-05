# Node Printer

Native printing APIs for Node.js on Windows/Linux/macOS.

Requires `node-gyp` platform prerequisites: https://github.com/nodejs/node-gyp#installation

Linux also needs CUPS development headers to build, and a CUPS server at runtime.

```bash
sudo apt-get update -y
sudo apt-get install -y libcups2-dev cups cups-client cups-bsd
sudo systemctl start cups
```

```
npm install github:liamcmitchell/node-printer
```

```js
import {
  getAllPrinterDetails,
  getDefaultPrinterName,
  print,
  getJob,
} from "node-printer";

console.log(await getAllPrinterDetails());
const printer = await getDefaultPrinterName();
if (printer) {
  const jobId = await print({
    printer,
    format: "TEXT",
    data: "Text",
  });
  console.log(await getJob(printer, jobId));
}
```

More usage examples in [examples/](examples).

## API

```ts
interface PrinterDetails {
  name: string;
  isDefault: boolean;
  state: "idle" | "processing" | "stopped"; // IPP printer-state
  stateReasons: string[]; // IPP printer-state-reasons keywords
  jobs: JobDetails[];
  raw: Record<string, any>; // platform-specific data (CUPS options / Windows PRINTER_INFO_2)
}

interface JobDetails {
  id: number; // job identifier
  name: string; // document/job title
  printerName: string; // target printer
  user: string; // submitting user
  state: // IPP job-state
    | "pending"
    | "pending-held"
    | "processing"
    | "processing-stopped"
    | "canceled"
    | "aborted"
    | "completed";
  size: number; // size in bytes
  createdAt: number; // epoch seconds
  processingAt: number; // epoch seconds (0 if not yet processing)
  completedAt: number; // epoch seconds (0 if not yet completed)
  raw: Record<string, any>; // platform-specific data
}

// Get all printers with details
function getAllPrinterDetails(): Promise<PrinterDetails[]>;

// Get specific printer details, or null if it does not exist
function getPrinterDetails(printer: string): Promise<PrinterDetails | null>;

// Returns true if a printer exists
function hasPrinter(printer: string): Promise<boolean>;

// Get the default printer name, or null if no default is configured
function getDefaultPrinterName(): Promise<string | null>;

// Send data or a file to a printer, returns the job ID
function print(options: {
  printer: string; // printer name
  data?: string | Uint8Array; // data to print
  format?: string; // data format, e.g. "RAW", "TEXT" (default "RAW")
  filename?: string; // path to file (POSIX only, alternative to data)
  docname?: string; // document name shown in the queue
  options?: Record<string, string>; // platform-specific print options
}): Promise<number>;

// Get job details, or null if the job no longer exists
function getJob(printer: string, jobId: number): Promise<JobDetails | null>;

// Cancel a print job; no-op if the job no longer exists
function cancelJob(printer: string, jobId: number): Promise<void>;

// Returns the built-in format aliases accepted by the format option
function getSupportedPrintFormats(): Promise<string[]>;
```

### Print formats

Behaviour differs by platform:

**POSIX (CUPS):** `getSupportedPrintFormats()` returns fixed aliases. When printing, aliases are translated to MIME types and passed to CUPS. Any string not in the alias list is passed to CUPS as-is as a MIME type; if no CUPS filter is registered for it, the job may be held.

| Alias        | CUPS MIME type             |
| ------------ | -------------------------- |
| `RAW`        | `application/vnd.cups-raw` |
| `TEXT`       | `text/plain`               |
| `PDF`        | `application/pdf`          |
| `JPEG`       | `image/jpeg`               |
| `POSTSCRIPT` | `application/postscript`   |

**Windows:** `getSupportedPrintFormats()` queries `EnumPrintProcessorDatatypes` and returns the datatypes supported by the installed print processors (typically `RAW`, `TEXT`, `NT EMF 1.008`, `XPS_PASS` — no PDF or JPEG). The format string is passed directly to `StartDocPrinterW` as `pDatatype` with no translation; the printer driver must natively recognise it.

> **PDF on Windows:** Passing `PDF` data on Windows will only work if the specific printer driver natively accepts PDF (e.g. Microsoft Print to PDF). Most hardware printer drivers do not — they require PDF to be converted to GDI/EMF first, which Windows has no built-in API for. In an Electron app, use [`webContents.print()`](https://www.electronjs.org/docs/latest/api/web-contents#contentsprintoptions-callback) instead, which routes through Chromium's PDFium-based PDF→EMF pipeline.

## Testing

On macOS/Linux we need to register a mock printer with CUPS:

```bash
sudo lpadmin -x NodePrinterMock 2>/dev/null || true
sudo lpadmin -p NodePrinterMock -E -v ipp://127.0.0.1:8631/ipp/print
lpstat -v NodePrinterMock
```

On Windows, we use `Microsoft Print to PDF` which should be available by default.

Run tests:

```bash
npm run test
```

## Contibutors

- Ion Lupascu, ionlupascu@gmail.com
- Timo Kunze, @timokunze
- Thiago Lugli, @thiagoelg
- Eko Eryanto, @ekoeryanto
- Liam Mitchell, liam.mitchell@siemens.com

## License

[MIT](LICENSE)
