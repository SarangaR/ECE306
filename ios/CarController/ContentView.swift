import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – ActionButton
// ─────────────────────────────────────────────────────────────────────────────

private struct ActionButton: View {
    let label:  String
    let icon:   String
    let color:  Color
    let action: () -> Void

    private let haptic = UIImpactFeedbackGenerator(style: .medium)

    var body: some View {
        if #available(iOS 26, *) {
            Button {
                haptic.impactOccurred()
                action()
            } label: {
                Label(label, systemImage: icon)
                    .font(.system(size: 15, weight: .semibold))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
            }
            .buttonStyle(.glass)
            .tint(color)
        } else {
            Button {
                haptic.impactOccurred()
                action()
            } label: {
                HStack(spacing: 10) {
                    Image(systemName: icon)
                        .font(.system(size: 15, weight: .semibold))
                    Text(label)
                        .font(.system(size: 15, weight: .semibold))
                }
                .foregroundStyle(.white)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 14)
                .background {
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(color.opacity(0.28))
                        .overlay {
                            RoundedRectangle(cornerRadius: 14, style: .continuous)
                                .strokeBorder(
                                    LinearGradient(
                                        colors: [color.opacity(0.6), color.opacity(0.15)],
                                        startPoint: .topLeading, endPoint: .bottomTrailing
                                    ),
                                    lineWidth: 1
                                )
                        }
                }
            }
            .buttonStyle(ScaleButtonStyle())
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – ScaleButtonStyle (fallback only)
// ─────────────────────────────────────────────────────────────────────────────

private struct ScaleButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.94 : 1.0)
            .animation(.spring(response: 0.2, dampingFraction: 0.65), value: configuration.isPressed)
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – GlassPill
// ─────────────────────────────────────────────────────────────────────────────

private struct GlassPill: View {
    let text:  String
    let color: Color

    var body: some View {
        if #available(iOS 26, *) {
            Text(text)
                .font(.system(size: 12, weight: .bold, design: .rounded))
                .tracking(1.2)
                .foregroundStyle(color)
                .padding(.horizontal, 12)
                .padding(.vertical, 5)
                .glassEffect(.regular.tint(color), in: Capsule())
        } else {
            Text(text)
                .font(.system(size: 12, weight: .bold, design: .rounded))
                .tracking(1.2)
                .foregroundStyle(color)
                .padding(.horizontal, 12)
                .padding(.vertical, 5)
                .background {
                    Capsule()
                        .fill(.ultraThinMaterial)
                        .overlay { Capsule().strokeBorder(color.opacity(0.45), lineWidth: 1) }
                }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – ContentView
// ─────────────────────────────────────────────────────────────────────────────

struct ContentView: View {

    // MARK: State
    @State private var conn    = CarConnection()
    @State private var gamepad = GamepadManager()

    @State private var jFwd:  Double = 0
    @State private var jTurn: Double = 0

    @State private var ipField        = UserDefaults.standard.string(forKey: "carIP") ?? ""
    @State private var showConnectPanel = true
    @State private var padNumber: Int = 1

    @State private var speedTimer: Timer?

    // MARK: Constants
    private let padHaptic   = UIImpactFeedbackGenerator(style: .rigid)
    private let notifHaptic = UINotificationFeedbackGenerator()
    private let bgBase      = Color(red: 0.04, green: 0.04, blue: 0.14)

    @Namespace private var pillNS

    // MARK: Body
    var body: some View {
        ZStack {
            backgroundView

            VStack(spacing: 0) {
                headerBar
                    .padding(.horizontal, 20)
                    .padding(.top, 8)

                GeometryReader { geo in
                    HStack(alignment: .top, spacing: 16) {
                        joystickColumn
                            .frame(width: geo.size.width * 0.42)
                        actionColumn
                            .frame(maxWidth: .infinity)
                    }
                    .padding(.horizontal, 16)
                    .padding(.top, 16)
                }

                connectPanel
                    .padding(.horizontal, 16)
                    .padding(.bottom, 16)
            }
        }
        .onAppear(perform: setupGamepad)
        .onAppear(perform: startSpeedTimer)
        .onDisappear(perform: stopSpeedTimer)
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MARK: Sub-views
    // ─────────────────────────────────────────────────────────────────────────

    private var backgroundView: some View {
        ZStack {
            Color(bgBase).ignoresSafeArea()
            Circle()
                .fill(RadialGradient(
                    colors: [Color(red: 0.15, green: 0.25, blue: 0.80).opacity(0.35), .clear],
                    center: .center, startRadius: 0, endRadius: 260))
                .frame(width: 500, height: 500)
                .offset(x: -140, y: -240)
                .blur(radius: 40)
            Circle()
                .fill(RadialGradient(
                    colors: [Color(red: 0.55, green: 0.15, blue: 0.80).opacity(0.28), .clear],
                    center: .center, startRadius: 0, endRadius: 200))
                .frame(width: 400, height: 400)
                .offset(x: 160, y: 120)
                .blur(radius: 40)
            Circle()
                .fill(RadialGradient(
                    colors: [Color(red: 0.10, green: 0.50, blue: 0.90).opacity(0.20), .clear],
                    center: .center, startRadius: 0, endRadius: 180))
                .frame(width: 360, height: 360)
                .offset(x: 60, y: 340)
                .blur(radius: 50)
        }
    }

    private var headerBar: some View {
        HStack(spacing: 10) {
            Text("CarController")
                .font(.system(size: 20, weight: .bold, design: .rounded))
                .foregroundStyle(.white)

            Spacer()

            // Pill group — use GlassEffectContainer on iOS 26 so pills morph
            // smoothly when gamepad connects / disconnects.
            if #available(iOS 26, *) {
                GlassEffectContainer(spacing: 16) {
                    HStack(spacing: 10) {
                        if gamepad.isConnected {
                            GlassPill(
                                text: "🎮 \(gamepad.connectedName ?? "Gamepad")",
                                color: Color(red: 0.4, green: 0.9, blue: 0.55)
                            )
                            .glassEffectID("gamepad-pill", in: pillNS)
                        }
                        GlassPill(
                            text: conn.state.label,
                            color: statusColor(conn.state)
                        )
                        .glassEffectID("conn-pill", in: pillNS)
                    }
                }
            } else {
                HStack(spacing: 10) {
                    if gamepad.isConnected {
                        GlassPill(
                            text: "🎮 \(gamepad.connectedName ?? "Gamepad")",
                            color: Color(red: 0.4, green: 0.9, blue: 0.55)
                        )
                    }
                    GlassPill(text: conn.state.label, color: statusColor(conn.state))
                }
            }
        }
        .padding(.vertical, 10)
    }

    private var joystickColumn: some View {
        VStack(spacing: 20) {
            Text("DRIVE")
                .font(.system(size: 11, weight: .bold, design: .rounded))
                .tracking(2)
                .foregroundStyle(.white.opacity(0.45))

            JoystickView(fwd: $jFwd, turn: $jTurn)

            HStack(spacing: 4) {
                Image(systemName: "arrow.up.arrow.down").font(.caption2)
                Text(String(format: "%.0f", jFwd)).monospacedDigit()
                Spacer()
                Image(systemName: "arrow.left.arrow.right").font(.caption2)
                Text(String(format: "%.0f", jTurn)).monospacedDigit()
            }
            .font(.system(size: 12, weight: .medium))
            .foregroundStyle(.white.opacity(0.45))
            .padding(.horizontal, 4)

            Spacer()
        }
    }

    private var actionColumn: some View {
        ScrollView(showsIndicators: false) {
            VStack(spacing: 10) {
                Text("COMMANDS")
                    .font(.system(size: 11, weight: .bold, design: .rounded))
                    .tracking(2)
                    .foregroundStyle(.white.opacity(0.45))
                    .frame(maxWidth: .infinity, alignment: .leading)

                // Wrap all glass buttons in a container on iOS 26+
                if #available(iOS 26, *) {
                    GlassEffectContainer(spacing: 12) {
                        commandButtons
                    }
                } else {
                    commandButtons
                }

                padCounterRow

                Spacer(minLength: 8)
            }
            .padding(.top, 2)
            .padding(.bottom, 12)
        }
    }

    private var commandButtons: some View {
        VStack(spacing: 10) {
            ActionButton(label: "Stop",         icon: "stop.fill",
                         color: Color(red: 1, green: 0.28, blue: 0.28))  { conn.sendCmd("S") }
            ActionButton(label: "Line Follow",  icon: "road.lanes",
                         color: Color(red: 0.28, green: 0.72, blue: 1))  { conn.sendCmd("P") }
            ActionButton(label: "L-Route",      icon: "arrow.turn.up.right",
                         color: Color(red: 0.55, green: 0.90, blue: 0.40)) { conn.sendCmd("T") }
            ActionButton(label: "Enter Circle", icon: "circle.dotted",
                         color: Color(red: 1, green: 0.75, blue: 0.15))  { conn.sendCmd("E") }
            ActionButton(label: "Exit Circle",  icon: "arrow.right.circle",
                         color: Color(red: 1, green: 0.50, blue: 0.10))  { conn.sendCmd("X") }
            ActionButton(label: "Zero OTOS",    icon: "scope",
                         color: Color(red: 0.60, green: 0.60, blue: 0.65)) { conn.sendCmd("Z") }
        }
    }

    private var padCounterRow: some View {
        VStack(spacing: 8) {
            Divider().background(.white.opacity(0.15))

            Text("PAD ARRIVAL")
                .font(.system(size: 11, weight: .bold, design: .rounded))
                .tracking(2)
                .foregroundStyle(.white.opacity(0.45))
                .frame(maxWidth: .infinity, alignment: .leading)

            HStack(spacing: 10) {
                padStepButton(icon: "minus", enabled: padNumber > 1) {
                    padNumber -= 1
                    padHaptic.impactOccurred()
                }

                Text("\(padNumber)")
                    .font(.system(size: 28, weight: .bold, design: .rounded))
                    .foregroundStyle(.white)
                    .monospacedDigit()
                    .frame(minWidth: 40)

                padStepButton(icon: "plus", enabled: padNumber < 8) {
                    padNumber += 1
                    padHaptic.impactOccurred()
                }

                Spacer()

                sendPadButton
            }
        }
    }

    @ViewBuilder
    private func padStepButton(icon: String, enabled: Bool, action: @escaping () -> Void) -> some View {
        if #available(iOS 26, *) {
            Button(action: action) {
                Image(systemName: icon)
                    .font(.system(size: 16, weight: .bold))
                    .frame(width: 40, height: 40)
            }
            .buttonStyle(.glass)
            .disabled(!enabled)
        } else {
            Button(action: action) {
                Image(systemName: icon)
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(.white)
                    .frame(width: 40, height: 40)
                    .background(.ultraThinMaterial, in: Circle())
                    .overlay { Circle().strokeBorder(.white.opacity(0.2), lineWidth: 1) }
            }
            .buttonStyle(ScaleButtonStyle())
            .disabled(!enabled)
        }
    }

    @ViewBuilder
    private var sendPadButton: some View {
        if #available(iOS 26, *) {
            Button("SEND") { sendPadArrival() }
                .font(.system(size: 13, weight: .bold, design: .rounded))
                .tracking(1)
                .buttonStyle(.glassProminent)
                .tint(Color(red: 0.28, green: 0.60, blue: 1))
        } else {
            Button {
                sendPadArrival()
            } label: {
                Text("SEND")
                    .font(.system(size: 13, weight: .bold, design: .rounded))
                    .tracking(1)
                    .foregroundStyle(.white)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                    .background {
                        Capsule()
                            .fill(Color(red: 0.28, green: 0.60, blue: 1).opacity(0.35))
                            .overlay {
                                Capsule()
                                    .strokeBorder(
                                        Color(red: 0.28, green: 0.60, blue: 1).opacity(0.6),
                                        lineWidth: 1)
                            }
                    }
            }
            .buttonStyle(ScaleButtonStyle())
        }
    }

    private var connectPanel: some View {
        VStack(spacing: 12) {
            // Collapse / expand toggle
            Button {
                withAnimation(.spring(response: 0.3)) { showConnectPanel.toggle() }
            } label: {
                HStack {
                    Text("CONNECTION")
                        .font(.system(size: 11, weight: .bold, design: .rounded))
                        .tracking(2)
                        .foregroundStyle(.white.opacity(0.55))
                    Spacer()
                    Image(systemName: showConnectPanel ? "chevron.down" : "chevron.up")
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.45))
                }
            }

            if showConnectPanel {
                HStack(spacing: 10) {
                    ipTextField
                    connectButton
                }
            }
        }
        .padding(16)
        .modifier(ConnectPanelBackground())
    }

    @ViewBuilder
    private var ipTextField: some View {
        if #available(iOS 26, *) {
            TextField("Car IP Address", text: $ipField)
                .keyboardType(.decimalPad)
                .autocorrectionDisabled()
                .textInputAutocapitalization(.never)
                .foregroundStyle(.white)
                .padding(.horizontal, 14)
                .padding(.vertical, 11)
                .glassEffect(.regular, in: .rect(cornerRadius: 12))
        } else {
            TextField("Car IP Address", text: $ipField)
                .keyboardType(.decimalPad)
                .autocorrectionDisabled()
                .textInputAutocapitalization(.never)
                .foregroundStyle(.white)
                .padding(.horizontal, 14)
                .padding(.vertical, 11)
                .background {
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(.ultraThinMaterial)
                        .overlay {
                            RoundedRectangle(cornerRadius: 12, style: .continuous)
                                .strokeBorder(.white.opacity(0.2), lineWidth: 1)
                        }
                }
        }
    }

    @ViewBuilder
    private var connectButton: some View {
        if #available(iOS 26, *) {
            if conn.isConnected {
                Button("Disconnect") { conn.disconnect() }
                    .font(.system(size: 15, weight: .semibold))
                    .padding(.horizontal, 4)
                    .buttonStyle(.glass)
                    .tint(.red)
            } else {
                Button("Connect") { conn.connect(to: ipField) }
                    .font(.system(size: 15, weight: .semibold))
                    .padding(.horizontal, 4)
                    .buttonStyle(.glassProminent)
                    .tint(Color(red: 0.2, green: 0.6, blue: 1.0))
            }
        } else {
            Button {
                if conn.isConnected { conn.disconnect() } else { conn.connect(to: ipField) }
            } label: {
                Text(conn.isConnected ? "Disconnect" : "Connect")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(.white)
                    .padding(.horizontal, 18)
                    .padding(.vertical, 11)
                    .background {
                        RoundedRectangle(cornerRadius: 12, style: .continuous)
                            .fill(conn.isConnected
                                  ? Color(red: 0.9, green: 0.2, blue: 0.2).opacity(0.4)
                                  : Color(red: 0.2, green: 0.6, blue: 1.0).opacity(0.4))
                            .overlay {
                                RoundedRectangle(cornerRadius: 12, style: .continuous)
                                    .strokeBorder(.white.opacity(0.25), lineWidth: 1)
                            }
                    }
            }
            .buttonStyle(ScaleButtonStyle())
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MARK: Logic
    // ─────────────────────────────────────────────────────────────────────────

    private func setupGamepad() {
        gamepad.onStop        = { conn.sendCmd("S") }
        gamepad.onLineFollow  = { conn.sendCmd("P") }
        gamepad.onRoute       = { conn.sendCmd("T") }
        gamepad.onEnterCircle = { conn.sendCmd("E") }
        gamepad.onExitCircle  = { conn.sendCmd("X") }
        gamepad.onZeroOTOS    = { conn.sendCmd("Z") }

        gamepad.onPadUp = {
            guard padNumber < 8 else { return }
            padNumber += 1
            padHaptic.impactOccurred()
            sendPadArrival()
        }
        gamepad.onPadDown = {
            guard padNumber > 1 else { return }
            padNumber -= 1
            padHaptic.impactOccurred()
            sendPadArrival()
        }
    }

    private func sendPadArrival() {
        conn.sendCmd("N", value: "\(padNumber)")
        notifHaptic.notificationOccurred(.success)
    }

    private func startSpeedTimer() {
        speedTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 20.0, repeats: true) { _ in
            gamepad.poll()
            let f = jFwd  != 0 ? jFwd  : gamepad.analogFwd
            let t = jTurn != 0 ? jTurn : gamepad.analogTurn
            guard conn.isConnected else { return }
            conn.sendSpeed(fwd: f, turn: t)
        }
    }

    private func stopSpeedTimer() {
        speedTimer?.invalidate()
        speedTimer = nil
    }

    private func statusColor(_ s: ConnectionState) -> Color {
        switch s {
        case .connected:    return Color(red: 0.35, green: 0.90, blue: 0.50)
        case .connecting:   return Color(red: 1.0,  green: 0.78, blue: 0.15)
        case .failed:       return Color(red: 1.0,  green: 0.30, blue: 0.30)
        case .disconnected: return Color(red: 0.7,  green: 0.7,  blue: 0.7)
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – ConnectPanelBackground ViewModifier
// ─────────────────────────────────────────────────────────────────────────────

private struct ConnectPanelBackground: ViewModifier {
    func body(content: Content) -> some View {
        if #available(iOS 26, *) {
            content
                .glassEffect(.regular, in: .rect(cornerRadius: 20))
        } else {
            content
                .background {
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .fill(.ultraThinMaterial)
                        .overlay {
                            RoundedRectangle(cornerRadius: 20, style: .continuous)
                                .strokeBorder(.white.opacity(0.18), lineWidth: 1)
                        }
                }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MARK: – Preview
// ─────────────────────────────────────────────────────────────────────────────

#Preview {
    ContentView()
        .preferredColorScheme(.dark)
}
