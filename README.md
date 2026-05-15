# bare-android-wifi-manager

Android `WifiManager` bindings for the [Bare](https://github.com/holepunchto/bare) runtime.

Provides two things needed for mDNS / multicast UDP to work on Android:

1. **Multicast lock** — Android drops multicast packets by default to save battery. Acquiring a `WifiManager.MulticastLock` tells the OS to let them through.
2. **WiFi interface IP** — `socket.addMembership` needs the local IP of the WiFi interface to join the right multicast group. This finds it via `getifaddrs`.

Both are no-ops on non-Android platforms so the same code runs everywhere.

## Usage

```js
const wifiManager = require('bare-android-wifi-manager')

// Acquire the multicast lock so UDP multicast packets are delivered.
// No-op on non-Android. The Android application context is obtained
// automatically via ActivityThread.currentApplication().
wifiManager.acquireMulticastLock('my-app-mdns')

// Find the WiFi interface IP to pass to socket.addMembership.
const wifiIP = wifiManager.getWifiIP() // e.g. "192.168.1.42" or null

// ... run mDNS discovery ...

// Release when done.
wifiManager.releaseMulticastLock()
```

### With `bare-mdns-discovery`

```js
const wifiManager = require('bare-android-wifi-manager')
const { Discovery } = require('bare-mdns-discovery')

wifiManager.acquireMulticastLock('my-app-mdns')

const discovery = new Discovery({ service: 'companion-link' })
// Pass wifiIP into addMembership inside Discovery._open() — requires a fork
// or patch of bare-mdns-discovery to accept a multicastInterface option.
```

## API

### `acquireMulticastLock([tag])`

Acquires a `WifiManager.MulticastLock` with the given tag string (default `"bare-mdns"`). Idempotent — acquiring a second lock releases the previous one. No-op on non-Android.

### `releaseMulticastLock()`

Releases the currently held lock. No-op if no lock is held or on non-Android.

### `getWifiIP()`

Returns the IPv4 address of the active WiFi interface (`wlan0`, `en0`, etc.) as a string, or `null` if none is found. Works on Android (NDK API 24+), macOS, and Linux.

## Building

```console
npm i -g bare-make
bare-make generate
bare-make build
bare-make install --link
```

For Android cross-compilation, pass `--platform android --arch arm64` (or `ia32`/`x64`) to `bare-make generate`.

## License

Apache-2.0
