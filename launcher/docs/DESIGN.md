# COD3 launcher, agreed scope

Windows launcher for an existing, extracted CoD3 PC port. On opening, check
an HTTPS update feed, verify signed release metadata, and install changed
content before enabling Campaign and Multiplayer. An unavailable feed permits
playing an intact existing install. Never launch while an interrupted update
needs recovery. Preserve player configuration and saves.

Include both current game executables, runtime DLLs, all six installed map
packs (Champs, Bravo, Valor, Carentan, Seelow, SeelowNight), custom weapon
dependencies, finished textures/replace content including campaign/maps/UI,
and the eight repacked vehicle/mortar archives the current friend update uses.
No ISO, full extracted base game, raw texture dumps, logs, private keys or
development backups in downloadable client packages.

Use a separate cod3-updates Worker and R2 bucket in account
03a45a9eaa5b72d0e95aca3957568b5f. Keep the multiplayer Worker separate.
The verified master server is https://cod3-master.hovgtv.workers.dev.

Approaches considered: offline setup was superseded by the user's request
for automatic updates; direct hosting from their PC was rejected because
downloads must work with it off. Chosen: hosted manifest and file objects,
plus an offline author/publishing tool, with explicit release publication.

Implementation choice: .NET Framework WinForms, compiled with the existing
Windows C# compiler; no Python/Node runtime needed by players. Author tools
use that same runtime plus Cloudflare's official Wrangler CLI.

Files are divided into <=8 MiB compressed, content-addressed chunks. Validate
SHA-256, keep verified chunks after interruption, and rebuild changed files
before committing any. Journal originals and recover on failure/restart.
Public release metadata is RSA-signed; publisher private key stays local.
The publishing tool inventories selected content, builds a candidate release,
uploads objects first and changes stable.json last. No automatic publication
from the developer's changing live folder. Do not downgrade newer local builds.

First release needs a publisher key, R2 bucket, deployed read-only Worker,
uploaded verified candidate and a real two-PC campaign/multiplayer playtest.
Local tests can prove transfer, repair and rollback before any cloud upload.

Launcher visual/audio update requested November 8: original WWII battlefield
art with dark readable UI, optional quiet original war ambience and a saved
mute/on preference. Embed media in the executable so player packages retain
the same simple companions. Pause audio while a launched game is running and
stop/dispose it when closing. Preserve update and publishing protocols.
