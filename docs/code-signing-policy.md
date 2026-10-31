# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

This applies to the Windows installer (`JPurrr-8000-<version>-Windows-x64-Setup.exe`) once SignPath Foundation has approved the project. Until then the installer is not signed and Windows may show an "unknown publisher" screen (More info, then Run anyway).

## What gets signed
- Only binaries built by this project's GitHub Actions workflow (`.github/workflows/release.yml`) from the source in this repository.
- Nothing built on a personal computer is ever signed.
- Upstream libraries (JUCE and others) are not re-signed.

## Team roles
JPurrr-8000 is a one person project, so all three roles are held by the repository owner:
- Author: [SpeakingColors](https://github.com/SpeakingColors)
- Reviewer: [SpeakingColors](https://github.com/SpeakingColors)
- Approver (approves every signing request by hand): [SpeakingColors](https://github.com/SpeakingColors)

## Privacy
JPurrr-8000 does not send any information to other networked systems. It makes no network connections of its own; the only link (Ko-fi, in the About dialog) opens in your browser when you click it.

## Verifying a download
Every release lists SHA-256 checksums (`SHA256SUMS.txt`, and `SHA256SUMS` inside the zip).
