import SwiftUI

struct JoystickView: View {
    @Binding var fwd:  Double   // –100 … +100
    @Binding var turn: Double   // –100 … +100

    private let baseSize:  CGFloat = 180
    private let knobSize:  CGFloat = 64
    private let maxRadius: CGFloat = 58

    @State private var offset: CGSize = .zero
    @GestureState private var dragging = false

    private let haptic = UIImpactFeedbackGenerator(style: .light)

    var body: some View {
        ZStack {
            // ── Base ──────────────────────────────────────────────────────────
            Circle()
                .fill(.ultraThinMaterial)
                .frame(width: baseSize, height: baseSize)
                .overlay {
                    Circle()
                        .strokeBorder(
                            LinearGradient(
                                colors: [.white.opacity(0.35), .white.opacity(0.08)],
                                startPoint: .topLeading, endPoint: .bottomTrailing
                            ),
                            lineWidth: 1.5
                        )
                }
                .shadow(color: .black.opacity(0.4), radius: 12, y: 6)

            // ── Cardinal labels ───────────────────────────────────────────────
            cardinalLabel("▲", offset: CGSize(width: 0, height: -(baseSize / 2 - 14)))
            cardinalLabel("▼", offset: CGSize(width: 0, height:  (baseSize / 2 - 14)))
            cardinalLabel("◀", offset: CGSize(width: -(baseSize / 2 - 14), height: 0))
            cardinalLabel("▶", offset: CGSize(width:  (baseSize / 2 - 14), height: 0))

            // ── Cross lines ───────────────────────────────────────────────────
            Path { p in
                p.move(to: CGPoint(x: baseSize/2, y: 12))
                p.addLine(to: CGPoint(x: baseSize/2, y: baseSize - 12))
            }
            .stroke(.white.opacity(0.12), lineWidth: 1)
            .frame(width: baseSize, height: baseSize)

            Path { p in
                p.move(to: CGPoint(x: 12, y: baseSize/2))
                p.addLine(to: CGPoint(x: baseSize - 12, y: baseSize/2))
            }
            .stroke(.white.opacity(0.12), lineWidth: 1)
            .frame(width: baseSize, height: baseSize)

            // ── Knob ──────────────────────────────────────────────────────────
            knob
                .shadow(color: Color(red: 0.3, green: 0.6, blue: 1.0).opacity(0.7),
                        radius: dragging ? 18 : 10, y: 0)
                .offset(offset)
                .scaleEffect(dragging ? 1.08 : 1.0)
                .animation(.spring(response: 0.25, dampingFraction: 0.7), value: dragging)
        }
        .frame(width: baseSize, height: baseSize)
        .gesture(
            DragGesture(minimumDistance: 0)
                .updating($dragging) { _, state, _ in state = true }
                .onChanged { g in
                    let dx = g.translation.width
                    let dy = g.translation.height
                    let dist = sqrt(dx * dx + dy * dy)
                    let clamped = min(dist, maxRadius)
                    let angle = atan2(dy, dx)
                    let cx = clamped * cos(angle)
                    let cy = clamped * sin(angle)
                    offset = CGSize(width: cx, height: cy)
                    turn =  Double(cx / maxRadius) * 100
                    fwd  = -Double(cy / maxRadius) * 100
                    if abs(dist - maxRadius) < 4 { haptic.impactOccurred(intensity: 0.4) }
                }
                .onEnded { _ in
                    withAnimation(.spring(response: 0.35, dampingFraction: 0.6)) {
                        offset = .zero
                    }
                    fwd  = 0
                    turn = 0
                    haptic.impactOccurred(intensity: 0.3)
                }
        )
    }

    // ── Knob: Liquid Glass on iOS 26+, gradient fallback below ───────────────

    @ViewBuilder
    private var knob: some View {
        if #available(iOS 26, *) {
            Circle()
                .frame(width: knobSize, height: knobSize)
                .glassEffect(
                    .regular.tint(Color(red: 0.3, green: 0.6, blue: 1.0)),
                    in: Circle()
                )
        } else {
            Circle()
                .fill(
                    RadialGradient(
                        colors: [
                            Color(red: 0.45, green: 0.72, blue: 1.0).opacity(0.95),
                            Color(red: 0.18, green: 0.45, blue: 0.95).opacity(0.85)
                        ],
                        center: .init(x: 0.38, y: 0.30),
                        startRadius: 2, endRadius: knobSize * 0.6
                    )
                )
                .frame(width: knobSize, height: knobSize)
                .overlay { Circle().strokeBorder(.white.opacity(0.4), lineWidth: 1) }
        }
    }

    private func cardinalLabel(_ text: String, offset: CGSize) -> some View {
        Text(text)
            .font(.system(size: 11, weight: .semibold))
            .foregroundStyle(.white.opacity(0.35))
            .offset(offset)
    }
}
