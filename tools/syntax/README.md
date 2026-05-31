# TeaScript Syntax Highlighting

Helpers for rendering syntax-highlighted TeaScript code.

## Pygments lexer — `teascript_lexer.py`

A [Pygments](https://pygments.org/) lexer for the TeaScript language. It powers
the syntax highlighting of the TeaScript code examples on the
[TeaScript home page](https://teascript.run-by-ai.cloud/) and can be used by
anyone who wants to render TeaScript code as HTML or other formats — for blogs,
documentation, and static site generators (Sphinx, MkDocs, Pelican, Hugo via
Pygments, …).

It is released into the **public domain** (see the notice at the top of the
file), so you can use, modify and ship it without restrictions.

### Requirements

```
pip install pygments
```

### Quick use (Python)

```python
import pygments
from pygments.formatters import HtmlFormatter
from teascript_lexer import TeaScriptLexer

code = open("example.tea", encoding="utf-8").read()
print(pygments.highlight(code, TeaScriptLexer(), HtmlFormatter()))
```

### Command line

`pygmentize` can load the lexer straight from the file with `-x`:

```
pygmentize -x -l teascript_lexer.py:TeaScriptLexer -f html -o example.html example.tea
```

### Register as a permanent Pygments plugin (optional)

To let Pygments find the lexer by name (`tea` / `teascript`) or by the `*.tea`
file extension everywhere, expose it as an entry point. Minimal
`pyproject.toml`:

```toml
[project.entry-points."pygments.lexers"]
teascript = "teascript_lexer:TeaScriptLexer"
```

After `pip install .` you can simply run `pygmentize -l teascript example.tea`.

### Scope / notes

The lexer covers the TeaScript language and the most common Core Library
functions. The Core Library is large, so the recognized-function list is not
exhaustive on purpose — unrecognized names are highlighted as plain
identifiers, which keeps old and brand-new scripts readable. Pull requests that
extend the recognized names are welcome.
