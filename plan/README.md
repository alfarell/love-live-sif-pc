# plan

Roadmaps, milestones, and decisions.

## Rules

- Every new plan topic gets its own folder: `<timestamp>-<topic>` (timestamp `YYYYMMDDHHmm`, local time Asia/Jakarta; topic in lowercase kebab-case). Example: `202610311300-build-the-port-version`.
- Inside it, each plan is a markdown file named `<timestamp>-plan.md`, with the timestamp of when that file was created.
- Revising a plan adds a new timestamped file in the same folder; earlier files are not overwritten.

```
plan/
└── 202610311300-build-the-port-version/
    └── 202610311300-plan.md
```

## Starting milestones (to go into the first plan)

- [ ] Document pt-pc architecture
- [ ] Inventory SIF PKG, DLC, and dump
- [ ] Analyze SIF `eboot.bin` and imports
- [ ] Gap analysis table (pt-pc vs SIF)
- [ ] Load eboot on PC and reach a known point with logging
- [ ] First rendered frame
- [ ] Input, audio, file access
- [ ] Playable main screens
