# PowerShell Argument Forwarding Fix Design

## Problem

The Windows debug-flash task passes `-DebugBuild` and `-OutDir` to a child
PowerShell script through a string array. Array splatting does not preserve the
tokens as named parameters for this script invocation, so the child binds them
positionally and attempts to open a WSL distribution named `-DebugBuild`.

The same pattern exists in the OpenOCD compatibility wrapper.

## Chosen Design

Use PowerShell hashtable splatting at both forwarding boundaries. Each key is
the destination script's real parameter name, and switch keys are included
only when enabled. This keeps PowerShell's parameter binder authoritative and
avoids positional dependence.

The Cube flash script will always pass `OutDir` by name and conditionally pass
`DebugBuild`. The OpenOCD wrapper will conditionally populate all explicitly
provided paths and switches before forwarding them to the Cube flash script.

## Alternatives Considered

- Duplicate explicit script calls for debug and release modes. This is
  reliable but repeats the invocation and becomes harder to maintain as
  options are added.
- Make child scripts accept malformed positional input. This hides the caller
  defect and weakens the parameter contract.

Hashtable splatting is the smallest idiomatic fix and preserves the existing
script interfaces.

## Compatibility and Error Handling

No VS Code task names, public script parameters, default paths, build flags,
or flash sequencing change. Existing failures from WSL, CubeProgrammer, image
validation, and ST-LINK remain visible with their existing exit behavior.

## Validation

Extend the QSPI boot-flow harness to require named hashtable forwarding and to
reject the prior string-array construction. Run the QSPI harness, the contest
harness, and `git diff --check`. The final hardware acceptance test runs the
Windows debug-flash task and verifies that the log names `Debian` as the WSL
distribution and `debug` as the build kind.
