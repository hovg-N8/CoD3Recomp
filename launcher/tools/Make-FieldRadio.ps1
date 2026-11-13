$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$voice = New-Object System.Speech.Synthesis.SpeechSynthesizer
$clips = Join-Path $PSScriptRoot 'radio-voice'
New-Item -ItemType Directory -Path $clips -Force | Out-Null
$lines = @(
    'Stay low! Keep moving!',
    'Get down! Incoming!',
    'This is the Allied field bulletin. Reports from Normandy say fighting continued overnight. Relief crews are bringing supplies into villages along the front. More news when reports are confirmed.',
    'Over here! On me!',
    'Field bulletin. Rain and smoke have slowed traffic on the roads south of Caen. Medical crews are working through the night. We will return with the next report.'
)
try {
    $voice.Rate = 1
    $format = [System.Speech.AudioFormat.SpeechAudioFormatInfo]::new(22050,
        [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,
        [System.Speech.AudioFormat.AudioChannel]::Mono)
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        $voice.SetOutputToWaveFile((Join-Path $clips "$i.wav"), $format)
        $voice.Speak($lines[$i])
        $voice.SetOutputToNull()
    }
} finally {
    $voice.Dispose()
}
& python (Join-Path $PSScriptRoot 'mix_field_radio.py')
if ($LASTEXITCODE -ne 0) { throw 'Soundtrack mix failed.' }
