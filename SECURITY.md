# Security

DriftSpec reads design files, writes HTML pages from them, and runs a local server for the live
session (`driftspec-live`, on 127.0.0.1 only). A problem in any of them can matter: a design
whose text runs code in the viewer, or a web page that reaches the live session.

## Reporting a problem

Report it privately through GitHub's private vulnerability reporting, under the repository's
Security tab ("Report a vulnerability"), not as a public issue. Say what you did, what happened
and which version (`ds --version`) you used.

We will answer, fix what is confirmed, and say in the [changelog](CHANGELOG.md) what was fixed
once a release carries the fix.

## Supported versions

Fixes go into the latest release.
