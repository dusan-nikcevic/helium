pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#1b1b1d"
    readonly property color panel: "#161618"
    readonly property color rail: "#121214"
    readonly property color well: "#0c0c0e"
    readonly property color strip: "#1c1c1f"
    readonly property color elevated: "#26262a"
    readonly property color ink: "#f5f5f7"
    readonly property color secondary: "#c5c5ca"
    readonly property color muted: "#86868b"
    readonly property color dim: "#6b6b70"
    readonly property color faint: "#56565b"
    readonly property color accent: "#2997ff"
    readonly property color success: "#54c98a"
    readonly property color warning: "#e8c24e"
    readonly property color danger: "#ff5a4d"
    readonly property color border: "#14ffffff"
    readonly property string fontFamily: "Inter"
    readonly property string monoFamily: "monospace"
    function alpha(value: color, opacity: real): color {
        return Qt.rgba(value.r, value.g, value.b, opacity)
    }
    function trackColor(key: string): color {
        return ({ drums: "#cf8163", bass: "#7e84c0", vocal: "#5aa9a0", vocaldbl: "#5d93b3",
            piano: "#bd9b62", pads: "#9b7cb8", guitars: "#8c9b66", fx: "#b66f80", master: "#9aa0a6" })[key] || key || "#9aa0a6"
    }
    function deviceColor(kind: string): color {
        return ({ eq: "#6f9fc8", dynamics: "#e0a955", saturation: "#d9774f", fx: "#9a7cb8",
            instrument: "#5aa9a0", utility: "#8a8f96" })[kind] || "#9a7cb8"
    }
}
