# VS Code Extension Publishing Setup

This document explains how the automated publishing workflow for the StarlingMonkey VS Code debugger extension works.

## Overview

The VS Code extension is published automatically to the VS Code Marketplace when changes are merged to the `main` branch that affect files in the `debugger/vscode-dap-extension/` directory.

## Workflow

The [publishing process](../../.github/workflows/vscode-extension-release.yml) uses two GitHub Actions jobs:

1. **release-please**: Creates or updates a release PR when changes are detected. When this PR is merged, it creates a GitHub release with an appropriate version tag (e.g., `v0.3.0`).

2. **publish-extension**: When a release is created, this job:
   - Builds the extension
   - Packages it as a `.vsix` file
   - Publishes it to the VS Code Marketplace
   - Uploads the `.vsix` file to the GitHub release

## Configuration

### Release Please

The extension is configured as a separate package in the top-level release-please config file.

### Required Secret

To publish to the VS Code Marketplace, a `VSCE_PAT` secret must be configured in the repository settings:

1. Go to https://dev.azure.com
2. Create a Personal Access Token (PAT) with:
   - Organization: `All accessible organizations`
   - Expiration: Choose an appropriate duration
   - Scopes: Select `Marketplace` > `Manage`
3. Add this token as a repository secret named `VSCE_PAT` in the GitHub repository settings

### Publisher Account

The extension is published under the `bytecodealliance` publisher account on the VS Code Marketplace.
Publishing a fork under a different account requires changing the `package.json` file accordingly.
In that case, make sure to also change the name and update the other metadata to point to your repository, etc.

## Making a Release

To trigger a new release:

1. Make changes to files in `debugger/vscode-dap-extension/`
2. Use [Conventional Commits](https://www.conventionalcommits.org/) format with the scope `debugger):
   - `feat(debugger):` for new features (bumps minor version)
   - `fix(debugger):` for bug fixes (bumps patch version)
   - `BREAKING CHANGE:` in footer for breaking changes (bumps major version)
   - `chore(debugger):`, `docs(debugger):`, etc. for other changes (no version bump)
3. Open a PR with your changes
4. Merge the PR to `main`
5. Release-please will create/update a release PR
6. Merge the release PR to trigger publishing

## Version Management

- Current version is tracked in `package.json`
- Release-please automatically updates the version
- Tags are prefixed with `v` (e.g., `v0.3.0`)

## Troubleshooting

### Publishing Fails

Check that:
- The `VSCE_PAT` secret is valid and hasn't expired
- The publisher account has proper permissions
- The extension builds successfully locally with `npm run build && npm run package`

### No Release PR Created

Ensure:
- Changes are in the `debugger/vscode-dap-extension/` directory
- Commits follow the Conventional Commits format
