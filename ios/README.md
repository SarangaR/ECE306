
# CarController – iOS SwiftUI App

Liquid-glass native iOS controller for the ECE306 car. Streams speed over TCP,
supports an MFi/PS4/Xbox gamepad, and exposes one-tap buttons for all car commands.

---

## Requirements

| Item | Version |
|------|---------|
| Xcode | 26 beta (or 16.x with iOS 18 SDK minimum) |
| Target iOS | 17.0+ (runs on iOS 26) |
| macOS | 14 Sonoma or later |
| Apple ID | Free account is enough for sideloading |

---

## Creating the Xcode Project

1. Open **Xcode → File → New → Project…**
2. Choose **iOS → App**
3. Fill in:
   - **Product Name:** `CarController`
   - **Team:** your Apple ID (add in Preferences → Accounts if needed)
   - **Organization Identifier:** anything (e.g. `com.yourname`)
   - **Interface:** SwiftUI
   - **Language:** Swift
   - **Uncheck** "Include Tests"
4. Save the project **inside** `ECE306/ios/CarController/` (or choose the parent
   folder and let Xcode create the group there).

---

## Adding the Source Files

After Xcode creates the project it generates a default `ContentView.swift` and
`<AppName>App.swift`. **Replace / add** files so the folder contains exactly:

```
CarControllerApp.swift   ← rename / replace default App file
ContentView.swift        ← replace default ContentView
CarConnection.swift      ← add
GamepadManager.swift     ← add
JoystickView.swift       ← add
```

To add existing files: **File → Add Files to "CarController"…** → select each
`.swift` file → make sure "Copy items if needed" is **unchecked** (they're
already in the right folder).

---

## Frameworks

Both required frameworks are built-in; no SPM packages needed.

1. Select the **project** in the navigator → **CarController target** →
   **General → Frameworks, Libraries, and Embedded Content**
2. Click **+** and add:
   - `GameController.framework`
   - `Network.framework` (usually auto-linked, but add manually if you see linker errors)

---

## Info.plist Entries

Add these keys to `Info.plist` (or the equivalent in the target's **Info** tab):

| Key | Value |
|-----|-------|
| `NSLocalNetworkUsageDescription` | "CarController needs local network access to connect to the car." |
| `NSBonjourServices` | _(array, leave empty or omit)_ |

Without `NSLocalNetworkUsageDescription` iOS will silently block the TCP
connection on first run.

---

## Sideloading (free Apple ID)

1. Plug your iPhone in via USB.
2. Select your device in the Xcode toolbar.
3. **Product → Run** (⌘R) — Xcode will sign with your free team and install.
4. On iPhone: **Settings → General → VPN & Device Management →** trust the
   developer certificate.
5. Re-run — the app will launch and stay installed for 7 days (free) or 1 year
   (paid developer account).

---

## Usage

1. Make sure your iPhone and the car's ESP32 are on the **same Wi-Fi network**.
2. Open the app, expand the **Connection** panel at the bottom.
3. Enter the car's IP and tap **Connect**.
4. Use the left joystick to drive, or pair a PS4/Xbox controller via Bluetooth.

### Controller mapping (PS4 / Xbox)

| Button | Action |
|--------|--------|
| × / A  | Stop |
| ○ / B  | Line Follow |
| △ / Y  | L-Route (36″→turn→24″→turn→12″) |
| L1 / LB | Enter Circle (turn 90° CW + 24″) |
| R1 / RB | Exit Circle (turn 90° CCW + 2 s drive) |
| D-Pad ▲ | Increment pad counter + send arrival |
| D-Pad ▼ | Decrement pad counter + send arrival |
| Left stick | Drive (forward / turn) |

---

## TCP Protocol Reference

All commands are sent as raw ASCII over TCP port **3107**:

```
^1234C<fwd>,<turn>\r\n   — continuous speed  (20 Hz)
^1234S\r\n               — stop
^1234P\r\n               — line follow (indefinite)
^1234T\r\n               — L-shape route
^1234E\r\n               — enter circle
^1234X\r\n               — exit circle
^1234N<1-8>\r\n          — show pad arrival on LCD
```
