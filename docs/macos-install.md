# Installing on macOS (why you see a warning, and what to do)

JPurrr-8000 and the other Studio 87 tools are free and open source. Our macOS downloads are not signed with an Apple Developer ID and not notarized by Apple, so macOS shows a warning the first time you open them, for example "Apple could not verify ... is free of malware".

Each release also has `JPurrr-8000-v<version>.zip` with the installers for all systems, the manual and an `INSTALL.txt`. On a Mac, a zip downloaded in the browser can pass the same mark on to the files inside, so the steps below apply to the .pkg from the zip too.

## Why

- Apple only skips this warning for software from developers who pay for the Apple Developer Program (99 US dollars per year) and send every build to Apple for notarization.
- Our projects are free, with no income. We chose not to pay a yearly fee to Apple to give away free software. Windows works the same way (SmartScreen), and on Linux there is no such step.
- The warning does not mean the file is infected. It means Apple has not checked it. You can check it yourself (see "Is it safe?" below).

## What to do

**Method 1, download with Terminal (easiest, no warning)**
Your browser marks every download as "from the internet", and that mark triggers the block. The `curl` command in Terminal does not add the mark. We tested this on a Mac: the installer opened with no warning.
1. Open Terminal (Applications, Utilities).
2. Run this (replace the version in both places if there is a newer one on the release page):
   `curl -L -o ~/Downloads/JPurrr-8000.pkg https://github.com/SpeakingColors/JPurrr-8000/releases/download/v0.2.3/JPurrr-8000-0.2.3-macOS.pkg`
3. Run `open ~/Downloads/JPurrr-8000.pkg`. The installer opens.

**Method 2, in System Settings (if you downloaded with the browser)**
1. Double click the .pkg. macOS blocks it. Close the message.
2. Open System Settings, Privacy & Security, scroll down to Security.
3. Click Open Anyway next to the blocked file (the button stays for about an hour after the blocked attempt).
4. Confirm with your password. On some Macs the same message comes back; then use Method 1 or 3.

**Method 3, remove the mark from a file you already downloaded**
`xattr -d com.apple.quarantine ~/Downloads/JPurrr-8000-0.2.3-macOS.pkg` then double click the .pkg again.

macOS 14 and older: right click the .pkg, choose Open, then Open again.

Your DAW can load the plugin after the install without more warnings, because the installer puts it in place. If you copy the plugin yourself, run `xattr -dr com.apple.quarantine JPurrr-8000.vst3` on it first.

## Is it safe?

- The full source code is public on GitHub, and the installers are built by public GitHub Actions from that code.
- Every release has `SHA256SUMS.txt`. To check your download, put both files in one folder and run `shasum -a 256 -c SHA256SUMS.txt` in Terminal. It must say OK for your file.
- Only remove the quarantine mark from files you downloaded from the official release page.

## For other Studio 87 tools

The same applies to every macOS download from SpeakingColors: the steps above are identical. If you want it to stop, the only route is a paid Apple Developer ID, and we do not plan to buy one.
