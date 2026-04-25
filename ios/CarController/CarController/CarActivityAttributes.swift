import ActivityKit
import SwiftUI

// MARK: - Live Activity Attributes
//
// ⚠️  This file must be compiled in TWO targets:
//      1. CarController  (main app — already included automatically)
//      2. CarWidget      (widget extension — add manually in Xcode:
//                         select CarWidget target → Build Phases →
//                         Compile Sources → + → CarActivityAttributes.swift)

struct CarActivityAttributes: ActivityAttributes {

    // Dynamic state updated while the activity is live
    public struct ContentState: Codable, Hashable {
        var isConnected: Bool
        var statusText:  String
    }

    // Static data set once when the activity starts
    var carIP: String
}
