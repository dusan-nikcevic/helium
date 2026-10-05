.pragma library

function dbToNorm(db) {
    return Math.max(0, Math.min(1, (db + 60) / 66))
}

function normToDb(n) {
    return Math.round((Math.max(0, Math.min(1, n)) * 66 - 60) * 10) / 10
}

function formatDb(db) {
    if (db <= -60)
        return "−∞ dB"
    return (db < 0 ? "−" : "") + Math.abs(db).toFixed(1) + " dB"
}

function signed(n, decimals) {
    return (n < 0 ? "−" : n > 0 ? "+" : "") + Math.abs(n).toFixed(decimals)
}

function panLabel(pan) {
    if (Math.abs(pan) < 0.02)
        return "C"
    return (pan < 0 ? "L" : "R") + Math.round(Math.abs(pan) * 100)
}

function kindLabel(kind) {
    return ({ eq: "Equalizer", dynamics: "Dynamics", saturation: "Saturation",
              fx: "Effect", instrument: "Instrument", utility: "Utility" })[kind] || "Effect"
}

function trackTypeLabel(kind) {
    return ({ audio: "Audio", midi: "MIDI", instrument: "Instrument" })[kind] || "Audio"
}

function eqPath(params, w, h) {
    const g = [0.5, 0.5, 0.5]
    for (let i = 0; i < Math.min(3, params.length); i++)
        g[i] = params[i].value
    const centers = [0.16, 0.5, 0.86]
    const mid = h / 2
    let d = ""
    for (let i = 0; i <= 32; i++) {
        const x = i / 32
        let y = 0
        for (let b = 0; b < 3; b++) {
            const dx = (x - centers[b]) / 0.17
            y += (g[b] - 0.5) * 2 * Math.exp(-dx * dx)
        }
        const py = Math.max(3, Math.min(h - 3, mid - y * (h * 0.4)))
        d += (i ? " L " : "M ") + (x * w).toFixed(1) + " " + py.toFixed(1)
    }
    return d
}

function compPath(params, w, h) {
    const thr = params.length > 0 ? params[0].value : 0.5
    const ratio = 1 + (params.length > 1 ? params[1].value : 0.5) * 7
    const kx = 3 + (w - 6) * (0.25 + thr * 0.5)
    const ky = (h - 3) - (kx - 3) * (h - 6) / (w - 6)
    const ey = ky - (w - 3 - kx) * (h - 6) / (w - 6) / ratio
    return "M 3 " + (h - 3) + " L " + kx.toFixed(1) + " " + ky.toFixed(1)
         + " L " + (w - 3) + " " + Math.max(3, ey).toFixed(1)
}
