Contributing To libloot
=======================

A general guide to contributing to LOOT may be found on [LOOT's website](https://loot.github.io/docs/contributing/How-To-Contribute). Information more specific to this repository is found here.

## Repository Branching Structure

The repository branching structure is pretty simple:

* The `master` branch holds code that's released or that will be in the next
  release. Code on `master` is generally pretty stable as CI must pass before
  anything can be merged into it.
* Other branches are generally themed on specific features or groups of changes,
  and come and go as they are merged into one of the two above, or discarded.

## Getting Involved

The best way to get started is to comment on something in GitHub's commit log or on the [issue tracker](https://github.com/loot/loot/issues) (new tracker entries are always welcome).

Surprise pull requests aren't recommended because everything you touched may have been rewritten, making your changes a pain to integrate, or obsolete. There are only generally a few contributors at any one time though, so there's not much chance of painful conflicts requiring resolution, provided you're working off the correct branch.

When you do make a pull request, please do so from a branch which doesn't have the same name as they branch you're requesting your changes to be merged into. It's a lot easier to keep track of what pull request branches do when they're named something like `you:specific-cool-feature` rather than `you:master`.
