(function (root) {
  'use strict';
  const defaults = Object.freeze({
    'general.webui': '0',
    'general.win32k': '0', 'general.debug_log': '0',
    'general.disable_features': '', 'general.command_line': '',
    'dir_setting.data': '', 'dir_setting.cache': '', 'hotkey.boss_key': ''
  });
  function scan(text) {
    let section = '';
    return text.replace(/^\uFEFF/, '').split(/\r\n|\n|\r/).map(line => {
      const heading = line.match(/^\s*\[([^\]]+)\]\s*(?:[;#].*)?$/);
      if (heading) section = heading[1].trim().toLowerCase();
      const match = !/^\s*[;#]/.test(line) && line.match(/^(\s*([^=]+?)\s*=\s*)(.*)$/);
      return { line, section, key: match ? `${section}.${match[2].trim().toLowerCase()}` : null,
        prefix: match ? match[1] : '', value: match ? match[3].trim() : '' };
    });
  }
  function read(text) {
    const values = { ...defaults }, seen = new Set();
    for (const row of scan(text)) {
      if (Object.hasOwn(defaults, row.key) && !seen.has(row.key)) {
        const value = row.value;
        values[row.key] = /^(["']).*\1$/.test(value) ? value.slice(1, -1) : value;
        seen.add(row.key);
      }
    }
    return values;
  }
  function update(text, values) {
    const original = read(text), newline = text.includes('\r\n') ? '\r\n' : '\n';
    const rows = scan(text), pending = new Map();
    for (const [key, value] of Object.entries(values)) {
      if (!Object.hasOwn(defaults, key)) throw new Error(`Unknown setting: ${key}`);
      if (typeof value !== 'string' || /[\r\n\0]/.test(value)) throw new Error(`Invalid INI value: ${key}`);
      if (value !== original[key]) pending.set(key, value);
    }
    for (const row of rows) {
      if (pending.has(row.key)) {
        row.line = row.prefix + pending.get(row.key);
        pending.delete(row.key);
      }
    }
    for (const [key, value] of pending) {
      const split = key.indexOf('.'), section = key.slice(0, split), name = key.slice(split + 1);
      let at = rows.findIndex(row => /^\s*\[/.test(row.line) && row.section === section);
      if (at < 0) {
        if (rows.length && rows.at(-1).line !== '') rows.push({ line: '' });
        rows.push({ line: `[${section}]`, section });
        at = rows.length - 1;
      }
      let end = at + 1;
      while (end < rows.length && !/^\s*\[/.test(rows[end].line)) end++;
      rows.splice(end, 0, { line: `${name}=${value}`, section });
    }
    const result = rows.map(row => row.line).join(newline);
    return result.endsWith(newline) ? result : result + newline;
  }
  function decode(bytes) {
    const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    if (data[0] === 0xff && data[1] === 0xfe) return new TextDecoder('utf-16le', { fatal: true }).decode(data);
    if (data[0] === 0xfe && data[1] === 0xff) return new TextDecoder('utf-16be', { fatal: true }).decode(data);
    try { return new TextDecoder('utf-8', { fatal: true }).decode(data); }
    catch { throw new Error('encoding'); }
  }
  function encode(text) {
    const bytes = new Uint8Array(2 + text.length * 2);
    bytes[0] = 0xff; bytes[1] = 0xfe;
    const view = new DataView(bytes.buffer);
    for (let i = 0; i < text.length; i++) view.setUint16(2 + i * 2, text.charCodeAt(i), true);
    return bytes;
  }
  const api = Object.freeze({ defaults, read, update, decode, encode });
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VivaldiIni = api;
})(globalThis);
