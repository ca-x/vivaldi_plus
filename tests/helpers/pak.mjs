import { gunzipSync, brotliDecompressSync, gzipSync } from 'node:zlib';

export function readPak(bytes) {
  if (bytes.length < 18 || bytes.readUInt32LE(0) !== 5) throw new Error('Only Chromium DataPack v5 is supported');
  const count = bytes.readUInt16LE(8);
  const aliasCount = bytes.readUInt16LE(10);
  const dataStart = 12 + (count + 1) * 6 + aliasCount * 4;
  if (dataStart > bytes.length) throw new Error('Truncated DataPack index');
  const entries = [];
  let previousId = -1;
  for (let i = 0; i < count; i++) {
    const p = 12 + i * 6;
    const id = bytes.readUInt16LE(p);
    const start = bytes.readUInt32LE(p + 2);
    const end = bytes.readUInt32LE(p + 8);
    if (id <= previousId || start < dataStart || end < start || end > bytes.length) throw new Error('Invalid DataPack entry');
    entries.push({ id, bytes: bytes.subarray(start, end) });
    previousId = id;
  }
  if (bytes.readUInt32LE(12 + count * 6 + 2) !== bytes.length) throw new Error('Unexpected DataPack trailing data');
  const aliases = bytes.subarray(12 + (count + 1) * 6, dataStart);
  for (let i = 0; i < aliasCount; i++) {
    if (aliases.readUInt16LE(i * 4 + 2) >= count) throw new Error('Invalid DataPack alias');
  }
  return { header: bytes.subarray(0, 12), entries, aliases };
}

export function writePak(pak) {
  const index = Buffer.alloc((pak.entries.length + 1) * 6);
  let offset = 12 + index.length + pak.aliases.length;
  pak.entries.forEach((entry, i) => {
    index.writeUInt16LE(entry.id, i * 6);
    index.writeUInt32LE(offset, i * 6 + 2);
    offset += entry.bytes.length;
  });
  index.writeUInt32LE(offset, pak.entries.length * 6 + 2);
  return Buffer.concat([pak.header, index, pak.aliases, ...pak.entries.map(e => e.bytes)]);
}

export function decodeResource(bytes) {
  if (bytes[0] === 0x1f && bytes[1] === 0x8b) return gunzipSync(bytes);
  if (bytes[0] === 0x1e && bytes[1] === 0x9b) {
    if (bytes.length < 8) throw new Error('Truncated Brotli resource');
    const result = brotliDecompressSync(bytes.subarray(8));
    // Chromium uses a 2-byte signature and a 6-byte little-endian decoded size.
    if (bytes.readUIntLE(2, 6) !== result.length) throw new Error('Invalid Brotli resource size');
    return result;
  }
  return bytes;
}

function htmlEscape(text) {
  return text.replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

export function patchAbout(bytes, settingsUrl, version) {
  const parsedUrl = new URL(settingsUrl);
  if (parsedUrl.protocol !== 'file:') throw new Error('Settings must be a local file URL');
  const pak = readPak(bytes);
  const matches = pak.entries.filter(entry => {
    const text = decodeResource(entry.bytes).toString('utf8');
    return text.includes('about:version template page') && text.includes('id="executable_path"') && text.includes('</table>');
  });
  if (matches.length !== 1) throw new Error(`Expected one about:version template, found ${matches.length}; browser resources left unchanged`);
  const entry = matches[0];
  let html = decodeResource(entry.bytes).toString('utf8');
  const marker = /\s*<!-- vivaldi-plus:start -->[\s\S]*?<!-- vivaldi-plus:end -->/g;
  html = html.replace(marker, '');
  parsedUrl.searchParams.set('version', version);
  const row = `\n<!-- vivaldi-plus:start -->\n<tr id="vivaldi-plus"><td class="label">Vivaldi++</td><td class="version">${htmlEscape(version)} &middot; <a id="vivaldi-plus-settings" href="${htmlEscape(parsedUrl.href)}" target="_blank" rel="noopener">Settings / 设置</a> &middot; <a href="https://github.com/ca-x/vivaldi_plus" target="_blank" rel="noopener">GitHub</a></td></tr>\n<!-- vivaldi-plus:end -->\n`;
  html = html.replace('</table>', `${row}</table>`);
  // DataPack's gzip marker works regardless of the original compression.
  entry.bytes = gzipSync(Buffer.from(html));
  const result = writePak(pak);
  readPak(result);
  return { bytes: result, resourceId: entry.id, html };
}
