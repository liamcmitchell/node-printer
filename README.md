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

// Get job details
function getJob(printer: string, jobId: number): JobDetails;

// Send a command to a job, e.g. "CANCEL"
function setJob(printer: string, jobId: number, command: string): boolean;

function getSupportedPrintFormats(): string[];
function getSupportedJobCommands(): string[];
```

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
