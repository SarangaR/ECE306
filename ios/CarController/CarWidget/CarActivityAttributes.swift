import ActivityKit
import SwiftUI

// Shared between the main app target and the CarWidget extension.
struct CarActivityAttributes: ActivityAttributes {

    public struct ContentState: Codable, Hashable {
        var isConnected: Bool
        var statusText:  String
    }

    var carIP: String
}
