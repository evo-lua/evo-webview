# evo-webview

This is a modified fork of [webview/webview](https://github.com/webview/webview). Please see the original readme [here](https://github.com/webview/webview/blob/master/README.md).

## Goals

The main purpose of this fork is to make my life easier by eliminating the biggest pain points that I've experienced while integrating WebViews into the [evo](https://evo-lua.github.io/) Lua runtime. 

Since this requires some drastic changes, the rationale for them is outlined below.

### Migration to ObjectiveC++ on Mac OS

For what I assume is portability reasons, all the OSX-specific platform code was written using the Objective-C runtime C APIs. These are extremely low-level, can fail spectacularly at runtime, and make the development experience quite miserable.

### Abandonment of the Header-only Paradigm

Because it's problematic to integrate the header-only library into a multi-language project with the original design, I decided to move away from this model and split the header into multiple files that can be built as a regular (static or dynamic) library.

### Introduction of a CMake-based Build System

Look, I dislike CMake as much as the next guy. But homebrew scripts isn't it. The main idea here is to make the library easier to consume (as a static library). May revert later; some people have suggested Meson as an alternative but I haven't looked into it yet.

### Dropping Support for Go and SWIG-generated Bindings

I just don't need any of these, and I don't know the first thing about them. My use case involves accessing the library from Lua, and I support C/C++ alongside that by sheer necessity. Everything else is up for grabs. If there's interest... "PRs welcome", I guess?

## CMake Build Instructions

We could probably do better, but this should work in principle:

```sh
# Create build files using the Ninja build generator
cmake -S . -B cmakebuild -G Ninja
# Build static library
cmake --build cmakebuild
# Run tests
cd cmakebuild && ctest
```

When in doubt, check the [Build Workflows](https://github.com/evo-lua/evo-webview/tree/main/.github/workflows). They are the only authoritative way of building the library as far as I'm concerned, as they're run on every Pull Request.
