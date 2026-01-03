# Contributing to dwin

## Ground rules

1. **Keep it simple** — dwin is intentionally minimal. Don't add complexity.
2. **No bloat** — if a feature can be done externally, it probably should be.
3. **Test your changes** — run it, use it, break it before submitting.

## How to contribute

### Bug reports

Open an issue with:
- macOS version
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs (`Console.app` → filter by "dwin")

### Code changes

1. Fork the repo
2. Create a branch: `git checkout -b fix/your-fix` or `feat/your-feature`
3. Make your changes
4. Test thoroughly on your machine
5. Submit a PR with a clear description

### Commit style

Keep commits atomic and descriptive:

```
fix: prevent focus falling to Finder on empty buffer
feat: add snap_center action
refactor: extract AX helpers to ax_utils.m
```

## Code style

- **Language**: Objective-C for AppKit/lifecycle, C for data structures and logic
- **Formatting**: Follow existing style (spaces, braces on same line)
- **Memory**: Release every `CFRef` you create. Wrap callbacks in `@autoreleasepool`.
- **Logging**: Use `NSLog` with `[Category]` prefix (e.g., `[Layout]`, `[Buffer]`)

## What we're NOT looking for

- Window borders/decorations (conflicts with macOS design)
- Complex scripting engines
- Plugin systems
- Electron/web-based anything

## Questions?

Open an issue. Keep it short.

