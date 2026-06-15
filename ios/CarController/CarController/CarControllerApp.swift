import SwiftUI
import ActivityKit

// MARK: - App Delegate

/// Handles app lifecycle events that SwiftUI's App protocol doesn't expose,
/// specifically applicationWillTerminate for ending Live Activities synchronously.
class AppDelegate: NSObject, UIApplicationDelegate {

    func applicationWillTerminate(_ application: UIApplication) {
        // End all running Live Activities before the process is killed.
        //
        // We use Task.detached (not Task) to avoid inheriting the main-actor
        // context — Activity.end() must not try to resume on the main thread
        // while we're blocking it with the semaphore.
        //
        // iOS gives ~5 s in applicationWillTerminate; 3 s timeout is safe.
        let semaphore = DispatchSemaphore(value: 0)
        Task.detached {
            for activity in Activity<CarActivityAttributes>.activities {
                let state   = CarActivityAttributes.ContentState(isConnected: false, statusText: "Offline")
                let content = ActivityContent(state: state, staleDate: nil)
                await activity.end(content, dismissalPolicy: .immediate)
            }
            semaphore.signal()
        }
        _ = semaphore.wait(timeout: .now() + 3)
    }
}

// MARK: - App entry point

@main
struct CarControllerApp: App {

    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        WindowGroup {
            ContentView()
                .preferredColorScheme(.dark)
        }
    }
}
