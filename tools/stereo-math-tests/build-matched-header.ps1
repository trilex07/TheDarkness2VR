$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot | Split-Path -Parent
$out=Join-Path $root 'build/matched-header-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function Extract($path,$start,$end,$name) {
 $text=[IO.File]::ReadAllText((Join-Path $root $path))
 $first=$text.IndexOf($start);$last=$text.IndexOf($end,$first)
 if($first -lt 0 -or $last -le $first){throw "Missing actual source: $start"}
 [IO.File]::WriteAllText((Join-Path $out $name),$text.Substring($first,$last-$first))
}
Extract 'src/matched_capture.inl' 'static bool Current()' 'static std::string HashFile(' 'matched_current.inl'
Extract 'src/matched_capture.inl' 'static bool LiveSkinCaptureWindow()' 'static unsigned skinUploadsSeen' 'matched_skin_window.inl'
Extract 'src/d3d9.cpp' 'static void NativeCameraCaptureChanged() {' "`n#endif" 'matched_camera_changed.inl'
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$out`" `"$PSScriptRoot/matched_header_test.cpp`" /Fe:matched-header-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Build failed'}
 & './matched-header-test.exe'
 if($LASTEXITCODE -ne 0){throw 'Capture header regression failed'}
} finally {Pop-Location}
