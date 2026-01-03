# dwin

A keyboard-driven window manager for macOS.

**dwin** virtualizes window management through *Buffers* — logical groupings of applications by PID. When Buffer N is active, only apps in that buffer are visible. All others are hidden.

Think of it as workspaces, but for apps instead of windows.

## Why

macOS doesn't have a simple, effective tiling WM like Hyprland or i3. dwin fills that gap for power users who want keyboard-driven workflow without the bloat.

## Features

- **Buffer-based workspaces**: X buffers, switch instantly with `Opt+[1-X]`
- **Dwindle tiling**: Hyprland-style recursive splitting layout
- **Window snapping**: Half-screen, quarter-screen, maximize, center
- **App rules**: Auto-assign apps to buffers by bundle ID
- **Pass-through mode**: Disable hotkeys for games/fullscreen apps
- **Menu bar indicator**: Visual feedback for active buffer

## Requirements

- macOS 15.0+
- Accessibility permissions (System Settings → Privacy & Security → Accessibility)

## Build

```bash
make            # Release build
make debug      # Debug build with symbols
make test       # Run tests
make codesign   # Self-sign binary for accessibility permissions
make install    # Install to /Applications
make format     # Format source files (requires clang-format)
make clean      # Remove build artifacts
```

## Configuration

Create `~/.config/.dwin`:

```bash
# Gaps
gaps_out = 12
gaps_in = 8

# Key bindings
bind = OPT+1, buffer_1
bind = OPT+2, buffer_2
bind = OPT+SHIFT+1, move_buffer_1
bind = OPT+h, snap_left
bind = OPT+l, snap_right
bind = OPT+SHIFT+p, passthrough

# App rules (auto-assign to buffer)
rule = com.jetbrains.CLion, 1
rule = net.kovidgoyal.kitty, 2
rule = company.thebrowser.Browser, 3

# Global Launcher (launch apps from global shortcuts)
bind = OPT+return, net.kovidgoyal.kitty
bind = OPT+s, com.tinyspeck.slackmacgap

```

## Usage

| Shortcut | Action |
|----------|--------|
| `Opt + [1-5]` | Switch to Buffer N |
| `Opt + Shift + [1-5]` | Move focused app to Buffer N |
| `Opt + H/L` | Snap left/right half |
| `Opt + Shift + P` | Toggle pass-through mode |

## How it works

1. On launch, dwin scans running apps and assigns them to buffers
2. Switching buffers: unhide new buffer apps → raise focus → hide old buffer apps
3. Layout engine tiles non-floating apps using dwindle algorithm
4. EventTap intercepts configured hotkeys globally

## License

MIT

