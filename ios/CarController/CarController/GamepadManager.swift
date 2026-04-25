import Foundation
import GameController

/// Monitors connected MFi / PS4 / Xbox controllers and exposes current analog
/// stick values plus action callbacks for discrete button presses.
///
/// Analog values are updated via GCExtendedGamepad axis handlers (main thread)
/// so they're always fresh when the drive-tick timer reads them.
@Observable
final class GamepadManager {

    // MARK: - Observed State

    /// Vendor name of the currently connected controller, or nil if none.
    var connectedGamepadName: String? = nil

    // Dual-joystick mode bindings
    private(set) var analogForward: Double = 0      // left-stick  Y  (+up  / -down)
    private(set) var analogTurn:    Double = 0      // right-stick X  (+right / -left)

    // Single-joystick mode binding (left stick X replaces right stick X)
    private(set) var analogTurnSingleMode: Double = 0   // left-stick X

    // MARK: - Action Callbacks (set by ViewModel)

    var onStop:         (() -> Void)?
    var onLineFollow:   (() -> Void)?
    var onLRoute:       (() -> Void)?
    var onEnterCircle:  (() -> Void)?
    var onExitCircle:   (() -> Void)?
    var onZeroOTOS:     (() -> Void)?
    var onPadIncrement: (() -> Void)?
    var onPadDecrement: (() -> Void)?

    // MARK: - Private

    private let deadzone = 0.07

    // MARK: - Lifecycle

    init() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidConnect(_:)),
            name: .GCControllerDidConnect,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidDisconnect(_:)),
            name: .GCControllerDidDisconnect,
            object: nil
        )

        // Attach to any controller already paired before the app launched
        if let existing = GCController.controllers().first {
            attach(existing)
        }
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
    }

    // MARK: - Notification Handlers

    @objc private func controllerDidConnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController else { return }
        attach(controller)
    }

    @objc private func controllerDidDisconnect(_ notification: Notification) {
        connectedGamepadName    = nil
        analogForward           = 0
        analogTurn              = 0
        analogTurnSingleMode    = 0
        onStop?()
    }

    // MARK: - Controller Setup

    private func attach(_ controller: GCController) {
        connectedGamepadName = controller.vendorName ?? "Gamepad"

        guard let pad = controller.extendedGamepad else { return }

        // ── Analog sticks ────────────────────────────────────────────────────

        // Left stick Y  → forward / reverse  (both modes)
        pad.leftThumbstick.yAxis.valueChangedHandler = { [weak self] _, value in
            guard let self else { return }
            var v = Double(value)
            if abs(v) < self.deadzone { v = 0 }
            self.analogForward = v * 100
        }

        // Left stick X  → turn  (single-joystick mode only)
        pad.leftThumbstick.xAxis.valueChangedHandler = { [weak self] _, value in
            guard let self else { return }
            var v = Double(value)
            if abs(v) < self.deadzone { v = 0 }
            self.analogTurnSingleMode = v * 100
        }

        // Right stick X → turn  (dual-joystick mode only)
        pad.rightThumbstick.xAxis.valueChangedHandler = { [weak self] _, value in
            guard let self else { return }
            var v = Double(value)
            if abs(v) < self.deadzone { v = 0 }
            self.analogTurn = v * 100
        }

        // ── Discrete buttons ─────────────────────────────────────────────────

        // × (buttonA) → Stop
        pad.buttonA.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onStop?() }
        }

        // ○ (buttonB) → Line Follow
        pad.buttonB.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onLineFollow?() }
        }

        // △ (buttonY) → L-Route
        pad.buttonY.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onLRoute?() }
        }

        // L1 → Enter Circle
        pad.leftShoulder.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onEnterCircle?() }
        }

        // R1 → Exit Circle
        pad.rightShoulder.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onExitCircle?() }
        }

        // Share / Options (buttonMenu) → Zero OTOS
        pad.buttonMenu.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onZeroOTOS?() }
        }

        // D-pad ↑ → Pad increment
        pad.dpad.up.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onPadIncrement?() }
        }

        // D-pad ↓ → Pad decrement
        pad.dpad.down.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed { self?.onPadDecrement?() }
        }
    }
}
