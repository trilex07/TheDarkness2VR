param([switch]$ExpectLegacyMismatch)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-projection-history'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
function Extract([string]$signature,[bool]$structure=$false) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0;$end=-1
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
    }
    if($end -lt 0){throw "Unclosed $signature"}
    return $text.Substring($start,$end-$start+1)+$(if($structure){';'}else{''})
}
$parts=@((Extract 'struct VsInfo {' $true),(Extract 'struct StereoParams {' $true),
    (Extract 'static void ApplyEyeShiftToMatrix('),(Extract 'static int ActiveWvpRegister('),
    (Extract 'static bool ApplyShiftToUpload('))
[IO.File]::WriteAllText((Join-Path $out 'actual_projection_history.inl'),($parts -join "`r`n"))
[IO.File]::WriteAllText((Join-Path $out 'actual_shader_identity.inl'),
    (Extract 'static bool VertexShaderAnalysisKnown(')+"`r`n"+
    (Extract 'static bool RememberVertexShader(')+"`r`n"+(Extract 'static void ResetProjectionHistories(')+"`r`n"+
    (Extract 'static void SetActiveVertexShaderMetadata('))
[IO.File]::WriteAllText((Join-Path $out 'actual_world_draw_activity.inl'),(Extract 'static void NoteRecordedWorldDraw() {'))
$sourceText=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
if($sourceText -notmatch 'RecLockGuard lock;\s*g_hadWorldDrawThisFrame=false;' -or
   $sourceText -notmatch 'if\(op>=OP_DrawPrimitive && op<=OP_DrawIndexedPrimitiveUP\)NoteRecordedWorldDraw\(\);') {
    throw 'Recording boundary or locked draw activity publication missing'
}
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'projection_history_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$source`" /Fe:projection_history.exe"
    if($LASTEXITCODE -ne 0){throw 'Projection history build failed'}
    $fixtureArgs=@();if($ExpectLegacyMismatch){$fixtureArgs=@('--expect-legacy-mismatch')}
    & .\projection_history.exe @fixtureArgs
    if($LASTEXITCODE -ne 0){throw 'Projection history regression failed'}
} finally {Pop-Location}
