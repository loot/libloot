Contributing To libloot
=======================

A general guide to contributing to LOOT may be found on [LOOT's website](https://loot.github.io/docs/contributing/How-To-Contribute). Information more specific to this repository is found here.

## Repository Branching Structure

The repository branching structure is pretty simple:

* The `master` branch holds released code.
* The `dev` branch is a next-release branch. It holds code that's working towards the next big release but which isn't there yet. Code on `dev` is generally pretty stable as CI must pass before anything can be merged into it, but may not be release-ready.
* Other branches are generally themed on specific features or groups of changes, and come and go as they are merged into one of the two above, or discarded.

## Getting Involved

The best way to get started is to comment on something in GitHub's commit log or on the [issue tracker](https://github.com/loot/loot/issues) (new tracker entries are always welcome).

Surprise pull requests aren't recommended because everything you touched may have been rewritten, making your changes a pain to integrate, or obsolete. There are only generally a few contributors at any one time though, so there's not much chance of painful conflicts requiring resolution, provided you're working off the correct branch.

When you do make a pull request, please do so from a branch which doesn't have the same name as they branch you're requesting your changes to be merged into. It's a lot easier to keep track of what pull request branches do when they're named something like `you:specific-cool-feature` rather than `you:master`.

## Code Style

libloot's code style is based on the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). Formatting style is codified in the repository's `.clang-format` file, but is not enforced.

### C++ Features

* Static variables may contain non-POD types.
* Reference arguments don't need to be `const` (ie. they can be used for output variables).
* Exceptions can be used.
* Unsigned integer types can be used.
* There's no restriction on which Boost libraries can be used.
* Specialising `std::hash` is allowed.

### Naming

* Constant, enumerator and variable names should use `camelCase` or `underscore_separators`, but they should be consistent within the same scope.
* Function names should use `PascalCase` or `camelCase`, but they should be consistent within the same scope.
