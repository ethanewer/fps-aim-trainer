# CLAUDE.md

This project keeps its full agent guidance in **`AGENTS.md`** (project overview, source layout,
build, dev best practices, and the testing / screenshot-review / lint tooling). It is imported
below so it loads automatically — read it before making changes.

@AGENTS.md

## Quick reference

```sh
make                              # build (warning-clean with -Wall -Wextra)
./build/aim-trainer --self-test   # logic gate — must print "SELF TEST PASSED"
./build/aim-trainer --debug-menu /tmp/m.bmp 1920 1080 0 0   # render a menu tab for screenshot review
```

Before finishing a change: warning-free build, passing self-test, a self-test for new behavior,
and a screenshot review of any UI/render change. See `AGENTS.md` for details and the PIL
BMP→PNG conversion step (`sips` can't read these BMPs).
