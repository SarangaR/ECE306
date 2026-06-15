import ActivityKit
import SwiftUI
import WidgetKit

// MARK: - Bundle entry point

@main
struct CarWidgetBundle: WidgetBundle {
    var body: some Widget {
        CarLiveActivityWidget()
    }
}

// MARK: - Live Activity configuration

struct CarLiveActivityWidget: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: CarActivityAttributes.self) { context in

            // ── Lock Screen / StandBy banner ──────────────────────────────
            HStack(spacing: 14) {
                Image(systemName: "burst.fill")
                    .font(.system(size: 32, weight: .black))
                    .foregroundColor(Color.orange)

                VStack(alignment: .leading, spacing: 3) {
                    Text("Car Controller")
                        .font(.system(size: 15, weight: .bold))
                        .foregroundColor(Color.white)
                    Text(context.state.statusText)
                        .font(.system(size: 12, weight: .medium))
                        .foregroundColor(context.state.isConnected ? Color.green : Color.orange)
                }

                Spacer()

                Text(context.attributes.carIP)
                    .font(.system(size: 11, weight: .medium, design: .monospaced))
                    .foregroundColor(Color.white.opacity(0.6))
                    .lineLimit(1)
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 16)
            .activityBackgroundTint(Color.black)
            .activitySystemActionForegroundColor(Color.white)

        } dynamicIsland: { context in
            DynamicIsland {

                // ── Expanded (long-press) ─────────────────────────────────
                DynamicIslandExpandedRegion(.leading) {
                    Image(systemName: "burst.fill")
                        .font(.system(size: 42, weight: .black))
                        .foregroundColor(Color.orange)
                        .padding(.leading, 8)
                }

                DynamicIslandExpandedRegion(.trailing) {
                    VStack(alignment: .trailing, spacing: 6) {
                        HStack(spacing: 6) {
                            Circle()
                                .fill(context.state.isConnected ? Color.green : Color.red)
                                .frame(width: 9, height: 9)
                            Text(context.state.isConnected ? "CONNECTED" : "OFFLINE")
                                .font(.system(size: 11, weight: .black))
                                .foregroundColor(context.state.isConnected ? Color.green : Color.red)
                        }
                        Text(context.attributes.carIP)
                            .font(.system(size: 10, weight: .semibold, design: .monospaced))
                            .foregroundColor(Color.white.opacity(0.55))
                    }
                    .padding(.trailing, 8)
                }

                DynamicIslandExpandedRegion(.bottom) {
                    HStack(spacing: 8) {
                        Image(systemName: "car.fill")
                            .foregroundColor(Color.white.opacity(0.35))
                            .font(.system(size: 11))
                        Text(context.state.isConnected ? "Car Controller  ·  Active session" : "Car Controller  ·  Disconnected")
                            .font(.system(size: 11, weight: .medium))
                            .foregroundColor(Color.white.opacity(0.35))
                    }
                    .padding(.bottom, 4)
                }

            } compactLeading: {

                Image(systemName: "burst.fill")
                    .font(.system(size: 17, weight: .black))
                    .foregroundColor(Color.orange)
                    .padding(.leading, 4)

            } compactTrailing: {

                HStack(spacing: 3) {
                    Circle()
                        .fill(context.state.isConnected ? Color.green : Color.red)
                        .frame(width: 7, height: 7)
                    Text(context.state.isConnected ? "ON" : "OFF")
                        .font(.system(size: 10, weight: .black))
                        .foregroundColor(context.state.isConnected ? Color.green : Color.red)
                }
                .padding(.trailing, 4)

            } minimal: {

                Image(systemName: "burst.fill")
                    .font(.system(size: 14, weight: .black))
                    .foregroundColor(Color.orange)

            }
            .widgetURL(URL(string: "carcontroller://open"))
            .keylineTint(Color.orange)
        }
    }
}
