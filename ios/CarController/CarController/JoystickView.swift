import SwiftUI

/// Virtual joystick for touch input.
///
/// - `position.x`  → turn   (-1 = full left,  +1 = full right)
/// - `position.y`  → forward (+1 = full fwd,  -1 = full reverse)
///
/// Use `axisLock` to constrain movement to a single axis:
/// - `.vertical`   — knob moves only up/down    (position.x is always 0)
/// - `.horizontal` — knob moves only left/right  (position.y is always 0)
/// - `.free`       — full 2-axis movement (default)
struct JoystickView: View {

    @Binding var position: CGPoint

    var axisLock: AxisLock = .free

    /// Physical diameter of the base circle in points.
    var baseSize: CGFloat = 200

    enum AxisLock { case vertical, horizontal, free }

    private var knobSize: CGFloat    { baseSize * 0.36 }   // scales proportionally
    private var deadzoneFrac: Double { 0.07 }
    private var maxRadius: CGFloat   { (baseSize - knobSize) / 2 }

    @State private var knobOffset: CGSize = .zero
    @State private var isDragging: Bool   = false

    // MARK: - Body

    var body: some View {
        VStack(spacing: 12) {
            joystickDial
            speedReadout
        }
    }

    // MARK: - Dial

    private var joystickDial: some View {
        ZStack {
            // ── Glass base ────────────────────────────────────────────────────
            Color.clear
                .frame(width: baseSize, height: baseSize)
                .glassEffect(.regular, in: Circle())

            // ── Crosshair — only draw the line(s) relevant to this axis ──────
            if axisLock != .horizontal {
                Rectangle()
                    .fill(Color.white.opacity(0.1))
                    .frame(width: 1, height: baseSize * 0.70)
            }
            if axisLock != .vertical {
                Rectangle()
                    .fill(Color.white.opacity(0.1))
                    .frame(width: baseSize * 0.70, height: 1)
            }

            // ── Direction labels — only for active directions ─────────────────
            if axisLock != .horizontal {
                label("F", dx: 0,                    dy: -(baseSize / 2 - 16))
                label("B", dx: 0,                    dy:   baseSize / 2 - 16)
            }
            if axisLock != .vertical {
                label("L", dx: -(baseSize / 2 - 16), dy: 0)
                label("R", dx:   baseSize / 2 - 16,  dy: 0)
            }

            // ── Knob ──────────────────────────────────────────────────────────
            Circle()
                .fill(
                    RadialGradient(
                        colors: [
                            Color(red: 0.48, green: 0.68, blue: 1.0),
                            Color(red: 0.16, green: 0.36, blue: 0.94)
                        ],
                        center: .topLeading,
                        startRadius: 2,
                        endRadius: knobSize
                    )
                )
                .frame(width: knobSize, height: knobSize)
                .shadow(
                    color: .blue.opacity(isDragging ? 0.8 : 0.4),
                    radius: isDragging ? 22 : 10
                )
                .offset(knobOffset)
        }
        .frame(width: baseSize, height: baseSize)
        .contentShape(Circle())
        // highPriorityGesture tells UIKit this gesture wins immediately,
        // avoiding the system gesture gate timeout near the home indicator.
        .highPriorityGesture(drag)
    }

    // MARK: - Speed Readout

    private var speedReadout: some View {
        HStack(spacing: 26) {
            if axisLock != .horizontal {
                speedLabel("FWD",  value: position.y)
            }
            if axisLock != .vertical {
                speedLabel("TURN", value: position.x)
            }
        }
    }

    // MARK: - Drag Gesture

    private var drag: some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { value in
                guard maxRadius > 0 else { return }   // guard against layout race
                isDragging = true

                let dx = value.translation.width
                let dy = value.translation.height

                // Clamp movement to the axis(es) this joystick is locked to
                let offsetX: CGFloat
                let offsetY: CGFloat

                switch axisLock {
                case .vertical:
                    offsetX = 0
                    offsetY = max(-maxRadius, min(maxRadius, dy))
                case .horizontal:
                    offsetX = max(-maxRadius, min(maxRadius, dx))
                    offsetY = 0
                case .free:
                    let dist = hypot(dx, dy)
                    if dist <= maxRadius {
                        offsetX = dx; offsetY = dy
                    } else {
                        offsetX = dx * maxRadius / dist
                        offsetY = dy * maxRadius / dist
                    }
                }

                knobOffset = CGSize(width: offsetX, height: offsetY)

                var nx = Double(offsetX /  maxRadius)
                var ny = Double(-offsetY / maxRadius)   // invert: up → positive

                if abs(nx) < deadzoneFrac { nx = 0 }
                if abs(ny) < deadzoneFrac { ny = 0 }

                position = CGPoint(x: nx, y: ny)
            }
            .onEnded { _ in
                isDragging = false
                withAnimation(.spring(response: 0.25, dampingFraction: 0.65)) {
                    knobOffset = .zero
                }
                position = .zero
            }
    }

    // MARK: - Helpers

    private func label(_ text: String, dx: CGFloat, dy: CGFloat) -> some View {
        Text(text)
            .font(.system(size: 12, weight: .bold, design: .monospaced))
            .foregroundStyle(Color.white.opacity(0.35))
            .offset(x: dx, y: dy)
    }

    private func speedLabel(_ name: String, value: Double) -> some View {
        VStack(spacing: 2) {
            Text(name)
                .font(.system(size: 10, weight: .semibold))
                .foregroundStyle(Color.white.opacity(0.4))
            Text(String(format: "%+.0f", value * 100))
                .font(.system(size: 18, weight: .bold, design: .monospaced))
                .foregroundStyle(
                    value != 0
                        ? Color(red: 0.4, green: 0.65, blue: 1.0)
                        : Color.white.opacity(0.35)
                )
                .contentTransition(.numericText())
                .animation(.easeInOut(duration: 0.1), value: value)
        }
    }
}
