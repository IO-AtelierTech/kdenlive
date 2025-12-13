# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: CC0-1.0
#
# CraftMaster wrapper script for Kdenlive WebSocket Fork
# Based on ownCloud client: https://github.com/owncloud/client
#
# Usage:
#   .craft.ps1 --setup                    # Initial Craft setup
#   .craft.ps1 -c --install-deps kdenlive # Install dependencies
#   .craft.ps1 -c --package kdenlive      # Build and package

# Determine Python executable
if ($IsWindows) {
    $python = (python -c "import sys; print(sys.executable)")
    Write-Host "Using Python: ${python}"
} else {
    $python = (Get-Command python3).Source
}

# Get paths
$ScriptDir = [System.IO.Path]::GetDirectoryName($myInvocation.MyCommand.Definition)
$RepoRoot = [System.IO.Path]::GetFullPath("${ScriptDir}/../../")

# Build CraftMaster command
$command = @(
    "${env:HOME}/craft/CraftMaster/CraftMaster/CraftMaster.py",
    "--config", "${RepoRoot}/.craft.ini",
    "--config-override", "${RepoRoot}/.github/workflows/craft_override.ini",
    "--target", "${env:CRAFT_TARGET}",
    "--variables", "WORKSPACE=${env:HOME}/craft"
) + $args

Write-Host "Executing: ${python} ${command}"

# Run CraftMaster
& $python @command

if ($LASTEXITCODE -ne 0) {
    Write-Error "CraftMaster failed with exit code: $LASTEXITCODE"
    exit 1
}
