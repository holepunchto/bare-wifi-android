const test = require('brittle')
const addon = require('.')

test('getWifiIP returns a string or null', (t) => {
  const ip = addon.getWifiIP()
  t.ok(ip === null || typeof ip === 'string', 'ip is string or null')
  if (ip !== null) {
    t.ok(/^\d+\.\d+\.\d+\.\d+$/.test(ip), 'ip looks like an IPv4 address')
  }
})

test('acquireMulticastLock is a no-op on non-Android', (t) => {
  t.execution(() => addon.acquireMulticastLock('test-lock'))
})

test('releaseMulticastLock is a no-op on non-Android', (t) => {
  t.execution(() => addon.releaseMulticastLock())
})
