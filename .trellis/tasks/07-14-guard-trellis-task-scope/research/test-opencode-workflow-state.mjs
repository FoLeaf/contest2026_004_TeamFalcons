import assert from "node:assert/strict"
import { mkdtemp, mkdir, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join } from "node:path"
import test from "node:test"

import subagentContextPlugin from "../../../../.opencode/plugins/inject-subagent-context.js"
import workflowStatePlugin from "../../../../.opencode/plugins/inject-workflow-state.js"

const WORKFLOW = `# Probe workflow

[workflow-state:no_task]
NO_TASK_BODY
[/workflow-state:no_task]

[workflow-state:planning]
PLANNING_BODY
[/workflow-state:planning]

[workflow-state:in_progress]
IN_PROGRESS_BODY
[/workflow-state:in_progress]
`

async function makeProject() {
  const directory = await mkdtemp(join(tmpdir(), "trellis-opencode-plugin-test-"))
  await mkdir(join(directory, ".trellis", ".runtime", "sessions"), { recursive: true })
  await mkdir(join(directory, ".trellis", "tasks"), { recursive: true })
  await writeFile(join(directory, ".trellis", "workflow.md"), WORKFLOW, "utf8")
  return directory
}

async function addTask(directory, sessionID, id, status) {
  const taskDir = join(directory, ".trellis", "tasks", id)
  await mkdir(taskDir, { recursive: true })
  await writeFile(
    join(taskDir, "task.json"),
    `${JSON.stringify({ id, status })}\n`,
    "utf8",
  )
  await writeFile(
    join(directory, ".trellis", ".runtime", "sessions", `opencode_${sessionID}.json`),
    `${JSON.stringify({ platform: "opencode", current_task: `.trellis/tasks/${id}` })}\n`,
    "utf8",
  )
}

async function inject(directory, sessionID, agent = "build") {
  const plugin = await workflowStatePlugin({ directory })
  const output = { parts: [{ type: "text", text: "ORIGINAL_REQUEST" }] }
  await plugin["chat.message"]({ sessionID, agent }, output)
  return output.parts[0].text
}

async function addCheckContext(directory, taskID) {
  const taskDir = join(directory, ".trellis", "tasks", taskID)
  const guideDir = join(directory, ".trellis", "spec", "guides")
  await mkdir(guideDir, { recursive: true })
  await writeFile(join(guideDir, "index.md"), "GUIDE_BODY\n", "utf8")
  await writeFile(join(taskDir, "prd.md"), "PRD_BODY\n", "utf8")
  await writeFile(
    join(taskDir, "check.jsonl"),
    `${JSON.stringify({ file: ".trellis/spec/guides/index.md", reason: "probe" })}\n`,
    "utf8",
  )
}

async function injectCheckPrompt(directory, sessionID, prompt) {
  const plugin = await subagentContextPlugin({
    directory,
    platform: "linux",
    env: {},
  })
  const output = {
    args: {
      subagent_type: "trellis-check",
      prompt,
    },
  }
  await plugin["tool.execute.before"](
    { tool: "task", sessionID, callID: `call-${Date.now()}` },
    output,
  )
  return output.args.prompt
}

test("injects no_task when the session has no selected task", async () => {
  const directory = await makeProject()
  try {
    const text = await inject(directory, "ses_no_task")
    assert.equal(
      text,
      "<workflow-state>\nStatus: no_task\nNO_TASK_BODY\n</workflow-state>\n\nORIGINAL_REQUEST",
    )
    assert.equal(text.match(/<workflow-state>/g)?.length, 1)
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})

test("injects the selected planning task for the matching session", async () => {
  const directory = await makeProject()
  try {
    await addTask(directory, "ses_planning", "planning-task", "planning")
    const text = await inject(directory, "ses_planning")
    assert.match(text, /^<workflow-state>\nTask: planning-task \(planning\)\nPLANNING_BODY/)
    assert.equal(text.match(/<workflow-state>/g)?.length, 1)
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})

test("injects the selected in-progress task for the matching session", async () => {
  const directory = await makeProject()
  try {
    await addTask(directory, "ses_in_progress", "active-task", "in_progress")
    const text = await inject(directory, "ses_in_progress")
    assert.match(text, /^<workflow-state>\nTask: active-task \(in_progress\)\nIN_PROGRESS_BODY/)
    assert.equal(text.match(/<workflow-state>/g)?.length, 1)
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})

test("does not inject a main-session breadcrumb into Trellis sub-agent turns", async () => {
  const directory = await makeProject()
  try {
    const text = await inject(directory, "ses_subagent", "trellis-implement")
    assert.equal(text, "ORIGINAL_REQUEST")
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})

test("replaces a persisted injected Task wrapper instead of nesting it", async () => {
  const directory = await makeProject()
  try {
    await addTask(directory, "ses_reuse", "reuse-task", "in_progress")
    await addCheckContext(directory, "reuse-task")
    const basePrompt = "Active task: .trellis/tasks/reuse-task\n\nBASE_TASK"

    const first = await injectCheckPrompt(directory, "ses_reuse", basePrompt)
    const second = await injectCheckPrompt(directory, "ses_reuse", first)

    assert.equal(second.match(/<!-- trellis-hook-injected -->/g)?.length, 1)
    assert.equal(second.match(/BASE_TASK/g)?.length, 1)
    assert.equal(second.match(/PRD_BODY/g)?.length, 1)
    assert.equal(second.match(/GUIDE_BODY/g)?.length, 1)
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})

test("unwraps the partial injected prefix reused by the OpenCode main model", async () => {
  const directory = await makeProject()
  try {
    await addTask(directory, "ses_partial", "partial-task", "in_progress")
    await addCheckContext(directory, "partial-task")
    const basePrompt = "Active task: .trellis/tasks/partial-task\n\nPARTIAL_BASE_TASK"
    const partialPersistedPrompt = `<!-- trellis-hook-injected -->
# Check Agent Task

## Your Context

STALE_CONTEXT

---

## Your Task

${basePrompt}`

    const prompt = await injectCheckPrompt(
      directory,
      "ses_partial",
      partialPersistedPrompt,
    )

    assert.equal(prompt.match(/<!-- trellis-hook-injected -->/g)?.length, 1)
    assert.equal(prompt.match(/PARTIAL_BASE_TASK/g)?.length, 1)
    assert.equal(prompt.match(/PRD_BODY/g)?.length, 1)
    assert.equal(prompt.match(/GUIDE_BODY/g)?.length, 1)
    assert.doesNotMatch(prompt, /STALE_CONTEXT/)
  } finally {
    await rm(directory, { recursive: true, force: true })
  }
})
