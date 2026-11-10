# Vivaldi++ logo

Use the user's supplied PNG unchanged as the source artwork. `logo-original.png` preserves the original bytes. `web/logo.png` is the 128px browser asset, proportionally resized with alpha preserved; only fully transparent outer margins may be cropped. Do not remove black pixels, recolor the design, redraw the mark, or add effects.

The `.brand-logo` container holds `img.logo-image` inside the existing brand link. Pointer movement tilts the image by at most four degrees. Keyboard focus provides a small single tilt, and clicking gives the image one short compression and release. The link's navigation remains intact. There is no automatic or continuous motion, sound, network request, or setting change. `prefers-reduced-motion` disables the interaction, including a change made while it is running.

Files: `web/logo.png`, `web/logo.css`, `web/logo.js`. Include the CSS and deferred script as local resources; use the same PNG for the favicon. `logo-preview.html` is a review sheet, not part of the product interface.
