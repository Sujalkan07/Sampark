/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        primary: "#0A1128",
        "primary-hover": "#1E293B",
        "primary-container": "#141A32",
        "on-primary": "#FFFFFF",
        saffron: "#F58220",
        "saffron-hover": "#E07519",
        "india-green": "#138808",
        secondary: "#2D4FCF",
        "secondary-hover": "#243FA8",
        background: "#FAFAF7",
        surface: "#FAFAF7",
        "surface-container-lowest": "#FFFFFF",
        "surface-container-low": "#F4F4F1",
        "surface-container": "#EEEEEB",
        "surface-container-high": "#E8E8E5",
        "surface-container-highest": "#E2E3E0",
        "on-surface": "#1A1C1B",
        "on-surface-variant": "#46464D",
        outline: "#76767E",
        "outline-variant": "#C6C6CE",
        "border-hairline": "#E7E9E4",
        error: "#BA1A1A",
        "error-container": "#FFDAD6",
      },
      fontFamily: {
        sans: ["Inter", "-apple-system", "BlinkMacSystemFont", "Segoe UI", "sans-serif"],
        mono: ["JetBrains Mono", "monospace"],
      },
      spacing: {
        "space-xs": "0.25rem",
        "space-sm": "0.5rem",
        "space-md": "0.75rem",
        "space-lg": "1.25rem",
        "space-xl": "2rem",
      },
    },
  },
  plugins: [],
}
