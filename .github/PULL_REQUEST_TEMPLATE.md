Thanks for taking the time to contribute to the TeaScript C++ Library!

Please read the short notes below before submitting. They exist to save your
time as much as mine.

## Before you open a pull request

**Please discuss the change in an issue first.**
My strong preference is: open an issue, we agree on *whether* and *how* to
solve it, and only then a pull request gets drafted. Unannounced pull requests
— even correct ones — are likely to be declined, because the fix may conflict
with a design decision, a deprecation strategy, or work already in progress
that is not visible here.

Please also note that this GitHub repository is currently a **snapshot** of a
larger private development repository (which also holds the host application,
the unit tests and the example scripts). Changes normally flow from there to
here. This is intended to change in the long term, but for now it means a merge
here is not automatically a merge upstream.

## Checklist

- [ ] This change was discussed in an issue first, and we agreed on the approach.
      Issue: #___
- [ ] I have read [CONTRIBUTING.md](../CONTRIBUTING.md) and I agree to the
      Contributor License Agreement stated there. In particular I confirm that
      my contribution is my own original work, and I grant the license set out
      in that document.
- [ ] My diff contains **only** the intended change. No unrelated reformatting,
      no re-indentation, and no line-ending (LF/CRLF) conversions.
      Please verify with: `git diff -w --ignore-cr-at-eol`
- [ ] `changelog.txt` is updated.
- [ ] If this is a **breaking change** (API, behavior, or language level),
      `Deprecation_and_Breaking_Changes.txt` is updated as well.
- [ ] The library still builds without warnings on at least one of the
      supported compilers, and I have stated which one below.

## Description

<!-- What does this change do, and why? Link the issue it was agreed in. -->

## Breaking change?

<!-- Yes / No. If yes: what breaks, and what should users do instead? -->

## Tested with

<!-- e.g. MSVC 19.4x / g++ 11 / clang 14, and the C++ standard level used. -->
