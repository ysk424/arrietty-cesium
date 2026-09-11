# Compatibility entry point; the primary launcher is ../run.ps1.
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'run.ps1') @args
