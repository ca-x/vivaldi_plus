# Vivaldi++ settings design

## Research and direction

Applied ui-ux-pro-max: `search.py "browser settings productivity utility minimal red professional dark mode" --design-system` and `search.py "animation accessibility z-index loading" --domain ux` (2026-11-10). Adopt the utility-oriented precision, restrained decoration, single primary save action, visible async feedback, and WCAG AA constraints. Adapt the search's landing-page recommendation to an actual settings application: compact section navigation and labelled forms, no oversized marketing hero. Use the project's red accent rather than the generic generated blue palette.

## Tokens and reusable primitives

- System fonts: Segoe UI, Microsoft YaHei, system-ui; no remote fonts or assets.
- Light: canvas #f5f6f8, surface #ffffff, text #202126, secondary #62656e, border #d7d9df, accent #bf2638, soft-accent #fff0f2.
- Dark: canvas #17181c, surface #202126, text #f3f3f5, secondary #b4b5bd, border #45464f, accent #ff8794, soft-accent #3b242a. Primary button uses #bf2638 with white text in both themes.
- Type scale: 12/14/16/20/30 px; body line height 1.6; headings 600–700.
- Spacing: 4/8/12/16/24/32/48 px; radii 8/12/16 px.
- Primitives: button (primary/secondary), text field, labelled native switch checkbox, section, note, badge, nav item. All interactive targets at least 44 px high. Visible 3 px focus ring.
- Motion: color/background/border transitions 160 ms; remove transitions and smooth scrolling under reduced-motion.

## Layout

Desktop has a 240 px sidebar, 64 px header, and max 920 px reading column. Sidebar provides general/paths/hotkey/appearance/about anchor links. Configuration sections have compact numbered headings, helper text and consistently aligned controls. Below 800 px the sidebar becomes a horizontal navigation; below 540 px every row and action group wraps without horizontal scroll. Footer save controls remain in document flow so enlarged text and mobile keyboards never cover fields.

## Language and theme

Complete Simplified Chinese and English dictionaries cover static text, document title, status messages, confirmation dialogs, help and validation errors. Language defaults to browser language. Light/dark/system selection defaults to system; system follows changes through matchMedia. Both preferences persist independently of config.ini in localStorage; storage failures retain working session preferences. No external network requests.

## Data and interaction

The page does not claim DLL connectivity. It instructs the user to select App/config.ini. Native File System Access API reads/writes a user-selected file, checks external modification before overwrite and writes UTF-16LE BOM for Win32 profile API compatibility. Unsupported browsers use file input import and explicit download, never report downloaded data as installed. Save failures remain errors; cancellation is silent. Unsaved changes trigger browser navigation protection and confirmation before replacing/resetting edits. INI editing preserves unknown sections/keys/comments and only rewrites changed known values. Startup-only changes require a complete browser restart; the page never closes the user's browser.

## Accessibility and accepted constraints

Use semantic labels, fieldsets, headings, anchor navigation, aria-live status, native selects and checkboxes. Do not rely on color alone. Dynamic text wraps; keyboard navigation follows visual order. This is an offline standalone configuration editor, not an HTTP service or Chrome extension. File permissions are user-mediated; file handles are intentionally not persisted. A downloaded configuration must be manually placed next to version.dll. No automatic migration of existing browser data.
