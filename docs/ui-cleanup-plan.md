# Settings UI cleanup

Scope: the settings files in `web/`, the user-provided logo and its interaction, and the single native about entry in `src/about_patch.cpp`. Keep the native configuration API, existing INI schema, language/theme persistence, validation, and save conflict handling intact.

Behavior lock: run the INI, native bridge, and about installer regressions before editing. Preserve keyboard focus, 44px controls, Chinese/English and light/dark/system behavior. The about regression must execute its native navigation handler.

1. Remove filler: slogan/intro, offline/account badge, duplicate local-config labels, section numbering, architecture descriptions, and footer promises. Keep concise field instructions and restart/error messages.
2. Put one entry inside System and Build and localize it with Vivaldi's translation function. Keep all page assets and the server embedded in the DLL. Remove the superseded script-based installer. Write only when needed, skip unsupported/unwritable files, and do not create backups or installation records.
3. Delete orphaned translations/selectors and consolidate duplicate checkbox rules. Format CSS into readable rules; retain semantic theme tokens and responsive behavior. Do not add dependencies or a new component layer.
4. Run focused regressions, real browser language/theme/viewport checks, then verify the actual Vivaldi about-card click path. Rebuild the embedded DLL and provide the new artifact.
