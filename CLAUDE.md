# CLAUDE.md

This project keeps its full agent guidance in **`AGENTS.md`** (project overview, source layout,
build, dev best practices, and the testing / screenshot-review / lint tooling). It is imported
below so it loads automatically. Read it before making changes.

@AGENTS.md

## Quick reference

```sh
make                              # build (warning-clean with -Wall -Wextra)
./build/aim-trainer --self-test   # logic gate; must print "SELF TEST PASSED"
./build/aim-trainer --debug-menu /tmp/m.bmp 1920 1080 0 0   # render a menu tab for screenshot review
```

Before finishing a change: warning-free build, passing self-test, a self-test for new behavior,
and a screenshot review of any UI/render change. See `AGENTS.md` for details and the PIL
BMP-to-PNG conversion step (`sips` cannot read these BMPs).

## Windows stable/dev shortcuts

`Aim Trainer Dev` should track the latest verified dev build through `scripts/windows-run.ps1`.
`Aim Trainer` is the pinned stable shortcut and must only be updated when the user explicitly asks.
After completing and verifying Windows work, ask whether to promote the current dev build to stable;
do not run `.\scripts\windows-install-shortcut.ps1 -UpdateStable` unless the user says yes.
