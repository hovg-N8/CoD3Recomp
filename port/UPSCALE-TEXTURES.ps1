# Upscales the game's world textures (bushes, trees, walls, vehicles, the
# guns in hand) for the port's texture replacement, on your own PC, from
# your own game's textures. 0.5.197.
#
# 1. In CoD3Recomp.ini set  texture_dump = 1  and play the map you want for a
#    few minutes (walk around: every texture the game shows is saved once,
#    into textures\dump, and listed per map in textures\maps).
# 2. Quit the game and run UPSCALE-TEXTURES.bat. It upscales 4x with
#    Real-ESRGAN on your graphics card (2x for the bigger ones, never past
#    2048), keeps see-through edges (leaves, fences) sharp, leaves normal
#    maps alone, and writes compressed .dds files (BC7, with mip levels) into
#    textures\replace\hd.
# 3. Set texture_dump = 0 again and start the game: the HD textures are used.
#
# To go back: delete textures\replace\hd. Nothing here is uploaded anywhere;
# the pictures are made from, and stay with, your own copy of the game.
param(
    [string]$Map = '',          # one map's textures (textures\maps\<map>.txt); empty: asks
    [switch]$All,               # every dumped texture
    [int]$MinSize = 64,         # skip textures smaller than this on their short side
    [int]$MaxSize = 2048,       # never make a texture bigger than this
    [int]$Limit = 0,            # at most this many (0: no limit), for a quick try
    [switch]$Redo               # make them again even if a replacement exists
)
$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$Dump = Join-Path $Here 'textures\dump'
$Maps = Join-Path $Here 'textures\maps'
$Replace = Join-Path $Here 'textures\replace'
$Hd = Join-Path $Replace 'hd'
$Tools = Join-Path $Here 'tools\upscale'
$Work = Join-Path $Here 'textures\work'
$Esrgan = Join-Path $Tools 'realesrgan-ncnn-vulkan.exe'
$Texconv = Join-Path $Tools 'texconv.exe'
$EsrganUrl = 'https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip'
$TexconvUrl = 'https://github.com/microsoft/DirectXTex/releases/latest/download/texconv.exe'

function Say($text, $color = 'Gray') { Write-Host $text -ForegroundColor $color }

if (-not (Test-Path $Dump)) {
    Say 'No textures\dump folder yet. Set texture_dump = 1 in CoD3Recomp.ini, play a map for a few minutes, quit, then run this again.' Red
    exit 1
}

# --- the two free tools, fetched once into tools\upscale --------------------
New-Item -ItemType Directory -Force $Tools | Out-Null
$haveModel = Test-Path (Join-Path $Tools 'models\realesrgan-x4plus.param')
if (-not (Test-Path $Esrgan) -or -not $haveModel -or -not (Test-Path $Texconv)) {
    Say 'Two free tools are needed (once):' Yellow
    Say "  Real-ESRGAN ncnn Vulkan (the upscaler, runs on any modern graphics card): $EsrganUrl"
    Say "  texconv (Microsoft's texture compressor): $TexconvUrl"
    $answer = Read-Host 'Download them now into tools\upscale? (Y/N)'
    if ($answer -notmatch '^[Yy]') {
        Say 'Put realesrgan-ncnn-vulkan.exe (with its models folder) and texconv.exe in tools\upscale by hand, then run this again.' Yellow
        exit 1
    }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    if (-not (Test-Path $Esrgan) -or -not $haveModel) {
        $zip = Join-Path $Tools 'realesrgan.zip'
        Say '  downloading Real-ESRGAN...'
        Invoke-WebRequest -Uri $EsrganUrl -OutFile $zip -UseBasicParsing
        $unpack = Join-Path $Tools 'unpack'
        if (Test-Path $unpack) { Remove-Item $unpack -Recurse -Force }
        Expand-Archive -Path $zip -DestinationPath $unpack -Force
        $exe = Get-ChildItem $unpack -Recurse -Filter 'realesrgan-ncnn-vulkan.exe' | Select-Object -First 1
        if (-not $exe) { Say 'The Real-ESRGAN download did not have realesrgan-ncnn-vulkan.exe in it.' Red; exit 1 }
        Get-ChildItem $exe.DirectoryName | ForEach-Object { Copy-Item $_.FullName $Tools -Recurse -Force }
        Remove-Item $unpack -Recurse -Force; Remove-Item $zip -Force
    }
    if (-not (Test-Path $Texconv)) {
        Say '  downloading texconv...'
        Invoke-WebRequest -Uri $TexconvUrl -OutFile $Texconv -UseBasicParsing
    }
    if (-not (Test-Path $Esrgan) -or -not (Test-Path $Texconv)) { Say 'The tools are still missing.' Red; exit 1 }
    Say '  tools ready.' Green
}

# --- the picture work (decided and done with .NET's System.Drawing) ---------
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class Cod3Tex
{
    static byte[] Read(Bitmap b, out int stride)
    {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        stride = d.Stride; var p = new byte[d.Stride * b.Height];
        Marshal.Copy(d.Scan0, p, 0, p.Length); b.UnlockBits(d); return p;
    }
    static void Write(Bitmap b, byte[] p)
    {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        Marshal.Copy(p, 0, d.Scan0, p.Length); b.UnlockBits(d);
    }
    // 1: has see-through texels, 2: mostly hard-edged (alpha tested: leaves,
    // fences), 4: looks like a normal map (bluish, red and green about half).
    public static int Analyze(string path)
    {
        using (var b = new Bitmap(path))
        {
            int stride; var p = Read(b, out stride);
            long n = 0, r = 0, g = 0, bl = 0; int minB = 255, low = 0, mid = 0;
            int sx = Math.Max(1, b.Width / 48), sy = Math.Max(1, b.Height / 48);
            for (int y = 0; y < b.Height; y += sy)
                for (int x = 0; x < b.Width; x += sx)
                {
                    int i = y * stride + x * 4; int B = p[i], G = p[i + 1], R = p[i + 2], A = p[i + 3];
                    r += R; g += G; bl += B; n++; if (B < minB) minB = B;
                    if (A < 250) low++; if (A > 24 && A < 232) mid++;
                }
            int flags = 0;
            if (low > 0) flags |= 1;
            if (low > 0 && mid * 3 < low) flags |= 2;
            double ar = r / (double)n, ag = g / (double)n, ab = bl / (double)n;
            if (ab > 175 && Math.Abs(ar - 128) < 40 && Math.Abs(ag - 128) < 40 && minB > 100) flags |= 4;
            return flags;
        }
    }
    // For the upscaler: opaque, the colour of see-through texels filled in
    // from their neighbours (else black seeps into the edges of leaves).
    public static void Prepare(string path, string output)
    {
        using (var b = new Bitmap(path))
        using (var o = new Bitmap(b.Width, b.Height, PixelFormat.Format32bppArgb))
        {
            int stride; var p = Read(b, out stride); int w = b.Width, h = b.Height;
            var known = new bool[w * h]; bool any = false;
            for (int i = 0; i < w * h; i++) { known[i] = p[(i / w) * stride + (i % w) * 4 + 3] >= 128; if (!known[i]) any = true; }
            for (int pass = 0; any && pass < 16; pass++)
            {
                var next = (bool[])known.Clone(); any = false;
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                    {
                        int k = y * w + x; if (known[k]) continue;
                        int sr = 0, sg = 0, sb = 0, c = 0;
                        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                        {
                            int nx = x + dx, ny = y + dy; if (nx < 0 || ny < 0 || nx >= w || ny >= h || !known[ny * w + nx]) continue;
                            int j = ny * stride + nx * 4; sb += p[j]; sg += p[j + 1]; sr += p[j + 2]; c++;
                        }
                        int i2 = y * stride + x * 4;
                        if (c > 0) { p[i2] = (byte)(sb / c); p[i2 + 1] = (byte)(sg / c); p[i2 + 2] = (byte)(sr / c); next[k] = true; } else any = true;
                    }
                known = next;
            }
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) p[y * stride + x * 4 + 3] = 255;
            Write(o, p);
            o.Save(output, ImageFormat.Png);
        }
    }
    static Bitmap Scaled(Bitmap src, int width, int height)
    {
        var dst = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(dst))
        using (var at = new ImageAttributes())
        {
            g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingMode = CompositingMode.SourceCopy; at.SetWrapMode(WrapMode.TileFlipXY);
            g.DrawImage(src, new Rectangle(0, 0, width, height), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel, at);
        }
        return dst;
    }
    // The upscaled colour at its final size, with the original's alpha
    // scaled up beside it (hard edges kept hard when sharpen is set).
    public static void Finish(string upscaled, string original, string output, int width, int height, bool alpha, bool sharpen)
    {
        using (var loaded = new Bitmap(upscaled))
        using (var src = new Bitmap(loaded.Width, loaded.Height, PixelFormat.Format32bppArgb))
        {
            // An opaque 32-bit copy (the upscaler's file may be 24-bit).
            int ss; var sp = Read(loaded, out ss);
            for (int y = 0; y < loaded.Height; y++) for (int x = 0; x < loaded.Width; x++) sp[y * ss + x * 4 + 3] = 255;
            Write(src, sp);
            using (var dst = Scaled(src, width, height))
            {
                int ds; var dp = Read(dst, out ds);
                if (alpha)
                {
                    using (var orig = new Bitmap(original))
                    using (var o = new Bitmap(orig.Width, orig.Height, PixelFormat.Format32bppArgb))
                    {
                        // The alpha as grey, scaled (so colour never mixes into it).
                        int os; var op = Read(orig, out os);
                        for (int y = 0; y < orig.Height; y++) for (int x = 0; x < orig.Width; x++) { int i = y * os + x * 4; op[i] = op[i + 1] = op[i + 2] = op[i + 3]; op[i + 3] = 255; }
                        Write(o, op);
                        using (var a = Scaled(o, width, height))
                        {
                            int s2; var ap = Read(a, out s2);
                            for (int y = 0; y < height; y++) for (int x = 0; x < width; x++)
                            {
                                int v = ap[y * s2 + x * 4];
                                if (sharpen) v = Math.Max(0, Math.Min(255, (v - 128) * 3 + 128));
                                dp[y * ds + x * 4 + 3] = (byte)v;
                            }
                        }
                    }
                }
                else for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) dp[y * ds + x * 4 + 3] = 255;
                Write(dst, dp);
                dst.Save(output, ImageFormat.Png);
            }
        }
    }
}
'@

# --- which textures ----------------------------------------------------------
if (-not $All -and -not $Map) {
    $lists = @()
    if (Test-Path $Maps) { $lists = Get-ChildItem $Maps -Filter '*.txt' | Sort-Object Name }
    if ($lists.Count -gt 0) {
        Say 'Maps with dumped textures:'
        foreach ($l in $lists) { $n = (Get-Content $l.FullName | Measure-Object -Line).Lines; Say ("  {0,-12} {1} textures" -f $l.BaseName, $n) }
        $Map = Read-Host 'Which map (empty: every dumped texture)'
    }
    if (-not $Map) { $All = $true }
}
$keys = @()
if ($All) { $keys = Get-ChildItem $Dump -Filter '*.png' | ForEach-Object { $_.BaseName } }
else {
    $list = Join-Path $Maps ($Map + '.txt')
    if (-not (Test-Path $list)) { Say "No list for $Map in textures\maps (play it with texture_dump = 1 first)." Red; exit 1 }
    $keys = Get-Content $list | Where-Object { $_ -match '\S' } | Sort-Object -Unique
}
$existing = @{}
if (Test-Path $Replace) { Get-ChildItem $Replace -Recurse -File | Where-Object { $_.Extension -in '.png', '.dds' } | ForEach-Object { $existing[$_.BaseName] = $true } }

$jobs = New-Object System.Collections.ArrayList
$skipped = @{ small = 0; big = 0; done = 0; normal = 0; missing = 0; mask = 0 }
foreach ($key in $keys) {
    if ($key -notmatch '^(\d+)x(\d+)_f(\d+)_([0-9A-Fa-f]{16})$') { continue }
    $w = [int]$Matches[1]; $h = [int]$Matches[2]; $f = [int]$Matches[3]
    if ([Math]::Min($w, $h) -lt $MinSize) { $skipped.small++; continue }
    if ([Math]::Max($w, $h) * 2 -gt $MaxSize) { $skipped.big++; continue }
    if ($f -eq 2) { $skipped.mask++; continue }                  # one channel: masks and lightmaps
    if ($existing.ContainsKey($key) -and -not $Redo) { $skipped.done++; continue }
    $png = Join-Path $Dump ($key + '.png')
    if (-not (Test-Path $png)) { $skipped.missing++; continue }
    $flags = [Cod3Tex]::Analyze($png)
    if ($flags -band 4) { $skipped.normal++; continue }
    $scale = 4; if ([Math]::Max($w, $h) * 4 -gt $MaxSize) { $scale = 2 }
    $tw = [int]([Math]::Ceiling($w * $scale / 4.0) * 4); $th = [int]([Math]::Ceiling($h * $scale / 4.0) * 4)
    [void]$jobs.Add([pscustomobject]@{ key = $key; png = $png; w = $w; h = $h; format = $f; flags = $flags; tw = $tw; th = $th })
    if ($Limit -gt 0 -and $jobs.Count -ge $Limit) { break }
}
Say ("{0} textures to upscale ({1} already done, {2} too small, {3} already big, {4} normal maps and {5} masks left as they are)" -f `
    $jobs.Count, $skipped.done, $skipped.small, $skipped.big, $skipped.normal, $skipped.mask) Cyan
if ($jobs.Count -eq 0) { Say 'Nothing to do.' Green; exit 0 }

# --- in batches of 40: prepare, upscale on the GPU, finish, compress ---------
New-Item -ItemType Directory -Force $Hd | Out-Null
$made = 0; $bytes = 0L; $failed = 0
$batches = [Math]::Ceiling($jobs.Count / 40.0)
for ($b = 0; $b -lt $batches; $b++) {
    $batch = $jobs[($b * 40)..([Math]::Min($jobs.Count, ($b + 1) * 40) - 1)]
    foreach ($d in 'in', 'out', 'final') { $p = Join-Path $Work $d; if (Test-Path $p) { Remove-Item $p -Recurse -Force }; New-Item -ItemType Directory -Force $p | Out-Null }
    foreach ($j in $batch) { [Cod3Tex]::Prepare($j.png, (Join-Path $Work ('in\' + $j.key + '.png'))) }
    Say ("batch {0}/{1}: upscaling {2} textures..." -f ($b + 1), $batches, $batch.Count)
    & $Esrgan -i (Join-Path $Work 'in') -o (Join-Path $Work 'out') -n realesrgan-x4plus -s 4 -f png 2>&1 | Out-Null
    $hard = @(); $soft = @()
    foreach ($j in $batch) {
        $up = Join-Path $Work ('out\' + $j.key + '.png')
        if (-not (Test-Path $up)) { $failed++; continue }
        $final = Join-Path $Work ('final\' + $j.key + '.png')
        $alpha = ($j.flags -band 1) -ne 0
        $sharp = $alpha -and ((($j.flags -band 2) -ne 0) -or $j.format -eq 18)
        [Cod3Tex]::Finish($up, $j.png, $final, $j.tw, $j.th, $alpha, $sharp)
        if ($sharp) { $hard += $final } else { $soft += $final }
    }
    # BC7 with every mip level; alpha-tested ones keep their coverage in the
    # smaller levels (leaves do not thin out with distance).
    if ($soft.Count -gt 0) { & $Texconv -nologo -y -f BC7_UNORM -m 0 --ignore-srgb -o $Hd @soft 2>&1 | Out-Null }
    if ($hard.Count -gt 0) { & $Texconv -nologo -y -f BC7_UNORM -m 0 --ignore-srgb -keepcoverage 0.5 -o $Hd @hard 2>&1 | Out-Null }
    # An older texconv without --ignore-srgb: the same without it.
    $first = @($soft + $hard) | Select-Object -First 1
    if ($first -and -not (Test-Path (Join-Path $Hd ([IO.Path]::GetFileNameWithoutExtension($first) + '.dds')))) {
        if ($soft.Count -gt 0) { & $Texconv -nologo -y -f BC7_UNORM -m 0 -o $Hd @soft 2>&1 | Out-Null }
        if ($hard.Count -gt 0) { & $Texconv -nologo -y -f BC7_UNORM -m 0 -keepcoverage 0.5 -o $Hd @hard 2>&1 | Out-Null }
    }
    foreach ($j in $batch) {
        $dds = Join-Path $Hd ($j.key + '.dds')
        if (Test-Path $dds) { $made++; $bytes += (Get-Item $dds).Length } elseif (Test-Path (Join-Path $Work ('final\' + $j.key + '.png'))) { $failed++ }
    }
}
if (Test-Path $Work) { Remove-Item $Work -Recurse -Force }
$mb = [Math]::Round($bytes / 1MB)
Say ("Done: {0} HD textures in textures\replace\hd ({1} MB on disk, about as much on the graphics card when all are in view){2}" -f $made, $mb, $(if ($failed) { ", $failed failed" } else { '' })) Green
Say 'Set texture_dump = 0 in CoD3Recomp.ini and start the game. To go back, delete textures\replace\hd.'
