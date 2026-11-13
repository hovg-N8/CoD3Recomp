# Incremental author tools — 0.1.1

The existing player launcher already compares file sizes and SHA-256 hashes.
Matching files are skipped, missing/changed files are repaired, and a player
missing the entire selected content pack receives the complete pack. A fresh
install still requires the player's own extracted base COD3 game.

The old author tools made progress look like a full repeat: BUILD recompressed
every selected file, while PUBLISH downloaded every cloud object to check its
hash even when it subsequently skipped the upload. The new tools separate this
checking work from actual new content.

BUILD now verifies its previous signed candidate, hashes each selected source
file and cached compressed object, and reuses unchanged entries without
recompression. Missing/corrupt cached objects and changed source files are
rebuilt. Complete manifests still list every required file so new players can
install everything; they are not cumulative patch chains. inventory.json adds
checkedFiles, reusedFiles and builtFiles counters.

PUBLISH checks cloud metadata first. Matching single-part R2 ETags and lengths
avoid downloading or uploading existing objects. Unknown, weak, multipart,
missing or mismatched ETags keep full SHA-256 download verification as a
fallback. Missing/corrupt objects are uploaded and confirmed. Local objects are
locked against modification through verification/upload, and expected metadata
checksums are bound to their compressed SHA-256 content keys.

Progress distinguishes Checking, Reused, Built, Already present - skipped, and
Uploading new or repaired content. The final summary counts skipped/uploaded
objects and uploaded bytes. Signed release metadata is always republished last.
Scanning all files and checking cloud metadata is expected; it is not a full
player re-download or an upload of every object.

Upgrade: let any running BUILD/PUBLISH finish and close it, then extract the
updated author kit into the SAME existing author folder, replacing its shipped
tool files. Keep release-candidate and the existing matching player/public-key.xml.
No private key is included in the update; the existing AppData signing identity
is retained. Use the same BUILD-RELEASE.cmd / PUBLISH-RELEASE.cmd workflow.
Existing players keep their current launcher. No Worker deployment is required.

The source selection remains the project's current curated list. New content
inside selected folders is detected automatically. New map-pack names or custom
weapon directories outside that list require extending the selection rules.
