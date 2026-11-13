# COD3 Launcher Implementation Plan

> Execute natively in this isolated exported project; keep game source/install read-only.

Goal: compiled Windows launcher, tested updater, release inventory and author
tools, deployable R2 Worker, and a concrete setup package.

Architecture: shared C# update engine, WinForms player UI, C# author CLI,
read-only JS Worker, Wrangler deployment/upload scripts.

Spec: DESIGN.md. Constraints: Windows x64, .NET Framework, no AVX2, no game
data on GitHub, all six packs plus finished texture replacements, current
master URL, no writes to the live game until explicitly tested/deployed.

Review focus: Windows path escapes/reparse points; interrupted commits and
locked game files; corrupt/oversized chunks; mismatched/unsigned manifests;
preserving unrelated settings/saves across update and repair.

## Task 1: Signed updater and transactional file installation
- [ ] Write/run failing real-filesystem tests for paths, signatures, repair,
  tampered downloads, rollback, restart recovery, settings preservation.
- [ ] Implement src/Core.cs with Manifest, Engine, ITransport and Journal.
- [ ] Run the entire core test executable; compare results.

## Task 2: Publisher and player UI
- [ ] Test curated selection using synthetic installs before packaging.
- [ ] Implement author inventory/build/keygen CLI, immutable chunk objects,
  signed candidate manifest; fail if a source file changes during packaging.
- [ ] Implement WinForms folder selection, automatic check/update, repair,
  two launch buttons, release notes, clear state and error messages.
- [ ] Compile release binaries and exercise form construction/UI where possible.

## Task 3: Hosting and delivery
- [ ] Test Worker health, read-only routing, missing release and chunk responses.
- [ ] Implement R2 binding, deployment and upload commands; publish feed last.
- [ ] Inventory the actual install and confirm six maps and texture coverage.
- [ ] Independent code review, fix material findings, rerun relevant tests.
- [ ] Package player and author binaries/source/docs separately from private key
  and game payload. Persist deliverables. Report live-hosting blockers precisely.
