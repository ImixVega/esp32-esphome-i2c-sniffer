# Publishing a new GitHub repository

Suggested repository name: `esp32-esphome-i2c-sniffer`  
Suggested visibility: **Public**  
Suggested description: `Passive I2C logic sniffer for classic ESP32 + ESPHome (ESP-IDF) with RAW GPIO capture and protocol hints.`

Do **not** initialize the GitHub repository with another README, .gitignore or LICENSE — these files already exist in the release package.

## Create on GitHub

1. In the GitHub interface open **New repository** under `ImixVega`.
2. Name it `esp32-esphome-i2c-sniffer`, select **Public**, leave initial files unchecked, and create.
3. Extract this source ZIP to a local directory and run the following commands from that directory:

```bash
git init -b main
git add .
git commit -m "Initial public release v1.0.0"
git remote add origin https://github.com/ImixVega/esp32-esphome-i2c-sniffer.git
git push -u origin main
git tag -a v1.0.0 -m "Version 1.0.0"
git push origin v1.0.0
```

`git push` uses your own configured GitHub authentication (e.g., Git Credential Manager). **Never paste access tokens into YAML, Git URLs or your README.** Before `git add .`, double-check the contents with `git status`.

## Release notes

A Git tag is not a GitHub Release page. To publish a release page, open **Releases → Draft a new release**, choose the `v1.0.0` tag, and copy release notes from `CHANGELOG.md`.