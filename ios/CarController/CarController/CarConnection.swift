import Foundation
import Network

/// Manages a TCP connection to the car and provides typed command methods.
/// All public properties are MainActor-isolated via the project's default actor isolation.
@Observable
final class CarConnection {

    // MARK: - State

    enum ConnectionState {
        case disconnected
        case connecting
        case connected
        case failed(String)

        var displayText: String {
            switch self {
            case .disconnected:      return "Offline"
            case .connecting:        return "Connecting…"
            case .connected:         return "Connected"
            case .failed(let msg):   return msg
            }
        }

        var isConnected: Bool {
            if case .connected = self { return true }
            return false
        }
    }

    var state: ConnectionState = .disconnected

    var isConnected: Bool  { state.isConnected }
    var statusText:  String { state.displayText }

    /// Called on the MainActor whenever the connection state changes.
    var onStateChange: ((ConnectionState) -> Void)?

    // MARK: - Private

    private var connection: NWConnection?
    private let queue = DispatchQueue(label: "com.306.CarController.tcp", qos: .userInitiated)
    private let pin = "1234"

    // MARK: - Connection Lifecycle

    func connect(host: String, port: UInt16) {
        // Cancel the old connection first and give the remote end a moment to
        // process the RST before we fire a new SYN.  Without this pause the
        // server sees RST immediately followed by SYN and replies with RST.
        let previous = connection
        connection = nil
        previous?.cancel()

        state = .connecting

        let nwHost = NWEndpoint.Host(host)
        guard let nwPort = NWEndpoint.Port(rawValue: port) else {
            state = .failed("Invalid port")
            return
        }

        let tcpOptions = NWProtocolTCP.Options()
        tcpOptions.noDelay = true                        // disable Nagle — send immediately
        let params = NWParameters(tls: nil, tcp: tcpOptions)

        let conn = NWConnection(host: nwHost, port: nwPort, using: params)
        self.connection = conn

        conn.stateUpdateHandler = { [weak self] newState in
            Task { @MainActor [weak self] in
                guard let self else { return }
                // Ignore callbacks from a connection that has already been
                // replaced.  Without this guard, the old connection's .cancelled
                // callback would nil-out the new connection reference and kill
                // any in-progress attempt.
                guard self.connection === conn else { return }

                switch newState {
                case .ready:
                    self.state = .connected
                    self.onStateChange?(.connected)

                case .failed(let error):
                    print("[CarConnection] failed: \(error)")
                    let s = ConnectionState.failed("Error")
                    self.state = s
                    self.connection = nil
                    self.onStateChange?(s)

                case .cancelled:
                    // Only reached when WE cancel the active connection
                    // (e.g. user taps Disconnect).  Replaced connections are
                    // filtered by the guard above.
                    self.state = .disconnected
                    self.connection = nil
                    self.onStateChange?(.disconnected)

                case .waiting:
                    // Network.framework is waiting for a path improvement
                    // (e.g. Wi-Fi not yet ready).  Don't report as failure —
                    // just stay in .connecting and let the retry loop handle it.
                    print("[CarConnection] waiting for path…")

                default:
                    break
                }
            }
        }

        conn.start(queue: queue)
    }

    func disconnect() {
        let conn = connection
        connection = nil
        state = .disconnected
        conn?.cancel()
    }

    // MARK: - Car Commands

    /// Continuous drive. forward/turn range: -100.0 … +100.0
    func sendDrive(forward: Double, turn: Double) {
        send("^\(pin)C\(fmt(forward)),\(fmt(turn))\r\n")
    }

    func sendStop() {
        send("^\(pin)C0.0,0.0\r\n")
    }

    func sendLineFollow() {
        send("^\(pin)P\r\n")
    }

    func sendLRoute() {
        send("^\(pin)T\r\n")
    }

    func sendEnterCircle() {
        send("^\(pin)E\r\n")
    }

    func sendExitCircle() {
        send("^\(pin)X\r\n")
    }

    func sendPadArrival(_ pad: Int) {
        send("^\(pin)N\(pad)\r\n")
    }

    func sendZeroOTOS() {
        send("^\(pin)Z\r\n")
    }

    // MARK: - Helpers

    private func fmt(_ value: Double) -> String {
        String(format: "%.1f", value)
    }

    private var driveLogCounter = 0

    private func send(_ message: String) {
        let trimmed = message.trimmingCharacters(in: .whitespacesAndNewlines)
        let isDrive = trimmed.hasPrefix("^\(pin)C")

        if isDrive {
            driveLogCounter += 1
            if driveLogCounter % 50 == 0 {
                let status = connection != nil ? "🟢" : "🔴"
                print("[CarController TX] \(status) \(trimmed)")
            }
        } else {
            let status = connection != nil ? "🟢" : "🔴"
            print("[CarController TX] \(status) \(trimmed)")
        }

        guard let data = message.data(using: .utf8) else { return }
        connection?.send(content: data, completion: .idempotent)
    }
}
