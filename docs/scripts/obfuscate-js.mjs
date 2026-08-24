/**
 * Obfuscate + minify site JS for GitHub Pages.
 *
 * Source:  docs/js/*.js  (edit these)
 * Output:  docs/js/min/*.js  (what index.html / manual.html load)
 *
 * Usage (from docs/):
 *   npm install
 *   npm run obfuscate
 */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import JavaScriptObfuscator from 'javascript-obfuscator';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const jsDir = path.resolve(__dirname, '..', 'js');
const outDir = path.join(jsDir, 'min');

const FILES = [
  'lang.js',
  'gb2312_codec.js',
  'flash.js',
  'airline_icao.js',
  'radar.js',
  'manual.js',
];

const BANNER =
  '/* AUTO-GENERATED — do not edit. Source: js/<name>. Rebuild: cd docs && npm run obfuscate */\n';

const baseOptions = {
  compact: true,
  simplify: true,
  identifierNamesGenerator: 'hexadecimal',
  renameGlobals: false,
  renameProperties: false,
  controlFlowFlattening: false,
  deadCodeInjection: false,
  selfDefending: false,
  stringArray: true,
  stringArrayEncoding: [],
  stringArrayThreshold: 0.75,
  splitStrings: false,
  transformObjectKeys: false,
  unicodeEscapeSequence: false,
  target: 'browser',
};

/** Huge GB2312 lookup table — compact only, do not string-array encode. */
const lightOptions = {
  ...baseOptions,
  stringArray: false,
  stringArrayEncoding: [],
};

fs.mkdirSync(outDir, { recursive: true });

for (const file of FILES) {
  const srcPath = path.join(jsDir, file);
  const src = fs.readFileSync(srcPath, 'utf8');
  const opts = file === 'gb2312_codec.js' ? lightOptions : baseOptions;
  const started = Date.now();
  const result = JavaScriptObfuscator.obfuscate(src, opts);
  const out = BANNER.replace('<name>', file) + result.getObfuscatedCode() + '\n';
  const dest = path.join(outDir, file);
  fs.writeFileSync(dest, out, 'utf8');
  const inKb = (src.length / 1024).toFixed(1);
  const outKb = (out.length / 1024).toFixed(1);
  console.log(`${file}: ${inKb} KB → ${outKb} KB (${Date.now() - started} ms)`);
}

console.log('Wrote', outDir);
