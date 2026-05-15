const binding = require('./binding')

// Acquire a WifiManager.MulticastLock so the OS delivers multicast UDP packets
// (required for mDNS / service discovery on Android). No-op on non-Android.
exports.acquireMulticastLock = binding.acquireMulticastLock

// Release the currently held multicast lock. No-op if none held or on non-Android.
exports.releaseMulticastLock = binding.releaseMulticastLock

// Returns the IPv4 address of the active WiFi interface (e.g. "192.168.1.42"),
// or null if none found. Pass to socket.addMembership(MDNS_ADDR, wifiIP) to
// ensure mDNS traffic goes on the right interface.
// Works on Android (NDK 24+), macOS, and Linux.
exports.getWifiIP = binding.getWifiIP
