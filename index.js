const binding = require('./binding')

// On Android, call this once at app startup before using the lock functions.
// Pass the JavaVM pointer and application Context reference from the bare-android runtime.
// No-op on other platforms.
exports.init = binding.init

// Acquire a WifiManager.MulticastLock so the OS delivers multicast UDP packets
// (required for mDNS / service discovery on Android). No-op on non-Android.
exports.acquireMulticastLock = binding.acquireMulticastLock

// Release the multicast lock. Call when discovery is no longer needed.
// No-op on non-Android.
exports.releaseMulticastLock = binding.releaseMulticastLock

// Returns the IPv4 address of the active WiFi interface as a string (e.g.
// "192.168.1.42"), or null if none is found. Useful for passing to
// socket.addMembership(MDNS_ADDR, wifiIP) to bind mDNS to the right interface.
// Works on Android (NDK 24+), macOS, and Linux.
exports.getWifiIP = binding.getWifiIP
