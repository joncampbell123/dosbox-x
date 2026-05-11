$ErrorActionPreference = "Stop"

$project_root = "$psScriptRoot\.."
Write-Output "project_root: $project_root"

$sdl2_version = "2.0.4"
$sdl2_zip = "SDL2-devel-$($sdl2_version)-VC.zip"

$sdl2_url = "https://www.libsdl.org/release/$($sdl2_zip)"
$sdl2_dlpath = "$($Env:TEMP)\$sdl2_zip"

$sdl2_bindir = "$($project_root)"
$sdl2_extractdir = "$($sdl2_bindir)\SDL2-$($sdl2_version)"
$sdl2_root_name = "SDL2-devel-VC"

echo "sdl2_bindir:     $sdl2_bindir"
echo "sdl2_extractdir: $sdl2_extractdir"
echo "sdl2_root_name:  $sdl2_root_name"

echo "Cleaning previous artifacts"
if (Test-Path $sdl2_extractdir) {
    Remove-Item $sdl2_extractdir -Recurse -Force
}
if (Test-Path "$($sdl2_bindir)/$sdl2_root_name") {
    Remove-Item "$($sdl2_bindir)/$sdl2_root_name" -Recurse -Force
}
if (Test-Path $sdl2_dlpath) {
    Remove-Item $sdl2_dlpath -Force
}

Write-Output "Downloading $sdl2_url"
Invoke-WebRequest -Uri $sdl2_url -OutFile $sdl2_dlpath

Write-Output "Extracting archive"
Expand-Archive $sdl2_dlpath -DestinationPath $sdl2_bindir

Write-Output "Setting up SDL2 folder"
Rename-Item $sdl2_extractdir $sdl2_root_name

Write-Output "Done"
