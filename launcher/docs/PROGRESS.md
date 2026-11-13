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

Live status after initial delivery: user deployed the R2 bucket/update Worker
and published initial release 0.5.867. Its live signed manifest was verified
against the local candidate. Actual client download/gameplay testing remains
separate from this manifest check.

Author tools 0.1.1: complete source/file verification remains; unchanged build
files reuse signed previous entries and compressed objects. Cloud comparisons
use HEAD and matching local/R2 metadata to avoid full-body transfers, retaining
SHA-256 fallback for unknown/mismatched metadata. Expected MD5 is bound to the
compressed SHA-256 key and fixed while the local object is held read-only;
candidate metadata is also held read-only during publishing.
New regression tests cover reuse, corrupt cache repair, source changes with
unchanged timestamps, client skip behavior, metadata fallback and upload locks.
Independent review's mutation-race finding was fixed with tests. Final local
suite: 36 core/publisher, 1 UI construction, 7 Worker tests pass.
