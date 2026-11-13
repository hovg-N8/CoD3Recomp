# COD3 launcher progress

Scope accepted across conversation; user requested continuation after verifying
the new master server deployment. Native implementation in a separate folder.
No game changes or payload publication yet. Source install is 0.5.867.

Decision: chunks are capped at 8 MiB uncompressed to keep uploads manageable
and allow retry at chunk boundaries. Changed files are reconstructed locally.
R2 provisioning/upload will use a complete extracted author kit, avoiding the
previous missing-config deployment failure.

Implemented: signed chunked updater, real-filesystem backup/recovery, persisted
expected release checks, launch/update lock, WinForms launcher, publisher,
curated install selection, and read-only R2 Worker with complete author CMDs.

Independent review found six material cases. Fixed with regressions: failed
repair launch gating; original backup size/hash validation; launch under lock;
remote compressed-object hash verification; established signing-key continuity;
publisher output equality/junction rejection. No deferred review findings.

Validation: 29 core/publisher tests pass, one WinForms construction/control check
passes, and seven Worker tests pass. Synthetic publisher roundtrip spans a
multi-chunk file. Link rejection is exercised against Windows' existing All
Users link, because the restricted test process cannot create its own junction.

Actual install inventory: 0.5.867, 317 files, ~3.25 GiB; all six map packs and
109 finished textures. No writes to the D: source/install. No release payload
or signing key included in the deliverable ZIP. Signing identity is generated
by the author's first BUILD-RELEASE run in their real local application data.

Live status: multiplayer master deployed and health-verified earlier. R2 UI
confirms enabled, no buckets. Update Worker, bucket and first content release
are not deployed. Command-line network access from this restricted session was
unavailable; host-side complete author kit is provided for that step.
