import * as printer from "../lib/index.js"; //=require('pritner')
import { inspect } from "node:util";

console.log(
  "supported job commands:\n" +
    inspect(printer.getSupportedJobCommands(), { colors: true, depth: 10 }),
);
