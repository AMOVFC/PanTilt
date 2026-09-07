# PCB review bot

Ask for it, and a board gets reviewed by a PCB engineer who has never seen the
project. That is the whole idea: the value of the review comes from the reviewer
*not* knowing why a decision was made.

Triggered by [`.github/workflows/pcb-review.yml`](../../.github/workflows/pcb-review.yml).

| file | role |
|---|---|
| `sexp.py` | dependency-free KiCad s-expression reader (CI has no plugin cache) |
| `extract_facts.py` | measures the board; emits facts + deterministic findings |
| `summarize_drc.py` | folds `kicad-cli` DRC/ERC JSON down to a readable digest |
| `reviewer_prompt.md` | the reviewer's brief — persona, severity rubric, checklist |
| `format_result.py` | splits the CLI's JSON into review text + a real cost line |

## When it runs

**Only when a human asks.** Nothing here spends money on its own — there is no
automatic review, on any branch or any pull request.

Two ways to ask:

| | how | notes |
|---|---|---|
| **Commit message** | Put `[pcb-review]` in the message of the commit you push to an open PR | Only the head commit is checked — a review looks at the resulting board, not at each commit that got there |
| **PR comment** | Comment `/pcb-review` on the pull request | Re-runnable any number of times; the bot reacts 👀 so you know it heard you |

Both accept an optional model: `[pcb-review] sonnet` or `/pcb-review sonnet`
(also `haiku`, `opus`). Matching is case-insensitive.

`workflow_dispatch` is the third way in — use it to review a board outside any
PR. Leave the `board` input blank to sweep every board in the repo.

### Who is allowed to spend money

This repository is **public**. Comment triggers are refused unless
`author_association` is `OWNER`, `MEMBER`, or `COLLABORATOR` — otherwise anyone
who can type in a PR thread could run up the API bill. `CONTRIBUTOR` and
`FIRST_TIME_CONTRIBUTOR` are refused too: having landed a patch is not the same
as holding the credit card.

The gate is a separate job that runs before anything expensive, and its decision
plus reasoning lands in the run summary, so a refused request is visible rather
than silent.

**All gate matching is plain shell `case` globbing, never `grep`.** Some `grep`
builds abort on `-iF` with bracket characters — `[pcb-review]` is exactly that
shape — and a gate that crashes is a gate that fails open the next time someone
edits it carelessly.

### What keeps the cost down

- **Explicit opt-in.** A commit with no keyword does not start the workflow.
- **Path filter.** Only `.kicad_pcb`, `.kicad_sch`, `.kicad_pro`, `.kicad_mod`
  and BOM/CPL changes are even considered.
- **Concurrency cancellation.** A second request supersedes an in-flight one.
- **Deterministic pre-pass.** The board file never reaches the model. Python
  measures it first and the model receives a ~2 KB fact sheet instead of a
  700 KB file — roughly 2.5 k tokens of input per board rather than 200 k.
- **Self-reported spend.** Every review comment ends with the real
  `total_cost_usd` for that run, so the cost is measured, not estimated.

## The two halves

`extract_facts.py` handles everything that is pure measurement: dimensions,
layer count, part inventory by technology and side, joint counts, courtyard
collisions, parts over the edge, drill and annular ring against process limits,
track widths, unrouted nets, silkscreen legibility. It never editorialises — it
emits the number and the rule, and it states plainly what it did not check.

The model handles judgement: whether a 0.6 mm trace is adequate for *this*
current, whether the decoupling is where it needs to be, whether the assembly
sequence is buildable. `reviewer_prompt.md` is its brief, and the severity
rubric there (`CRITICAL` / `MILD` / `BE AWARE` / `RECOMMENDATION`) is the
contract for the output.

Both halves degrade independently. No `ANTHROPIC_API_KEY` secret, or no KiCad
container available, and the run still posts whatever it did manage to produce.
The deterministic report is free and catches the mechanical mistakes on its own.

## Running it by hand

```bash
python tools/pcb_review/extract_facts.py path/to/board.kicad_pcb
```

Add `--json out.json` for machine-readable findings, `--md out.md` to write the
report to a file instead of stdout. Exit status is always 0 unless the file
cannot be parsed — findings are data, and the workflow decides what blocks a
merge.

To review a board outside a PR, use the workflow's `workflow_dispatch` trigger.
Leave the `board` input blank to sweep every board in the repo.

## Setup

One repository secret: **`ANTHROPIC_API_KEY`**. Without it the workflow runs and
posts the deterministic half, with a note saying the model review was skipped.

`DEFAULT_MODEL` in the workflow sets which model a bare request uses. Opus 5 is
$5/$25 per MTok; Sonnet 5 is $2/$10 and is usually adequate here, since the fact
sheet has already done the measuring and the model is mostly applying judgement
to numbers it has been handed. Start on Opus, read the cost line on a few real
reviews, and decide from your own data.

`KICAD_IMAGES` in the workflow lists container tags to try in order for
`kicad-cli`. **`kicad-cli` must be at least as new as the files it opens** — a
KiCad 10 board will not load in a 9.x CLI. Put a newer tag at the front of that
list when the project moves forward.

## Fab limits

`FAB` at the top of `extract_facts.py` holds standard-process limits — the "no
upcharge, no engineering query" numbers at JLCPCB and PCBWay for 4-layer 1 oz.
Boards that clear them will not generate a DFM email from any prototype house.
Loosen them only if a shop's advanced process has actually been paid for.

`PRICE_BREAKS_MM` is informational: a board that misses the 100 × 100 mm tier by
a few millimetres is worth knowing about before ordering, not after.

## Known limits

- Courtyard collision and parts-over-edge use **bounding boxes**, so a rotated
  or L-shaped part can false-positive. `kicad-cli pcb drc` is authoritative and
  the report says so wherever the approximation is used.
- Unrouted-net detection finds nets with *no copper at all*. A partially routed
  net is not detected — that figure is a lower bound, and DRC gives the true
  ratsnest.
- Copper clearance, silkscreen-over-pad, impedance and thermal relief are not
  checked here at all. They come from DRC.
