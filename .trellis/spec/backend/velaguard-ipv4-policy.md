# VelaGuard IPv4 Policy

## Scenario: Runtime DHCP and static policy ownership

### 1. Scope / Trigger

This contract applies when VelaGuard changes Ethernet IPv4 mode at runtime or
when netinit reacquires addressing after a carrier edge. Netinit is the only
owner that writes the active IPv4 address, mask, default route, and DNS.

### 2. Signatures

```c
int netinit_set_ipv4_config(
  FAR const struct netinit_ipv4_config *config);
int netinit_get_ipv4_config(FAR struct netinit_ipv4_config *config);
```

The maintained implementation lives in
`scripts/openvela-netinit-carrier-poll.patch`; the applied `apps` checkout must
remain reverse-checkable against that patch.

### 3. Contracts

- `g_ipv4_policy_lock` protects the desired policy, `g_use_dhcpc`, and the
  policy generation.
- Every accepted runtime setter increments `g_ipv4_policy_generation`.
- DHCP acquisition must not hold the policy lock while waiting for a lease.
- A DHCP attempt captures the generation before waiting. After it returns, it
  reacquires the lock and restores the current static policy when the
  generation changed and the current mode is `NETINIT_IPV4_STATIC`.
- Static restore includes address, mask, default route, and clear-then-set DNS.
- Carrier loss clears active IPv4/DNS state but retains the desired policy.

### 4. Validation & Error Matrix

| Condition | Result |
|---|---|
| Null config or unknown mode | `-EINVAL`; policy unchanged |
| Static address or mask is unspecified | `-EINVAL`; policy unchanged |
| DHCP lease fails and policy is unchanged | DHCP error is returned/retried |
| Policy changes to static during DHCP | Current static policy is replayed; replay result is returned |
| Carrier absent | Desired policy is stored and applied after recovery |

### 5. Good / Base / Bad Cases

- Good: DHCP starts at generation 4, static mode is installed at generation 5,
  and the DHCP completion path reapplies generation 5 before returning.
- Base: DHCP starts and completes at the same generation; its lease remains
  active.
- Bad: DHCP reads `g_use_dhcpc`, drops the lock, and later installs a lease
  without checking whether a newer static policy was installed.

### 6. Tests Required

- Static check: both initial bring-up and carrier retry call the guarded DHCP
  helper with a captured generation.
- Static check: the helper compares generations under the policy lock and
  calls `netinit_apply_static_locked()` for a newer static policy.
- Patch check: `git -C ../apps apply --reverse --check` succeeds.
- Build check: the OpenVela incremental firmware build links the applied API.
- Hardware check: switch DHCP to static while DHCP is waiting, then verify the
  final address, route, and DNS remain static.

### 7. Wrong vs Correct

Wrong:

```c
use_dhcp = g_use_dhcpc;
pthread_mutex_unlock(&g_ipv4_policy_lock);
netlib_obtain_ipv4addr(NET_DEVNAME);
```

Correct:

```c
generation = g_ipv4_policy_generation;
pthread_mutex_unlock(&g_ipv4_policy_lock);
netinit_obtain_ipv4addr(generation); /* Rechecks and restores newer static. */
```
