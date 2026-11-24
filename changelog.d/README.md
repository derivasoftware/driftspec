# Changelog entries

A merge request writes its entry for `CHANGELOG.md` here, as one file named after its card
(`DS-403.md`, or `DS-403-rules.md` when one card has several merge requests). It holds what the
entry in `CHANGELOG.md` would hold: what the release brings and what a design written for the release
before has to change. It has no heading; the version is not known until the merge.

Why: the version goes up on merge (`VERSION_BUMP_MODE: "merge"` in `.gitlab-ci.yml`), and a branch
behind main merges without rebasing. Two merge requests that both added an entry at the top of
`CHANGELOG.md` would conflict there, and the conflict would send one of them back for another whole
pipeline. Two files never conflict.

Later, on an up-to-date main and in a merge request of its own, `python3 tools/changelog.py` moves
every released entry into `CHANGELOG.md`, under the version its merge was released as, and deletes the
file. An entry whose release is not on main yet stays here.
