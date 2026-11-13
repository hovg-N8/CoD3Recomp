# Author tools 0.1.1 validation

36 core/publisher tests pass; WinForms construction check passes; all 7 Worker
tests pass. No client or Worker source behavior was changed for this upgrade.

New tests prove:
- Unchanged second builds reuse every prior file, without recompression.
- Damaged local cached objects are rebuilt and verify afterward.
- Same-size source edits with restored timestamps still rebuild (content hash).
- Existing clients download only an added file, skipping matching files.
- Cloud metadata matches require exact size and the expected ETag.
- Missing/weak/multipart/mismatched ETags use full SHA-256 body verification;
  a truly missing cloud object returns absent without fetching its body.
- Local content changing after earlier verification cannot establish a new
  metadata checksum under the old compressed-content key.
- Holding a file for upload prevents another writer from modifying it.

An independent reviewer found the mutable-local-file checksum race; the
expected checksum is now SHA-256 bound and fixed through probes/upload, while
the object and candidate are held against writes/deletion. That regression
test failed before the fix and passes after it.

The live publishing path still needs a host-side run to observe real Cloudflare
ETags and the new skipped/uploaded summary. No new public release is published
by these local tests. The client's existing update format is unchanged.
