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
import { getPrinters, print, getJob } from "node-printer";

const printers = getPrinters();
const printer = printers.find((p) => p.isDefault)?.name;
if (printer) {
  const jobId = print({
    printer,
    format: "TEXT",
    data: "Text",
  });
  console.log(getJob(printer, jobId));
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

// Get all printers
function getPrinters(): PrinterDetails[];

// Get specific printer
function getPrinter(printer: string): PrinterDetails;

// Send data or a file to a printer, returns the job ID
function print(options: {
  printer: string; // printer name
  data?: string | Uint8Array; // data to print
  format?: string; // data format, e.g. "RAW", "TEXT" (default "RAW")
  filename?: string; // path to file (POSIX only, alternative to data)
  docname?: string; // document name shown in the queue
  options?: Record<string, string>; // platform-specific print options
}): number;

// Get job details, or null if the job no longer exists
function getJob(printer: string, jobId: number): JobDetails | null;

// Cancel a print job; no-op if the job no longer exists
function cancelJob(printer: string, jobId: number): void;

// Returns the built-in format aliases accepted by the format option
function getSupportedPrintFormats(): string[];
```

### Print formats

`getSupportedPrintFormats()` returns built-in case-sensitive aliases (`RAW`, `TEXT`, `PDF`, …) that are translated to MIME types before being passed to the platform spooler. Any string not in this list is passed to the spooler as-is, as a MIME type.

| Alias        | MIME type                                                      |
| ------------ | -------------------------------------------------------------- |
| `RAW`        | `application/vnd.cups-raw` (POSIX) / raw passthrough (Windows) |
| `TEXT`       | `text/plain`                                                   |
| `PDF`        | `application/pdf`                                              |
| `JPEG`       | `image/jpeg`                                                   |
| `POSTSCRIPT` | `application/postscript`                                       |

**POSIX (CUPS):** When a MIME type is passed directly, CUPS looks it up in its filter database. If no filter is registered for the type, CUPS falls back to `application/octet-stream`, which may cause the job to be held.

**Windows:** The data type string is passed directly to the Windows spooler. Custom data types are supported as long as the printer driver recognises them.

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
