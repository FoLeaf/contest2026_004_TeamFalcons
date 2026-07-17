import { cp, mkdir, mkdtemp, symlink, writeFile } from "node:fs/promises"
import { execFileSync } from "node:child_process"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(new URL("../../../../", import.meta.url).pathname)
const directory = await mkdtemp(join(tmpdir(), "trellis-opencode-model-probe-"))

await mkdir(join(directory, ".opencode"), { recursive: true })
for (const entry of ["agents", "lib", "plugins", "skills", "package.json"]) {
  await cp(join(root, ".opencode", entry), join(directory, ".opencode", entry), {
    recursive: true,
  })
}
await symlink(
  join(root, ".opencode", "node_modules"),
  join(directory, ".opencode", "node_modules"),
  "dir",
)

await mkdir(join(directory, ".trellis", "spec", "guides"), { recursive: true })
await mkdir(join(directory, ".trellis", ".runtime", "sessions"), { recursive: true })
await cp(
  join(root, ".trellis", "workflow.md"),
  join(directory, ".trellis", "workflow.md"),
)
await cp(
  join(root, ".trellis", "scripts"),
  join(directory, ".trellis", "scripts"),
  { recursive: true, filter: source => !source.includes("__pycache__") },
)
await writeFile(join(directory, ".trellis", ".version"), "0.6.6\n", "utf8")
await writeFile(join(directory, ".trellis", ".developer"), "name=Probe\n", "utf8")
await writeFile(join(directory, ".trellis", "config.yaml"), "# isolated probe\n", "utf8")
await writeFile(
  join(directory, ".trellis", "spec", "guides", "index.md"),
  "# Probe Guides\n\nNo project-specific rules.\n",
  "utf8",
)
await writeFile(
  join(directory, "README.md"),
  "# Isolated OpenCode Trellis Probe\n\nThis repository contains no production code.\n",
  "utf8",
)

execFileSync("git", ["init", "-q"], { cwd: directory })
execFileSync("git", ["config", "user.name", "Trellis Probe"], { cwd: directory })
execFileSync("git", ["config", "user.email", "probe@example.invalid"], { cwd: directory })
execFileSync("git", ["add", "."], { cwd: directory })
execFileSync("git", ["commit", "-qm", "probe baseline"], { cwd: directory })

process.stdout.write(`${directory}\n`)
