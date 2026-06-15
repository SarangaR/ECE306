#!/usr/bin/env node
/**
 * build.js — local helper that copies index.html to docs/ for GitHub Pages.
 *
 * You typically don't need to run this manually.  The GitHub Actions workflow
 * (.github/workflows/pages.yml) deploys automatically on every push to main.
 *
 * Use this only if you want to preview the site output locally or deploy
 * manually from the docs/ folder instead of using Actions.
 *
 * Usage:
 *   node build.js        → copies index.html → docs/index.html
 *
 * Then commit docs/ and configure GitHub Pages:
 *   Settings → Pages → Deploy from branch: main → /docs
 *
 * For day-to-day use just push to main and let Actions handle it.
 */

'use strict';

const fs   = require('fs');
const path = require('path');

const SRC  = path.join(__dirname, 'index.html');
const DOCS = path.join(__dirname, 'docs');
const DEST = path.join(DOCS, 'index.html');

if (!fs.existsSync(SRC)) {
  console.error(`[build] ERROR: source not found: ${SRC}`);
  process.exit(1);
}

fs.mkdirSync(DOCS, { recursive: true });
fs.copyFileSync(SRC, DEST);

// Also write .nojekyll so GitHub doesn't process the site with Jekyll.
fs.writeFileSync(path.join(DOCS, '.nojekyll'), '');

const size = (fs.statSync(DEST).size / 1024).toFixed(1);
console.log(`[build] ✓  ${path.relative(process.cwd(), DEST)}  (${size} kB)`);
console.log('[build]    Push to GitHub — Actions will deploy automatically.');
console.log('[build]    Or: Settings → Pages → Deploy from branch main /docs');
