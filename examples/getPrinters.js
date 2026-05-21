import * as printer from "../lib/index.js";
import { inspect } from "node:util";

console.log("installed printers:\n" + inspect(printer.getPrinters(), { colors: true, depth: 10 }));
