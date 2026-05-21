import * as printer from "../lib/index.js";
import { inspect } from "node:util";

console.log(
  "supported formats are:\n" +
    inspect(printer.getSupportedPrintFormats(), { colors: true, depth: 10 }),
);
