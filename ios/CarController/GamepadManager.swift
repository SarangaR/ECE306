import GameController
import Observation
import UIKit

@MainActor
@Observable
final class GamepadManager {
    var connectedName: String?
    var isConnected: Bool { connectedName != nil }

    var analogFwd: Double  = 0
    var analogTurn: Double = 0

    var onStop:        (() -> Void)?
    var onLineFollow:  (() -> Void)?
    var onRoute:       (() -> Void)?
    var onEnterCircle: (() -> Void)?
    var onExitCircle:  (() -> Void)?
    var onPadUp:       (() -> Void)?
    var onPadDown:     (() -> Void)?
    var onZeroOTOS:    (() -> Void)?

    private var controller: GCController?
    private let deadzone = 0.07

    init() {
        NotificationCenter.default.addObserver(
            self, selector: #selector(didConnect(_:)),
            name: .GCControllerDidConnect, object: nil
        )
        NotificationCenter.default.addObserver(
            self, selector: #selector(didDisconnect(_:)),
            name: .GCControllerDidDisconnect, object: nil
        )
        GCController.startWirelessControllerDiscovery { }
        if let first = GCController.controllers().first {
            Task { @MainActor in self.attach(first) }
        }
    }

    @objc nonisolated private func didConnect(_ n: Notification) {
        guard let gc = n.object as? GCController else { return }
        Task { @MainActor in
            if self.controller == nil { self.attach(gc) }
        }
    }

    @objc nonisolated private func didDisconnect(_ n: Notification) {
        guard let gc = n.object as? GCController else { return }
        Task { @MainActor in
            if gc === self.controller {
                self.controller = nil
                self.connectedName = nil
                self.analogFwd  = 0
                self.analogTurn = 0
            }
        }
    }

    private func attach(_ gc: GCController) {
        controller = gc
        connectedName = gc.vendorName ?? "Controller"
        guard let pad = gc.extendedGamepad else { return }

        // ── One-shot button handlers ──────────────────────────────────────────
        pad.buttonA.pressedChangedHandler = { [weak self] _, _, p in  // × / A  → stop
            guard p else { return }
            Task { @MainActor in self?.onStop?() }
        }
        pad.buttonB.pressedChangedHandler = { [weak self] _, _, p in  // ○ / B  → line follow
            guard p else { return }
            Task { @MainActor in self?.onLineFollow?() }
        }
        pad.buttonY.pressedChangedHandler = { [weak self] _, _, p in  // △ / Y  → L-route
            guard p else { return }
            Task { @MainActor in self?.onRoute?() }
        }
        pad.leftShoulder.pressedChangedHandler = { [weak self] _, _, p in  // L1 → enter circle
            guard p else { return }
            Task { @MainActor in self?.onEnterCircle?() }
        }
        pad.rightShoulder.pressedChangedHandler = { [weak self] _, _, p in  // R1 → exit circle
            guard p else { return }
            Task { @MainActor in self?.onExitCircle?() }
        }
        pad.dpad.up.pressedChangedHandler = { [weak self] _, _, p in
            guard p else { return }
            Task { @MainActor in self?.onPadUp?() }
        }
        pad.dpad.down.pressedChangedHandler = { [weak self] _, _, p in
            guard p else { return }
            Task { @MainActor in self?.onPadDown?() }
        }
        pad.buttonOptions?.pressedChangedHandler = { [weak self] _, _, p in  // Share → zero OTOS
            guard p else { return }
            Task { @MainActor in self?.onZeroOTOS?() }
        }
    }

    func poll() {
        guard let pad = controller?.extendedGamepad else {
            analogFwd = 0; analogTurn = 0; return
        }
        analogFwd  = applyDZ(Double(-pad.leftThumbstick.yAxis.value)) * 100
        analogTurn = applyDZ(Double( pad.leftThumbstick.xAxis.value)) * 100
    }

    private func applyDZ(_ v: Double) -> Double {
        let a = abs(v)
        guard a >= deadzone else { return 0 }
        return (v / a) * (a - deadzone) / (1 - deadzone)
    }
}
