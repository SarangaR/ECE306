import SwiftUI

// MARK: - Root View

struct ContentView: View {

    @State private var vm = CarControllerViewModel()
    @Environment(\.scenePhase) private var scenePhase

    private enum Tab: Int { case drive = 0, controls = 1 }
    @State private var selectedTab: Tab = .drive

    private enum Field { case ip, port }
    @FocusState private var focusedField: Field?

    // 28 pt keeps every glass edge well clear of the screen bezel.
    private let hPad: CGFloat = 28

    var body: some View {
        ZStack(alignment: .bottom) {

            backgroundCanvas

            VStack(spacing: 0) {

                headerBar
                    .padding(.vertical, 6)
                    .padding(.horizontal, hPad)

                tabSwitcher
                    .padding(.bottom, 10)
                    .padding(.horizontal, hPad)

                // TabView with .page style manages its own horizontal extent;
                // each tab applies its own hPad internally.
                TabView(selection: $selectedTab) {
                    driveTab.tag(Tab.drive)
                    controlsTab.tag(Tab.controls)
                }
                .tabViewStyle(.page(indexDisplayMode: .never))
                .frame(maxHeight: .infinity)

                connectionBar
                    .padding(.bottom, 8)
                    .padding(.horizontal, hPad)
            }

            if vm.toastVisible {
                toastBadge
                    .padding(.bottom, 100)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                    .zIndex(1)
            }
        }
        .onChange(of: scenePhase) { _, phase in
            if phase != .active { vm.stop() }
        }
    }

    // MARK: - Background ──────────────────────────────────────────────────────

    private var backgroundCanvas: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Circle()
                .fill(Color.blue.opacity(0.28))
                .frame(width: 480).blur(radius: 130)
                .offset(x: -210, y: -270)
            Circle()
                .fill(Color.indigo.opacity(0.22))
                .frame(width: 400).blur(radius: 110)
                .offset(x: 210, y: 40)
            Circle()
                .fill(Color(red: 0.0, green: 0.72, blue: 0.55).opacity(0.15))
                .frame(width: 340).blur(radius: 95)
                .offset(x: -80, y: 330)
        }
        .ignoresSafeArea()
        .onTapGesture { focusedField = nil }
    }

    // MARK: - Header ──────────────────────────────────────────────────────────
    // ZStack guarantees the title is always exactly centred regardless of
    // whether the left pill (gamepad name) is wider than the right pill.

    private var headerBar: some View {
        ZStack {
            Text("💥  Car Controller")
                .font(.system(size: 14, weight: .bold))
                .foregroundStyle(.white)

            HStack {
                StatusPill(
                    icon: "gamecontroller.fill",
                    label: vm.gamepad.connectedGamepadName ?? "No Gamepad",
                    active: vm.gamepad.connectedGamepadName != nil,
                    color: .green
                )
                Spacer()
                StatusPill(
                    icon: vm.car.isConnected ? "wifi" : "wifi.slash",
                    label: vm.car.statusText,
                    active: vm.car.isConnected,
                    color: .green
                )
            }
        }
    }

    // MARK: - Tab Switcher ────────────────────────────────────────────────────

    private var tabSwitcher: some View {
        HStack(spacing: 0) {
            tabPill(label: "Drive",    icon: "gamecontroller.fill",  tab: .drive)
            tabPill(label: "Controls", icon: "slider.horizontal.3",  tab: .controls)
        }
        .padding(3)
        .glassEffect(.regular, in: Capsule())
    }

    private func tabPill(label: String, icon: String, tab: Tab) -> some View {
        let active = selectedTab == tab
        return HStack(spacing: 6) {
            Image(systemName: icon)
                .font(.system(size: 12, weight: .semibold))
            Text(label)
                .font(.system(size: 13, weight: .semibold))
        }
        .foregroundStyle(active ? Color.white : Color.white.opacity(0.4))
        .frame(maxWidth: .infinity)
        .padding(.vertical, 9)
        .background {
            if active { Capsule().fill(Color.white.opacity(0.16)) }
        }
        // .contentShape makes the whole padded area hittable (not just the text).
        // .highPriorityGesture beats the TabView page-swipe recognizer so taps
        // on the pills are never stolen by the scroll gesture underneath.
        .contentShape(Rectangle())
        .highPriorityGesture(
            TapGesture().onEnded {
                withAnimation(.easeInOut(duration: 0.22)) { selectedTab = tab }
            }
        )
        .animation(.easeInOut(duration: 0.22), value: selectedTab)
    }

    // MARK: - Drive Tab ───────────────────────────────────────────────────────

    private var driveTab: some View {
        VStack(spacing: 8) {
            joystickModeToggle

            GeometryReader { geo in
                let w = geo.size.width
                let h = geo.size.height

                if vm.dualJoystickMode {
                    // Reserve ~90 pt height for label + readout chrome.
                    // Give each stick 16 pt of breathing room per side.
                    let size = min(w / 2 - 16, h - 90)
                    HStack(spacing: 0) {
                        stickCell(position: $vm.leftJoystickPosition,
                                  lock: .vertical,   label: "FWD",  size: size)
                        stickCell(position: $vm.rightJoystickPosition,
                                  lock: .horizontal, label: "TURN", size: size)
                    }
                    .frame(width: w, height: h)
                } else {
                    // Single stick: 24 pt margin each side, cap at 340 pt.
                    let size = min(w - 48, min(h - 90, 340))
                    VStack {
                        JoystickView(position: $vm.leftJoystickPosition,
                                     axisLock: .free, baseSize: size)
                    }
                    .frame(width: w, height: h)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .padding(.horizontal, hPad)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private func stickCell(
        position: Binding<CGPoint>,
        lock: JoystickView.AxisLock,
        label: String,
        size: CGFloat
    ) -> some View {
        VStack(spacing: 6) {
            Text(label)
                .font(.system(size: 10, weight: .semibold))
                .foregroundStyle(Color.white.opacity(0.35))
                .tracking(1.5)
            JoystickView(position: position, axisLock: lock, baseSize: size)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
    }

    // MARK: - Controls Tab ────────────────────────────────────────────────────

    private var controlsTab: some View {
        ScrollView {
            VStack(spacing: 10) {

                CarButton(label: "Stop", icon: "stop.fill",
                          color: .red, enabled: vm.car.isConnected) { vm.stop() }

                HStack(spacing: 10) {
                    CarButton(label: "Line Follow", icon: "line.diagonal",
                              color: .green, enabled: vm.car.isConnected) { vm.sendLineFollow() }
                    CarButton(label: "L-Route", icon: "arrow.turn.right.down",
                              color: .purple, enabled: vm.car.isConnected) { vm.sendLRoute() }
                }

                HStack(spacing: 10) {
                    CarButton(label: "Enter Circle", icon: "arrow.right.circle",
                              color: .orange, enabled: vm.car.isConnected) { vm.sendEnterCircle() }
                    CarButton(label: "Exit Circle", icon: "arrow.left.circle",
                              color: Color(red: 0.9, green: 0.78, blue: 0.1),
                              enabled: vm.car.isConnected) { vm.sendExitCircle() }
                }

                CarButton(label: "Zero Heading & Position",
                          icon: "arrow.counterclockwise.circle",
                          color: Color(white: 0.75), enabled: vm.car.isConnected) {
                    vm.car.sendZeroOTOS()
                }

                padArrivalRow

                joystickModeToggle
                    .padding(.top, 4)
            }
            .padding(.vertical, 4)
            .padding(.horizontal, hPad)
        }
        .scrollIndicators(.hidden)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: - Joystick Mode Toggle ────────────────────────────────────────────

    private var joystickModeToggle: some View {
        Button { vm.toggleJoystickMode() } label: {
            HStack(spacing: 6) {
                Image(systemName: vm.dualJoystickMode ? "circle.grid.2x1.fill" : "circle.fill")
                    .font(.system(size: 11, weight: .semibold))
                Text(vm.dualJoystickMode ? "Dual Stick" : "Single Stick")
                    .font(.system(size: 11, weight: .semibold))
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 7)
        }
        .buttonStyle(.glass)
        .tint(vm.dualJoystickMode ? .blue : .indigo)
        .animation(.easeInOut(duration: 0.2), value: vm.dualJoystickMode)
    }

    // MARK: - Pad Arrival Row ─────────────────────────────────────────────────
    // No send button — increment/decrement immediately transmits the pad number.

    private var padArrivalRow: some View {
        HStack(spacing: 12) {
            Text("Pad Arrival")
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(Color.white.opacity(0.65))
                .fixedSize()

            Spacer(minLength: 0)

            Button {
                if vm.padNumber > 1 { vm.padNumber -= 1; vm.sendPad() }
            } label: {
                Image(systemName: "minus")
                    .font(.system(size: 14, weight: .bold))
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.glass)
            .disabled(!vm.car.isConnected)

            Text("Pad \(vm.padNumber)")
                .font(.system(size: 15, weight: .bold, design: .monospaced))
                .foregroundStyle(.white)
                .frame(minWidth: 52)
                .contentTransition(.numericText())
                .animation(.easeInOut(duration: 0.15), value: vm.padNumber)

            Button {
                if vm.padNumber < 8 { vm.padNumber += 1; vm.sendPad() }
            } label: {
                Image(systemName: "plus")
                    .font(.system(size: 14, weight: .bold))
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.glass)
            .disabled(!vm.car.isConnected)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .glassEffect(.regular, in: RoundedRectangle(cornerRadius: 14))
    }

    // MARK: - Connection Bar ──────────────────────────────────────────────────

    private var connectionBar: some View {
        HStack(spacing: 10) {

            HStack(spacing: 7) {
                Image(systemName: "network")
                    .foregroundStyle(Color.white.opacity(0.5))
                    .font(.system(size: 13))
                    .frame(width: 18)
                TextField("192.168.x.x", text: $vm.ipAddress)
                    .foregroundStyle(.white)
                    .keyboardType(.decimalPad)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    .focused($focusedField, equals: .ip)
                    .onSubmit { focusedField = nil }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .glassEffect(.regular, in: RoundedRectangle(cornerRadius: 10))

            TextField("Port", text: $vm.portText)
                .foregroundStyle(.white)
                .keyboardType(.numberPad)
                .multilineTextAlignment(.center)
                .frame(width: 56)
                .focused($focusedField, equals: .port)
                .padding(.horizontal, 10)
                .padding(.vertical, 10)
                .glassEffect(.regular, in: RoundedRectangle(cornerRadius: 10))

            Button {
                if vm.car.isConnected      { vm.disconnect() }
                else if vm.isRetrying      { vm.cancelRetries() }
                else                       { vm.startAutoConnect() }
            } label: {
                HStack(spacing: 7) {
                    if case .connecting = vm.car.state {
                        ProgressView().tint(.white).scaleEffect(0.75).frame(width: 14, height: 14)
                    } else if vm.isRetrying && !vm.car.isConnected {
                        ProgressView().tint(.white).scaleEffect(0.75).frame(width: 14, height: 14)
                    }
                    Text(connectButtonLabel)
                        .font(.system(size: 14, weight: .semibold))
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 10)
            }
            .tint(connectButtonTint)
            .buttonStyle(.glass)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 10)
        .glassEffect(.regular, in: RoundedRectangle(cornerRadius: 16))
        .toolbar {
            ToolbarItemGroup(placement: .keyboard) {
                Spacer()
                Button("Done") { focusedField = nil }.fontWeight(.semibold)
            }
        }
    }

    private var connectButtonLabel: String {
        if vm.car.isConnected { return "Disconnect" }
        if vm.isRetrying      { return "Stop Retrying" }
        return "Connect"
    }

    private var connectButtonTint: Color {
        if vm.car.isConnected { return .red }
        if vm.isRetrying      { return .orange }
        return .blue
    }

    // MARK: - Toast ───────────────────────────────────────────────────────────

    private var toastBadge: some View {
        Text(vm.toastMessage)
            .font(.system(size: 13, weight: .medium))
            .foregroundStyle(.white)
            .padding(.horizontal, 20)
            .padding(.vertical, 11)
            .glassEffect(.regular, in: Capsule())
    }
}

// MARK: - Status Pill ─────────────────────────────────────────────────────────

private struct StatusPill: View {
    let icon:   String
    let label:  String
    let active: Bool
    let color:  Color

    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: icon)
                .font(.system(size: 11, weight: .semibold))
            Text(label)
                .font(.system(size: 11, weight: .medium))
                .lineLimit(1)
        }
        .foregroundStyle(active ? color : Color.white.opacity(0.5))
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .glassEffect(.regular, in: Capsule())
    }
}

// MARK: - Car Button ──────────────────────────────────────────────────────────

private struct CarButton: View {
    let label:   String
    let icon:    String
    let color:   Color
    let enabled: Bool
    let action:  () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 9) {
                Image(systemName: icon)
                    .font(.system(size: 15, weight: .semibold))
                Text(label)
                    .font(.system(size: 14, weight: .semibold))
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 12)
        }
        .tint(enabled ? color : Color(white: 0.35))
        .buttonStyle(.glass)
        .disabled(!enabled)
    }
}

// MARK: - Preview ─────────────────────────────────────────────────────────────

#Preview("Portrait — iPhone 15 Pro Max") {
    ContentView()
        .preferredColorScheme(.dark)
}
