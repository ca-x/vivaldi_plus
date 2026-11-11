# Startup reliability and lifecycle actions

Implement the approved features independently from the current MIT codebase; do not copy upstream GPL implementation.

1. Replace portable relaunch via ShellExecuteEx with CreateProcessW. Preserve exact parsed arguments, quoting and explicit profile/cache precedence; set the child working directory to the browser executable directory, disable handle inheritance and close returned handles.
2. Add `[general] lifecycle_actions=0`, `launch_on_startup=` and `launch_on_exit=`. Missing keys keep the feature off. Each action is one Windows executable command line, with quoted paths, `%app%` and environment variables. Shell scripts require an explicit interpreter such as `cmd.exe /d /c` or `powershell.exe -File`.
3. Register real browser owners only after their Chromium message-only singleton window appears. Coordinate by installation in memory: first valid instance runs startup, final normal exit runs exit. Forwarding processes do not execute actions. Serialize restart so the old exit action's bounded wait precedes the new startup action. Wait at most three seconds for an exit command; do not kill the command. Crashes/forced termination do not guarantee exit actions.
4. Add a concise WebUI section with a default-off switch and startup/exit command inputs. Retain Chinese/English, themes, logo, navigation selection, INI comment preservation and opt-in WebUI behavior.
5. Verify native launch round trips, default-off behavior, single/multiple owners, URL forwarding, restart ordering, timeout, dead-process cleanup, command quoting and UI save/reload. Use isolated Chrome and Vivaldi profiles, never real user data or destructive example commands.
6. Run three-architecture Actions builds and independent reviews. Merge only passing changes, release a new stable version with six browser/architecture assets, and delete this task branch afterward.

No new dependencies, installer scripts, persistent coordination files or direct upstream code copies.
