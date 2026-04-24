import Foundation
import Network
import Observation

enum ConnectionState: Equatable {
    case disconnected
    case connecting
    case connected
    case failed(String)

    var label: String {
        switch self {
        case .disconnected:    return "OFFLINE"
        case .connecting:      return "CONNECTING"
        case .connected:       return "CONNECTED"
        case .failed:          return "FAILED"
        }
    }

    var isConnected: Bool { self == .connected }
}

@MainActor
@Observable
final class CarConnection {
    var state: ConnectionState = .disconnected
    var lastIP: String = UserDefaults.standard.string(forKey: "carIP") ?? ""

    private var nwConnection: NWConnection?
    private let carPort: NWEndpoint.Port = 3107
    private let pin = "1234"

    var isConnected: Bool { state == .connected }

    func connect(to ip: String) {
        guard !ip.isEmpty else { return }
        lastIP = ip
        UserDefaults.standard.set(ip, forKey: "carIP")
        state = .connecting

        let endpoint = NWEndpoint.hostPort(
            host: NWEndpoint.Host(ip),
            port: carPort
        )
        let params = NWParameters.tcp
        params.allowLocalEndpointReuse = true

        nwConnection = NWConnection(to: endpoint, using: params)

        nwConnection?.stateUpdateHandler = { [weak self] newState in
            Task { @MainActor [weak self] in
                switch newState {
                case .ready:
                    self?.state = .connected
                case .failed(let err):
                    self?.state = .failed(err.localizedDescription)
                    self?.nwConnection = nil
                case .cancelled:
                    self?.state = .disconnected
                    self?.nwConnection = nil
                default:
                    break
                }
            }
        }

        nwConnection?.start(queue: .global(qos: .userInteractive))
    }

    func disconnect() {
        sendRaw("^\(pin)C0.0,0.0\r\n")
        nwConnection?.cancel()
        nwConnection = nil
        state = .disconnected
    }

    func sendRaw(_ s: String) {
        guard isConnected else { return }
        nwConnection?.send(content: Data(s.utf8), completion: .idempotent)
    }

    func sendSpeed(fwd: Double, turn: Double) {
        sendRaw("^\(pin)C\(fwd.formatted(.number.precision(.fractionLength(1)))),\(turn.formatted(.number.precision(.fractionLength(1))))\r\n")
    }

    func sendCmd(_ letter: Character, value: String = "") {
        sendRaw("^\(pin)\(letter)\(value)\r\n")
    }
}
