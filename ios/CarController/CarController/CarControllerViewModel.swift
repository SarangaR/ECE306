import SwiftUI
import ActivityKit
import GameController

/// Central coordinator for car connection, gamepad input, and UI state.
/// Owned by ContentView as a @State property so it lives for the app's lifetime.
@Observable
final class CarControllerViewModel {

    // MARK: - Sub-managers

    let car     = CarConnection()
    let gamepad = GamepadManager()

    // MARK: - UI State

    /// Left joystick  — Y axis → forward/reverse (+1 fwd, -1 rev)
    ///                  X axis → turn in single-joystick mode
    var leftJoystickPosition:  CGPoint = .zero
    /// Right joystick — X axis → turn (+1 right, -1 left) in dual mode only
    var rightJoystickPosition: CGPoint = .zero

    var padNumber: Int = 1   // 1-8

    /// true = left Y + right X (dual sticks)
    /// false = left Y + left X (single stick, free movement)
    var dualJoystickMode: Bool = UserDefaults.standard.object(forKey: "dualJoystickMode") as? Bool ?? true

    // Connection form — persisted across launches
    var ipAddress: String = UserDefaults.standard.string(forKey: "carIPAddress") ?? ""
    var portText:  String = UserDefaults.standard.string(forKey: "carPort")      ?? "3107"

    /// Whether the app is actively retrying connection in the background
    var isRetrying: Bool = false

    // Toast
    var toastMessage: String = ""
    var toastVisible: Bool   = false

    // MARK: - Private

    private var driveTimer:   DispatchSourceTimer?
    private var retryTask:    Task<Void, Never>?
    private var liveActivity: Activity<CarActivityAttributes>?

    /// Tracks whether a non-zero drive command was last sent.
    /// When the stick returns to zero we send exactly one stop command, then go silent
    /// until the stick moves again. This keeps the wire quiet during autonomous commands.
    private var wasDriving: Bool = false

    // MARK: - Init

    init() {
        wireConnectionCallback()
        wireGamepadCallbacks()
        startDriveLoop()

        // Auto-connect to last used IP on launch
        if !ipAddress.trimmingCharacters(in: .whitespaces).isEmpty {
            startAutoConnect()
        }
    }

    deinit {
        driveTimer?.cancel()
        retryTask?.cancel()
    }

    // MARK: - Drive Loop

    private func startDriveLoop() {
        driveTimer?.cancel()
        let timer = DispatchSource.makeTimerSource(queue: .global(qos: .userInteractive))
        timer.schedule(deadline: .now(), repeating: .milliseconds(20), leeway: .milliseconds(1))
        timer.setEventHandler { [weak self] in
            DispatchQueue.main.async { [weak self] in
                self?.sendDriveTick()
            }
        }
        timer.resume()
        driveTimer = timer
    }

    // MARK: - Drive Tick

    func sendDriveTick() {
        guard car.isConnected else {
            wasDriving = false
            return
        }

        // ── Gather input ──────────────────────────────────────────────────
        var fwd: Double = 0
        var trn: Double = 0

        // Read gamepad axes directly from GCController so we always get the
        // live hardware value — avoids any @Observable / MainActor threading
        // issues with stored properties set from GCController callbacks.
        let deadzone = 0.07
        if let pad = GCController.current?.extendedGamepad {
            var rawFwd = Double(pad.leftThumbstick.yAxis.value)
            if abs(rawFwd) < deadzone { rawFwd = 0 }
            fwd = rawFwd * 100

            let rawX = dualJoystickMode
                ? Double(pad.rightThumbstick.xAxis.value)
                : Double(pad.leftThumbstick.xAxis.value)
            var rawTrn = rawX
            if abs(rawTrn) < deadzone { rawTrn = 0 }
            trn = rawTrn * 100
        }

        // On-screen joystick — whichever axis has larger magnitude wins,
        // so both gamepad and touch work simultaneously.
        let joystickFwd = leftJoystickPosition.y * 100
        let joystickTrn = dualJoystickMode
            ? rightJoystickPosition.x * 100
            : leftJoystickPosition.x * 100

        if abs(joystickFwd) > abs(fwd) { fwd = joystickFwd }
        if abs(joystickTrn) > abs(trn) { trn = joystickTrn }

        // ── Send only when needed ─────────────────────────────────────────
        // 2 % deadzone prevents tiny float noise from waking up the car.
        let isMoving = abs(fwd) > 2 || abs(trn) > 2

        if isMoving {
            car.sendDrive(forward: fwd, turn: trn)
            wasDriving = true
        } else if wasDriving {
            // Transitioning from moving → stopped: send one explicit stop,
            // then stay silent so autonomous commands aren't overridden.
            car.sendStop()
            wasDriving = false
        }
        // If !isMoving && !wasDriving: already stopped, send nothing.
    }

    // MARK: - Connection

    func connect() {
        let host = ipAddress.trimmingCharacters(in: .whitespaces)
        guard !host.isEmpty else {
            toast("Enter the car's IP address")
            return
        }
        let port = UInt16(portText.trimmingCharacters(in: .whitespaces)) ?? 3107

        // Persist for next launch
        UserDefaults.standard.set(host, forKey: "carIPAddress")
        UserDefaults.standard.set(portText, forKey: "carPort")

        car.connect(host: host, port: port)
    }

    func disconnect() {
        cancelRetries()
        wasDriving = false
        car.sendStop()
        car.disconnect()
        leftJoystickPosition  = .zero
        rightJoystickPosition = .zero
        // Live Activity update happens via the connection state callback
    }

    // MARK: - Auto-Connect & Retry

    /// Starts a retry loop that calls connect() every ~5 s until connected or cancelled.
    func startAutoConnect() {
        retryTask?.cancel()
        isRetrying = true

        retryTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                self.connect()

                // Poll for up to 6 s in 200 ms increments.
                for _ in 0..<30 {
                    if Task.isCancelled || self.car.isConnected { break }
                    try? await Task.sleep(for: .milliseconds(200))
                }

                if Task.isCancelled || self.car.isConnected { break }

                // Pause before the next attempt.  The 1 s gap lets the robot's
                // TCP stack fully process our RST from the cancel before we
                // send a new SYN — otherwise the server may RST the new attempt.
                try? await Task.sleep(for: .seconds(1))
            }
            if !Task.isCancelled {
                self?.isRetrying = false
            }
        }
    }

    func cancelRetries() {
        retryTask?.cancel()
        retryTask = nil
        isRetrying = false
    }

    // MARK: - Actions

    func stop() {
        wasDriving = false
        leftJoystickPosition  = .zero
        rightJoystickPosition = .zero
        car.sendStop()
    }

    func sendLineFollow()  { car.sendLineFollow() }
    func sendLRoute()      { car.sendLRoute() }
    func sendEnterCircle() { car.sendEnterCircle() }
    func sendExitCircle()  { car.sendExitCircle() }

    func sendPad() {
        guard car.isConnected else { return }
        car.sendPadArrival(padNumber)
        toast("→ Pad \(padNumber)")
    }

    // MARK: - Joystick Mode

    func toggleJoystickMode() {
        dualJoystickMode.toggle()
        UserDefaults.standard.set(dualJoystickMode, forKey: "dualJoystickMode")
        // Reset positions when switching so there's no stale input
        leftJoystickPosition  = .zero
        rightJoystickPosition = .zero
    }

    // MARK: - Live Activity

    private func wireConnectionCallback() {
        car.onStateChange = { [weak self] newState in
            guard let self else { return }
            switch newState {
            case .connected:
                self.isRetrying = false
                self.retryTask?.cancel()
                self.startLiveActivity()
            case .disconnected:
                self.updateLiveActivityDisconnected()
            case .failed:
                self.updateLiveActivityDisconnected()
            default:
                break
            }
        }
    }

    private func startLiveActivity() {
        guard ActivityAuthorizationInfo().areActivitiesEnabled else { return }
        terminateLiveActivity()

        let attrs   = CarActivityAttributes(carIP: ipAddress)
        let state   = CarActivityAttributes.ContentState(isConnected: true, statusText: "Connected")
        let content = ActivityContent(state: state, staleDate: nil)

        liveActivity = try? Activity.request(
            attributes: attrs,
            content: content,
            pushType: nil
        )
    }

    private func updateLiveActivityDisconnected() {
        guard let activity = liveActivity else { return }
        Task {
            let state   = CarActivityAttributes.ContentState(isConnected: false, statusText: "Offline")
            let content = ActivityContent(state: state, staleDate: nil)
            await activity.update(content)
        }
    }

    private func terminateLiveActivity() {
        guard let activity = liveActivity else { return }
        liveActivity = nil
        Task {
            let state   = CarActivityAttributes.ContentState(isConnected: false, statusText: "Offline")
            let content = ActivityContent(state: state, staleDate: nil)
            await activity.end(content, dismissalPolicy: .immediate)
        }
    }

    // MARK: - Toast

    func toast(_ message: String) {
        toastMessage = message
        withAnimation(.spring(response: 0.3)) { toastVisible = true }
        Task {
            try? await Task.sleep(for: .seconds(2))
            withAnimation(.spring(response: 0.3)) { toastVisible = false }
        }
    }

    // MARK: - Gamepad Wiring

    private func wireGamepadCallbacks() {
        gamepad.onStop         = { [weak self] in self?.stop() }
        gamepad.onLineFollow   = { [weak self] in self?.sendLineFollow() }
        gamepad.onLRoute       = { [weak self] in self?.sendLRoute() }
        gamepad.onEnterCircle  = { [weak self] in self?.sendEnterCircle() }
        gamepad.onExitCircle   = { [weak self] in self?.sendExitCircle() }
        gamepad.onZeroOTOS     = { [weak self] in self?.car.sendZeroOTOS() }

        gamepad.onPadIncrement = { [weak self] in
            guard let self else { return }
            if padNumber < 8 { padNumber += 1 }
            sendPad()
        }
        gamepad.onPadDecrement = { [weak self] in
            guard let self else { return }
            if padNumber > 1 { padNumber -= 1 }
            sendPad()
        }
    }
}
