# Packaging

`.github/workflows/release.yml` fills these templates (`render.sh`) with the release's
version and checksums and publishes them. Each channel needs a one-time setup; until its
secret exists (Settings → Secrets and variables → Actions), the workflow skips it.

| Channel | Install | Secret |
|---|---|---|
| `.deb` / `.rpm` | `sudo dnf install https://github.com/bacteriafield/pitchshifter/releases/latest/download/pitchshifter-linux-x86_64.rpm` | none |
| Homebrew tap | `brew install bacteriafield/tap/pitchshifter` | `PACKAGES_TOKEN` |
| Scoop bucket | `scoop bucket add bacteriafield https://github.com/bacteriafield/scoop-bucket` then `scoop install pitchshifter` | `PACKAGES_TOKEN` |
| AUR | `yay -S pitchshifter-bin` | `AUR_SSH_PRIVATE_KEY` |
| Chocolatey | `choco install pitchshifter` | `CHOCO_API_KEY` |
| winget | `winget install Bacteriafield.PitchShifter` | `WINGET_TOKEN` |

## Homebrew and Scoop

1. Create two public repositories, each with an initial commit (a README is enough):
   `bacteriafield/homebrew-tap` and `bacteriafield/scoop-bucket`.
2. Create a fine-grained token with *Contents: Read and write* on both repositories and save it as `PACKAGES_TOKEN`.

## AUR

1. Create an account on <https://aur.archlinux.org> and add an SSH public key to it
   (`ssh-keygen -t ed25519 -f aur -C aur`, then paste `aur.pub` into *My Account*).
2. Save the private key (`aur`) as `AUR_SSH_PRIVATE_KEY`. The first release creates the `pitchshifter-bin` package.

## Chocolatey

1. Create an account on <https://community.chocolatey.org> and copy the API key from *My Account*.
2. Save it as `CHOCO_API_KEY`. Every version is reviewed by Chocolatey's moderators before it shows up, the first one can take days.

## winget

winget only accepts automatic updates for packages it already has, so the first version is submitted by hand after the first release:

1. Install [Komac](https://github.com/russellbanks/Komac) and run
   `komac new Bacteriafield.PitchShifter --version 0.0.1 --urls https://github.com/bacteriafield/pitchshifter/releases/download/v0.0.1/pitchshifter-windows-x86_64.zip`.
   The zip holds a portable app: the nested installer is `pitchshifter-windows-x86_64\bin\PitchShifter.exe` with the command alias `PitchShifter`.
2. In the installer manifest set `ArchiveBinariesDependOnPath: true`: PitchShifter.exe needs the DLLs next to it, so winget must put that folder on PATH instead of linking the exe.
3. Submit the pull request to `microsoft/winget-pkgs` and wait for it to be merged.
4. Create a classic token with the `public_repo` scope and save it as `WINGET_TOKEN`; later releases are submitted by the workflow.
