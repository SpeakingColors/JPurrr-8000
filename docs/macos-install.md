# Installing on macOS (why you see a warning, and what to do)

JPurrr-8000 and the other Studio 87 tools are free and open source. Our macOS downloads are not signed with an Apple Developer ID and not notarized by Apple, so macOS shows a warning the first time you open them, for example "Apple could not verify ... is free of malware".

## Why

- Apple only skips this warning for software from developers who pay for the Apple Developer Program (99 US dollars per year) and send every build to Apple for notarization.
- Our projects are free, with no income. We chose not to pay a yearly fee to Apple to give away free software. Windows works the same way (SmartScreen), and on Linux there is no such step.
- The warning does not mean the file is infected. It means Apple has not checked it. You can check it yourself (see "Is it safe?" below).

## What to do (macOS 15 and newer)

**Method A, in System Settings**
1. Double click the .pkg. macOS blocks it. Close the message.
2. Open System Settings, Privacy & Security, scroll down to Security.
3. Click Open Anyway next to the blocked file (the button stays for about an hour after the blocked attempt).
4. Confirm with your password. The installer opens.

**Method B, in Terminal (use this if Method A only shows the same message again)**
1. Open Terminal (Applications, Utilities).
2. Remove the "downloaded from the internet" mark from the file, for example:
   `xattr -d com.apple.quarantine ~/Downloads/JPurrr-8000-0.2.1-macOS.pkg`
3. Double click the .pkg again. It opens the installer normally.

macOS 14 and older: right click the .pkg, choose Open, then Open again.

Your DAW can load the plugin after the install without more warnings, because the installer puts it in place. If you copy the plugin yourself, run `xattr -dr com.apple.quarantine JPurrr-8000.vst3` on it first.

## Is it safe?

- The full source code is public on GitHub, and the installers are built by public GitHub Actions from that code.
- Every release has `SHA256SUMS.txt`. To check your download, put both files in one folder and run `shasum -a 256 -c SHA256SUMS.txt` in Terminal. It must say OK for your file.
- Only remove the quarantine mark from files you downloaded from the official release page.

## For other Studio 87 tools

The same applies to every macOS download from SpeakingColors: the steps above are identical. If you want it to stop, the only route is a paid Apple Developer ID, and we do not plan to buy one.
