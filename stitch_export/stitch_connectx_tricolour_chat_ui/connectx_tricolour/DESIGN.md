---
name: ConnectX Tricolour
colors:
  surface: '#f9f9f6'
  surface-dim: '#dadad7'
  surface-bright: '#f9f9f6'
  surface-container-lowest: '#ffffff'
  surface-container-low: '#f4f4f1'
  surface-container: '#eeeeeb'
  surface-container-high: '#e8e8e5'
  surface-container-highest: '#e2e3e0'
  on-surface: '#1a1c1b'
  on-surface-variant: '#46464d'
  inverse-surface: '#2f312f'
  inverse-on-surface: '#f1f1ee'
  outline: '#76767e'
  outline-variant: '#c6c6ce'
  surface-tint: '#575d78'
  primary: '#000000'
  on-primary: '#ffffff'
  primary-container: '#141a32'
  on-primary-container: '#7c839f'
  inverse-primary: '#bfc5e4'
  secondary: '#2d4fcf'
  on-secondary: '#ffffff'
  secondary-container: '#4b69ea'
  on-secondary-container: '#fffbff'
  tertiary: '#000000'
  on-tertiary: '#ffffff'
  tertiary-container: '#311300'
  on-tertiary-container: '#ce6700'
  error: '#ba1a1a'
  on-error: '#ffffff'
  error-container: '#ffdad6'
  on-error-container: '#93000a'
  primary-fixed: '#dce1ff'
  primary-fixed-dim: '#bfc5e4'
  on-primary-fixed: '#141a32'
  on-primary-fixed-variant: '#3f465f'
  secondary-fixed: '#dde1ff'
  secondary-fixed-dim: '#b8c3ff'
  on-secondary-fixed: '#001355'
  on-secondary-fixed-variant: '#0736ba'
  tertiary-fixed: '#ffdcc6'
  tertiary-fixed-dim: '#ffb786'
  on-tertiary-fixed: '#311300'
  on-tertiary-fixed-variant: '#723600'
  background: '#f9f9f6'
  on-background: '#1a1c1b'
  surface-variant: '#e2e3e0'
typography:
  display-lg:
    fontFamily: Inter
    fontSize: 36px
    fontWeight: '600'
    lineHeight: 44px
    letterSpacing: -0.025em
  display-lg-mobile:
    fontFamily: Inter
    fontSize: 28px
    fontWeight: '600'
    lineHeight: 36px
    letterSpacing: -0.02em
  headline-lg:
    fontFamily: Inter
    fontSize: 24px
    fontWeight: '600'
    lineHeight: 32px
    letterSpacing: -0.02em
  headline-lg-mobile:
    fontFamily: Inter
    fontSize: 20px
    fontWeight: '600'
    lineHeight: 28px
    letterSpacing: -0.015em
  headline-md:
    fontFamily: Inter
    fontSize: 18px
    fontWeight: '600'
    lineHeight: 26px
    letterSpacing: -0.015em
  headline-sm:
    fontFamily: Inter
    fontSize: 15px
    fontWeight: '600'
    lineHeight: 22px
    letterSpacing: -0.01em
  body-lg:
    fontFamily: Inter
    fontSize: 15px
    fontWeight: '400'
    lineHeight: 24px
    letterSpacing: -0.005em
  body-md:
    fontFamily: Inter
    fontSize: 13px
    fontWeight: '400'
    lineHeight: 20px
    letterSpacing: 0em
  body-sm:
    fontFamily: Inter
    fontSize: 12px
    fontWeight: '400'
    lineHeight: 18px
    letterSpacing: 0em
  label-md:
    fontFamily: Inter
    fontSize: 12px
    fontWeight: '500'
    lineHeight: 16px
    letterSpacing: 0.01em
  label-sm:
    fontFamily: Inter
    fontSize: 11px
    fontWeight: '600'
    lineHeight: 14px
    letterSpacing: 0.02em
  code-sm:
    fontFamily: JetBrains Mono
    fontSize: 12px
    fontWeight: '400'
    lineHeight: 18px
    letterSpacing: 0em
rounded:
  sm: 0.25rem
  DEFAULT: 0.5rem
  md: 0.75rem
  lg: 1rem
  xl: 1.5rem
  full: 9999px
spacing:
  gutter: 1rem
  gutter-mobile: 0.75rem
  margin: 1.5rem
  margin-mobile: 1rem
  space-xs: 0.25rem
  space-sm: 0.5rem
  space-md: 0.75rem
  space-lg: 1.25rem
  space-xl: 2rem
---

## Brand & Style

This design system translates the dignity, discipline, and kinetic balance of the Indian national flag into an ultra-refined, modern digital workspace. Engineered specifically for high-velocity peer communication and technical collaboration, the aesthetic balances high-precision developer-tool minimalism (reminiscent of Linear and modern macOS interfaces) with subtle patriotic symbolism.

The brand persona is rigorous, cerebral, and quiet. It rejects festive tropes, governmental bureaucracy, and saturated clutter. Instead, it relies on expansive architectural white space, quiet structural lines, deep navy typography reflecting the Ashoka Chakra, and surgical micro-accents of Saffron and India Green deployed purely for functional telemetry. The resulting interface feels calm, swift, and authoritative.

## Colors

The palette enforces a strict spatial distribution: 70% pure white (`#FFFFFF`) and warm alabaster neutrals (`#FAFAF7`, `#F4F5F2`), 20% deep navy ink (`#0A1128`, `#1E293B`) for crisp typographic legibility, and 10% intentional functional accents.

- **Canvas & Surfaces:** Primary application background uses `#FAFAF7`, while active messaging panes, contextual cards, and editor surfaces sit on `#FFFFFF`. Secondary panels (sidebars, inspection panes) rest on `#F4F5F2`.
- **Primary Navy:** `#0A1128` forms the bedrock for headings, high-contrast actions, and structural anchors, derived from the Ashoka Chakra.
- **Interactive Blue:** `#3B5BDB` acts as the focused link, cursor, and interactive selection indicator.
- **Saffron (`#F58220`):** Strictly reserved for non-destructive attention anchors—notification pings, pending execution flags, active thread highlights, and mentions.
- **India Green (`#138808`):** Reserved for system telemetry—online indicators, successful code builds, delivered status, and real-time socket connections.
- **Borders & Dividers:** Built using muted hairline tokens (`#E7E9E4` and `#DDE0DA`) to preserve structural clarity without visual weight.

## Typography

Typography centers on `Inter` for all structural narrative, paired with `JetBrains Mono` for code snippets, compilation outputs, and hash references. Tight negative letter-spacing is applied across medium and large headings to replicate modern technical communication suites.

Type hierarchy remains tight and restrained: contrast is created via subtle weight transitions (`400` to `600`) rather than drastic scale jumps. Body copy prioritizes optical reading density across dense multi-channel messaging threads.

## Layout & Spacing

The layout philosophy follows a rigid fluid grid with desktop three-pane column modularity: Navigation Sidebar (240px fixed), Conversation Thread (360px fixed or flex), and Active Workspace/Message Canvas (flexible fill). 

- **Desktop (>=1024px):** Three-tier layout with thin 1px `#E7E9E4` vertical borders acting as absolute structural separators. Content panes maintain internal container padding of `1.25rem` (`space-lg`).
- **Tablet (768px - 1023px):** Left navigation collapses to an icon rail (64px); the active canvas claims dynamic precedence with contextual slide-out panels.
- **Mobile (<768px):** Single-pane sequential navigation. Edge margins reduce to `1rem` (`margin-mobile`), and conversational streams utilize tight `0.5rem` (`space-sm`) vertical message grouping.

## Elevation & Depth

Visual hierarchy is primarily communicated through tonal contrast and razor-thin outlines rather than deep dropshadows:

- **Border Hierarchy:** Primary containment is defined by 1px solid borders (`#E7E9E4`). Floating elements (menus, command palettes) transition to a crisp border (`#DDE0DA`).
- **Ambient Elevation:** Modals and context menus employ a double-diffused Apple-style shadow tinted with primary navy: `0 4px 16px -2px rgba(10, 17, 40, 0.04), 0 12px 32px -4px rgba(10, 17, 40, 0.08)`.
- **Glassmorphic App Bars:** Thread header bars and pinned status strips utilize subtle frosted surfaces: `background: rgba(255, 255, 255, 0.85); backdrop-filter: blur(12px)`.

## Shapes

The interface embraces a measured, balanced curvature profile (roundedness level `2`), mapping components consistently across a 10px to 14px window:

- Small components (inputs, chips, action buttons): `8px` to `10px` border-radius.
- Cards, chat bubbles, and modal frames: `12px` to `14px` border-radius.
- Fully rounded pills are strictly limited to user status badges, live presence dots, and unread notification capsules.

## Components

- **Buttons:** 
  - *Primary:* Ashoka Chakra Navy (`#0A1128`) solid fill with white text, 10px radius, subtle hover lift to `#1E293B`.
  - *Secondary:* Pure white surface with 1px border (`#E7E9E4`), text in `#0A1128`, subtle fill shift to `#FAFAF7` on hover.
  - *Accent (Special Action):* `#3B5BDB` with crisp white label for primary compile/send executions.
- **Message Bubbles & Cards:**
  - *Peer Incoming:* Background `#F4F5F2`, text `#0A1128`, 12px radius, no border.
  - *Self Outgoing:* Background `#0A1128`, text `#FFFFFF`, 12px radius. Code blocks within self-bubbles take a deeper navy `#050914` background with syntax highlighting.
  - *Card Containers:* Background `#FFFFFF`, 1px solid `#E7E9E4`, 12px radius, zero shadow at rest; slight ambient elevation on hover.
- **Chips & Badges:**
  - Presence indicators use small 6px pill shapes: Green (`#138808`) for active sockets, Saffron (`#F58220`) for busy/debugging, and Muted Slate for offline.
  - Unread thread counter: Subtle Saffron tint background (`rgba(245, 130, 32, 0.12)`) with `#F58220` bold label text.
- **Input Fields & Chat Composer:**
  - Framed in 1px `#E7E9E4`, background `#FFFFFF`, transitioning to `#3B5BDB` border with a 2px `rgba(59, 91, 219, 0.12)` halo on focus. Integrated Lucide outline icons styled with `#8E95A5`.
- **Lists & Thread Selectors:**
  - Channel rows feature 8px inner corner radii on active selection, paired with an ultra-subtle vertical indicator line in `#3B5BDB` on the leading left edge.