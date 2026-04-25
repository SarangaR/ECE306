import SwiftUI
import ActivityKit

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

    /// When true the drive loop is suppressed so autonomous commands aren't
    /// immediately overridden by a C0,0 keep-alive tick.
    var autonomousMode: Bool = false

    // Connection form — persisted across launches
    var ipAddress: String = UserDefaults.standard.string(forKey: "carIPAddress") ?? ""
    var portText:  String = UserDefaults.standard.string(forKey: "carPort")      ?? "3107"

    /// Whether the app is actively retrying connection in the background
    var isRetrying: Bool = false

    // Toast
    var toastMessage: String = ""
    var toastVisible: Bool   = false

    // MARK: - Private

    private var driveTimer:  DispatchSourceTimer?
    private var retryTask:   Task<Void, Never>?
    private var liveActivity: Activity<CarActivityAttributes>?

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
        guard car.isConnected else { return }

        let fwd: Double
        let trn: Double

        if gamepad.connectedGamepadName != nil {
            fwd = gamepad.analogForward
            trn = dualJoystickMode ? gamepad.analogTurn : gamepad.analogTurnSingleMode
        } else if dualJoystickMode {
            fwd = leftJoystickPosition.y  * 100
            trn = rightJoystickPosition.x * 100
        } else {
            fwd = leftJoystickPosition.y * 100
            trn = leftJoystickPosition.x * 100
        }

        if autonomousMode {
            if abs(fwd) > 5 || abs(trn) > 5 {
                autonomousMode = false
            } else {
                return
            }
        }

        car.sendDrive(forward: fwd, turn: trn)
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
        autonomousMode = false
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

                // Poll for up to 5 s in 200 ms increments
                for _ in 0..<25 {
                    if Task.isCancelled || self.car.isConnected { break }
                    try? await Task.sleep(for: .milliseconds(200))
                }

                if Task.isCancelled || self.car.isConnected { break }

                // Connected → the callback will clear isRetrying
                // Not connected → pause 3 s before next attempt
                try? await Task.sleep(for: .seconds(3))
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
        autonomousMode = false
        leftJoystickPosition  = .zero
        rightJoystickPosition = .zero
        car.sendStop()
    }

    func sendLineFollow() {
        autonomousMode = true
        car.sendLineFollow()
    }

    func sendLRoute() {
        autonomousMode = true
        car.sendLRoute()
    }

    func sendEnterCircle() {
        autonomousMode = true
        car.sendEnterCircle()
    }

    func sendExitCircle() {
        autonomousMode = true
        car.sendExitCircle()
    }

    func sendPad() {
        car.sendPadArrival(padNumber)
        toast("Pad \(padNumber) sent")
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
                // Update the Live Activity to show "Offline" — it stays visible
                // until the app is explicitly killed (terminateLiveActivity)
                self.updateLiveActivityDisconnected()
            case .failed:
                self.updateLiveActivityDisconnected()
                // If we were in retry mode, the retry loop keeps going
            default:
                break
            }
        }
    }

    private func startLiveActivity() {
        guard ActivityAuthorizationInfo().areActivitiesEnabled else { return }
        terminateLiveActivity()   // dismiss any stale activity before starting fresh

        let attrs   = CarActivityAttributes(carIP: ipAddress)
        let state   = CarActivityAttributes.ContentState(isConnected: true, statusText: "Connected")
        let content = ActivityContent(state: state, staleDate: nil)

        liveActivity = try? Activity.request(
            attributes: attrs,
            content: content,
            pushType: nil
        )
    }

    /// Update the Live Activity state to "Offline" without dismissing it.
    /// The activity stays visible in the Dynamic Island / Lock Screen so the
    /// user can see the car disconnected, then open the app to reconnect.
    private func updateLiveActivityDisconnected() {
        guard let activity = liveActivity else { return }
        Task {
            let state   = CarActivityAttributes.ContentState(isConnected: false, statusText: "Offline")
            let content = ActivityContent(state: state, staleDate: nil)
            await activity.update(content)
        }
    }

    /// Actually end and immediately dismiss the Live Activity.
    /// Called when the app is being terminated (swipe-to-close).
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
            car.sendPadArrival(padNumber)
            toast("Pad \(padNumber)")
        }
        gamepad.onPadDecrement = { [weak self] in
            guard let self else { return }
            if padNumber > 1 { padNumber -= 1 }
            car.sendPadArrival(padNumber)
            toast("Pad \(padNumber)")
        }
    }
}
