(function () {
  'use strict';
  const $ = id => document.getElementById(id);
  const fields = [...document.querySelectorAll('[data-setting]')];
  const template = '[general]\r\nwin32k=0\r\ndebug_log=0\r\ndisable_features=\r\ncommand_line=\r\n\r\n[dir_setting]\r\ndata=\r\ncache=\r\n\r\n[hotkey]\r\nboss_key=\r\n';
  const state = { source: template, handle: null, bytes: null, name: '', snapshot: {}, busy: false, message: 'ready', error: false };
  const token = document.querySelector('meta[name="vivaldi-plus-token"]')?.content || '';
  const bridge = location.protocol === 'http:' && location.hostname === '127.0.0.1' && token.length >= 16 && token !== '__VIVALDI_PLUS_TOKEN__';
  let revision = null;
  function preference(key, fallback) {
    try { return localStorage.getItem(`vivaldi-plus.${key}`) || fallback; } catch { return fallback; }
  }
  function persist(key, value) {
    try { localStorage.setItem(`vivaldi-plus.${key}`, value); } catch { /* Preferences remain active for this page session. */ }
  }
  let language = preference('language', navigator.language.startsWith('zh') ? 'zh-CN' : 'en');
  if (!Object.hasOwn(VivaldiMessages, language)) language = 'en';
  let theme = preference('theme', 'system');
  if (!['system', 'light', 'dark'].includes(theme)) theme = 'system';
  const t = key => VivaldiMessages[language][key] ?? VivaldiMessages.en[key] ?? key;
  const values = () => Object.fromEntries(fields.map(field => [field.dataset.setting, field.type === 'checkbox' ? (field.checked ? '1' : '0') : field.value]));
  const dirty = () => Object.entries(values()).some(([key, value]) => value !== state.snapshot[key]);
  function markDirty() {
    $('dirty-state').textContent = t(dirty() ? 'dirty' : 'clean');
    $('dirty-state').dataset.dirty = String(dirty());
  }
  function showStatus(key, error = false) {
    state.message = key; state.error = error;
    $('status').textContent = t(key); $('status').dataset.error = String(error);
  }
  function fileLabel() {
    $('file-name').textContent = state.name ? t(bridge ? 'connected' : state.handle ? 'fileLoaded' : 'fileImported') + state.name : t('fileNone');
  }
  function translate() {
    document.documentElement.lang = language;
    document.title = t('title');
    document.querySelectorAll('[data-i18n]').forEach(node => { node.textContent = t(node.dataset.i18n); });
    document.querySelectorAll('.field-error[data-error-key]').forEach(node => {
      node.textContent = t(node.dataset.errorKey);
      document.getElementById(node.dataset.field).setCustomValidity(t(node.dataset.errorKey));
    });
    document.querySelector('nav').setAttribute('aria-label', t('settings'));
    $('language').value = language;
    if (bridge) {
      $('file-title').textContent = t('connectedTitle');
      document.querySelector('[data-i18n="fileHelp"]').hidden = true;
      $('open').textContent = t('reload');
    }
    const version = new URLSearchParams(location.search).get('version');
    $('version').textContent = version && /^\d+(?:\.\d+){1,3}$/.test(version) ? version : t('versionUnknown');
    fileLabel(); markDirty(); showStatus(state.message, state.error);
  }
  function applyTheme() {
    document.documentElement.dataset.theme = theme === 'system' ? (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light') : theme;
    $('theme').value = theme;
  }
  function populate(source) {
    const settings = VivaldiIni.read(source);
    for (const field of fields) {
      const value = settings[field.dataset.setting];
      if (field.type === 'checkbox') field.checked = /^[-+]?\d+$/.test(value) && Number(value) !== 0;
      else field.value = value;
      field.setCustomValidity('');
      field.removeAttribute('aria-invalid');
      const error = $(`${field.id}-error`); if (error) error.remove();
    }
    state.snapshot = values(); markDirty();
  }
  function setBusy(busy) {
    state.busy = busy;
    document.querySelectorAll('button,input,select').forEach(control => { control.disabled = busy; });
    $('save').textContent = t(busy ? 'saving' : 'save');
    $('configuration').setAttribute('aria-busy', String(busy));
  }
  function validValues() {
    let valid = true;
    for (const field of fields.filter(item => item.type !== 'checkbox')) {
      const value = field.value, key = field.dataset.setting;
      let error = /[\r\n\0]/.test(value) || value.length > field.maxLength ? 'invalid' : '';
      if (key === 'hotkey.boss_key' && value && !error) {
        const parts = value.split('+');
        const modifiers = parts.slice(0, -1);
        const last = parts.at(-1);
        if (!modifiers.every(part => /^(ctrl|control|alt|shift|win)$/i.test(part)) || !/^(?:F(?:[1-9]|1\d|2[0-4])|left|right|up|down|esc|escape|tab|backspace|enter|return|space|prtsc|printscreen|scroll|pause|insert|delete|del|home|end|pageup|pagedown|pgup|pgdn|[^\s+])$/i.test(last)) error = 'keyInvalid';
      }
      const oldError = $(`${field.id}-error`); if (oldError) oldError.remove();
      field.setCustomValidity(error ? t(error) : '');
      if (error) field.setAttribute('aria-invalid', 'true');
      else field.removeAttribute('aria-invalid');
      if (error) {
        const message = document.createElement('p'); message.id = `${field.id}-error`;
        message.className = 'field-error'; message.dataset.errorKey = error; message.dataset.field = field.id;
        message.textContent = t(error); field.after(message);
        if (valid) field.focus(); valid = false;
      }
    }
    return valid;
  }
  async function readFile(file) {
    if (file.size > 1024 * 1024) throw new Error('largeError');
    const bytes = new Uint8Array(await file.arrayBuffer());
    let source;
    try { source = VivaldiIni.decode(bytes); } catch { throw new Error('encodingError'); }
    if (source.includes('\0')) throw new Error('encodingError');
    return { source, bytes, name: file.name };
  }
  async function acceptFile(file, handle) {
    const loaded = await readFile(file);
    state.source = loaded.source; state.bytes = loaded.bytes;
    state.name = loaded.name; state.handle = handle;
    populate(state.source); fileLabel(); showStatus('ready');
  }
  function report(error, fallback) {
    if (error.name === 'AbortError') return;
    showStatus(['largeError', 'encodingError', 'conflict'].includes(error.message) ? error.message : fallback, true);
  }
  async function bridgeRequest(method, source) {
    const headers = { 'X-Vivaldi-Plus-Token': token };
    const options = { method, headers, cache: 'no-store', credentials: 'same-origin', redirect: 'error' };
    if (method === 'PUT') {
      if (revision === null) throw new Error('loadError');
      headers['Content-Type'] = 'application/octet-stream';
      headers['X-Vivaldi-Plus-Revision'] = revision;
      options.body = VivaldiIni.encode(source);
    }
    const response = await fetch('/api/config', options);
    if (response.status === 409) throw new Error('conflict');
    if (!response.ok) throw new Error(method === 'GET' ? 'loadError' : 'saveError');
    const config = await response.json();
    if (typeof config.text !== 'string' || typeof config.exists !== 'boolean' || typeof config.revision !== 'string' || typeof config.path !== 'string') throw new Error('loadError');
    state.source = config.exists ? config.text : template;
    state.name = config.path; revision = config.revision;
    populate(state.source); fileLabel();
  }
  async function reloadBridge() {
    setBusy(true); showStatus('connecting');
    try { await bridgeRequest('GET'); showStatus('ready'); }
    catch (error) { report(error, 'loadError'); }
    finally { setBusy(false); }
  }
  $('open').addEventListener('click', async () => {
    if (state.busy || (dirty() && !confirm(t('discard')))) return;
    if (bridge) { await reloadBridge(); return; }
    if (!window.showOpenFilePicker) { $('file-input').click(); return; }
    setBusy(true);
    try {
      const [handle] = await showOpenFilePicker({ multiple: false, types: [{ description: 'INI', accept: { 'text/plain': ['.ini'] } }] });
      await acceptFile(await handle.getFile(), handle);
    } catch (error) { report(error, 'loadError'); }
    finally { setBusy(false); }
  });
  $('file-input').addEventListener('change', async event => {
    const file = event.target.files[0]; if (!file) return;
    setBusy(true);
    try { await acceptFile(file, null); } catch (error) { report(error, 'loadError'); }
    finally { event.target.value = ''; setBusy(false); }
  });
  function serialized() {
    const changed = Object.fromEntries(Object.entries(values()).filter(([key, value]) => value !== state.snapshot[key]));
    return VivaldiIni.update(state.source, changed);
  }
  function download() {
    if (!validValues()) return;
    const url = URL.createObjectURL(new Blob([VivaldiIni.encode(serialized())], { type: 'text/plain;charset=utf-16le' }));
    const link = document.createElement('a'); link.href = url; link.download = 'config.ini';
    document.body.append(link); link.click(); link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 30000);
    showStatus('downloaded');
  }
  $('download').addEventListener('click', download);
  $('configuration').addEventListener('submit', async event => {
    event.preventDefault(); if (state.busy || !validValues()) return;
    if (!bridge && !state.handle && !window.showSaveFilePicker) { download(); return; }
    setBusy(true);
    let writable;
    try {
      if (bridge) { await bridgeRequest('PUT', serialized()); markDirty(); showStatus('saved'); return; }
      const handle = state.handle || await showSaveFilePicker({ suggestedName: 'config.ini', types: [{ description: 'INI', accept: { 'text/plain': ['.ini'] } }] });
      if (state.handle) {
        const current = new Uint8Array(await (await handle.getFile()).arrayBuffer());
        if (current.length !== state.bytes.length || current.some((byte, index) => byte !== state.bytes[index])) throw new Error('conflict');
      }
      const source = serialized(), bytes = VivaldiIni.encode(source);
      writable = await handle.createWritable(); await writable.write(bytes); await writable.close(); writable = null;
      state.source = source; state.bytes = bytes; state.handle = handle; state.name = handle.name;
      state.snapshot = values(); fileLabel(); markDirty(); showStatus('saved');
    } catch (error) {
      if (writable) { try { await writable.abort(); } catch { /* Preserve the original write failure. */ } }
      report(error, 'saveError');
    } finally { setBusy(false); }
  });
  $('reset').addEventListener('click', () => {
    if (!confirm(t('resetConfirm'))) return;
    const baseline = state.snapshot;
    populate(template); state.snapshot = baseline; markDirty(); showStatus('ready');
  });
  fields.forEach(field => field.addEventListener('input', () => {
    field.setCustomValidity(''); field.removeAttribute('aria-invalid');
    const error = $(`${field.id}-error`); if (error) error.remove();
    markDirty();
  }));
  $('language').addEventListener('change', event => { language = event.target.value; persist('language', language); translate(); });
  $('theme').addEventListener('change', event => { theme = event.target.value; persist('theme', theme); applyTheme(); });
  matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => { if (theme === 'system') applyTheme(); });
  addEventListener('beforeunload', event => { if (dirty() || state.busy) { event.preventDefault(); event.returnValue = ''; } });
  const navigation = [...document.querySelectorAll('.sidebar nav a')];
  const sections = navigation.map(link => $(link.hash.slice(1)));
  let navigationTarget = null;
  function selectSection(id) {
    navigation.forEach(link => {
      if (link.hash === `#${id}`) link.setAttribute('aria-current', 'location');
      else link.removeAttribute('aria-current');
    });
  }
  let navigationFrame = 0;
  function trackSection() {
    navigationFrame = 0;
    if (navigationTarget) return;
    let selected = sections[0];
    for (const section of sections) {
      if (section.getBoundingClientRect().top <= Math.min(innerHeight * 0.25, 180)) selected = section;
    }
    if (scrollY + innerHeight >= document.documentElement.scrollHeight - 2) selected = sections.at(-1);
    selectSection(selected.id);
  }
  function scheduleNavigation() {
    if (!navigationFrame) navigationFrame = requestAnimationFrame(trackSection);
  }
  navigation.forEach(link => link.addEventListener('click', () => {
    navigationTarget = link.hash.slice(1); selectSection(navigationTarget);
  }));
  addEventListener('hashchange', () => {
    if (sections.some(section => section.id === location.hash.slice(1))) {
      navigationTarget = location.hash.slice(1); selectSection(navigationTarget);
    } else if (location.hash === '#main') { navigationTarget = null; selectSection(sections[0].id); }
  });
  addEventListener('scrollend', () => { if (navigationTarget) { cancelAnimationFrame(navigationFrame); navigationFrame = 0; navigationTarget = null; } });
  const manualScroll = () => { navigationTarget = null; scheduleNavigation(); };
  addEventListener('wheel', manualScroll, { passive: true });
  addEventListener('touchmove', manualScroll, { passive: true });
  addEventListener('pointerdown', event => { if (!event.target.closest('.sidebar nav')) navigationTarget = null; });
  addEventListener('keydown', event => {
    if (['ArrowUp', 'ArrowDown', 'PageUp', 'PageDown', 'Home', 'End', ' '].includes(event.key) && !event.target.closest('input,textarea,select')) manualScroll();
  });
  addEventListener('scroll', scheduleNavigation, { passive: true });
  addEventListener('resize', scheduleNavigation);
  selectSection(sections.some(section => section.id === location.hash.slice(1)) ? location.hash.slice(1) : sections[0].id);
  $('fallback').hidden = bridge || Boolean(window.showOpenFilePicker && window.showSaveFilePicker);
  populate(state.source); applyTheme(); translate();
  if (bridge) reloadBridge();
})();
