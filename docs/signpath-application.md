# SignPath Foundation application (draft, for the maintainer)

Status: not submitted. Apply at https://signpath.org/apply (log in with GitHub).

## Check before applying
- Terms say the project must be "actively maintained", already released in the form to be signed, and pass a check of "a certain verifiable reputation". A very new project may be asked to wait. Releases v0.2.0 to v0.2.3 exist.
- Every team member needs multi-factor authentication on GitHub and on SignPath.
- Add the "Code signing policy" link to the README (done) and to the download page.

## Answers to paste
- Project name: JPurrr-8000
- Repository: https://github.com/SpeakingColors/JPurrr-8000
- License: GNU AGPL-3.0-or-later (OSI approved, no dual licensing)
- Description: Free VST3 and standalone synthesizer in the style of the Roland JP-8000 and JP-8080, with its own effects page. Windows, macOS and Linux.
- Download page: https://github.com/SpeakingColors/JPurrr-8000/releases
- Code signing policy: https://github.com/SpeakingColors/JPurrr-8000/blob/main/docs/code-signing-policy.md
- What to sign: the Windows installer built by `.github/workflows/release.yml` (Inno Setup .exe). Product name `JPurrr-8000`, version taken from the tag.
- Builds: GitHub Actions, from source, on GitHub hosted runners.
- Team: one person (the owner) as author, reviewer and approver.
- Proprietary components: none. Third party code: JUCE (AGPL/GPL option), no closed source parts.
- Network use: none.

## After approval (needs the IDs SignPath gives)
1. Install the SignPath GitHub App on the repository, add secret `SIGNPATH_API_TOKEN`.
2. In `release.yml`, after the Windows installer is built: upload it as an artifact, then call `signpath/github-action-submit-signing-request@v1` with the organization id, project slug, signing policy slug (`release-signing`) and the artifact id, and download the signed file in place of the unsigned one.
3. Approve the request in SignPath for each release.
4. Switch the line in `INSTALL.txt.in` from "not signed" to "signed by SignPath Foundation".
