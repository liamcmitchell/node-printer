import { getAllPrinterDetails } from "../lib/index.js";

console.log("installed printers:", await getAllPrinterDetails());
