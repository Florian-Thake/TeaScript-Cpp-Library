# TeaScript Pygments Lexer
# ========================
# Syntax-highlighting lexer for the TeaScript programming language.
#
# Project:  https://teascript.run-by-ai.cloud/
# Library:  https://github.com/Florian-Thake/TeaScript-Cpp-Library
# Author:   Florian Thake
#
# Public domain dedication (CC0-style):
# To the extent possible under law, the author has waived all copyright and
# related or neighboring rights to this file. You may copy, modify, publish,
# use, compile, sell, or distribute it, for any purpose, commercial or
# non-commercial, and by any means, without asking permission. The file is
# provided "as is", without warranty of any kind.

"""Pygments lexer for the TeaScript programming language.

TeaScript is an embeddable, standalone multi-paradigm scripting language with a
syntax close to C++ but easier to use. See https://teascript.run-by-ai.cloud/
for the language and https://github.com/Florian-Thake/TeaScript-Cpp-Library for
the C++ library.

Use it directly with pygments.highlight():

    import pygments
    from pygments.formatters import HtmlFormatter
    from teascript_lexer import TeaScriptLexer

    html = pygments.highlight(code, TeaScriptLexer(), HtmlFormatter())

The lexer answers to the names "tea" / "teascript" and to the *.tea file
extension. See the accompanying README.md for command-line usage and how to
register it as a permanent Pygments plugin.

Scope: the lexer covers the TeaScript language and the most common Core Library
functions. The Core Library is large, so the recognized-function list is not
exhaustive on purpose -- any name that is not specifically recognized falls
through to a plain identifier, which keeps unknown (and old or brand-new)
functions readable.

Requires: pygments (pip install pygments).
"""
from __future__ import annotations

from pygments.lexer import RegexLexer, bygroups, include, words
from pygments.token import (
    Comment, Keyword, Name, Number, Operator, Punctuation, String,
    Text, Whitespace,
)

__all__ = ["TeaScriptLexer"]


# ----- vocabulary ----------------------------------------------------------

# Declarators and statement keywords.
_KEYWORDS = (
    "def", "const", "undef", "is_defined", "mutable",
    "func", "return",
    "if", "else",
    "repeat", "forall", "stop", "loop", "with", "in",
    "catch", "suspend", "yield",
)

# Operator-spelled-as-word.
_OP_WORDS = (
    "and", "or", "not", "mod",
    "is", "as",
    "typename", "typeof", "debug",
    "bit_and", "bit_or", "bit_xor", "bit_not", "bit_lsh", "bit_rsh",
    "eq", "ne", "gt", "lt", "ge", "le",
)

# Built-in types. We accept both the lowercase i64/u64/... that appear in
# real code AND the capitalized I64/U64/... that appear in docs/error text.
_TYPES = (
    "Bool",
    "i64", "u64", "u8", "f64", "f32",
    "I64", "U64", "U8", "F64", "F32",
    "String", "Tuple", "Buffer", "Error", "Function",
    "IntegerSequence", "TypeInfo", "Number",
    # Additional type/concept names used in documentation signatures and in
    # `is` / `as` checks. Const + NaV are registered as predefined TypeInfo
    # globals in CoreLibrary.hpp; Any / UTF8_Iterator / Sequence / Passthrough
    # appear only in signatures and prose.
    "Any", "Const", "NaV",
    "UTF8_Iterator", "Sequence", "Passthrough",
)

# Literal-valued keywords.
_CONSTANTS = ("true", "false", "void")

# Built-in / Core Library functions and well-known names. Not exhaustive -- the
# Core Library is large; the goal is to highlight the most common ones so that
# typical scripts and documentation examples read well. Anything missing falls
# through to plain Name.
_BUILTINS = (
    # I/O
    "print", "println", "cprint", "format", "make_rgb",
    # tuple / buffer / sequence
    "_tuple_create", "_tuple_size", "_tuple_val", "_tuple_named_append",
    "_tuple_remove", "_tuple_swap", "_tuple_append",
    "_seq", "_buf", "_buf_resize", "_buf_size",
    # cast/convert
    "to_string", "_strtonum", "_strtonumex", "_strfromascii",
    # math / util
    "abs", "inc", "dec", "min", "max", "sqrt", "pow",
    # script lifecycle
    "_Exit", "_exit", "_exit_failure", "_exit_success",
    "fail", "fail_with_message", "fail_with_error",
    # introspection / runtime
    "_timestamp", "_version_major", "_version_minor", "_version_patch",
    "timetostr",
    # file / JSON / BSON I/O
    "readtextfile", "writetextfile", "readjsonstring", "writejsonstring",
    "readbsonbuffer", "writebsonbuffer",
)

# Tokens that look like control names but really are special pseudo-identifiers
# the runtime provides to scripts.
_SPECIAL_NAMES = ("args", "argN", "arg1", "features")


# ----- raw-string handling -------------------------------------------------

# Raw strings open with N>=3 quotes and close with exactly the same N quotes
# (more than N is fine; the *first* N of the closing run end the string).
# Python regex can't back-reference group LENGTH, so we enumerate counts
# 3..8 in descending order -- TeaScript raw strings in practice top out at 4
# quotes, so this leaves plenty of headroom.
def _raw_string_rules():
    rules = []
    for n in range(8, 2, -1):
        q = '"' * n
        rules.append((q + r'[\s\S]*?' + q, String))
    return rules


# ----- lexer ---------------------------------------------------------------

class TeaScriptLexer(RegexLexer):
    name = "TeaScript"
    aliases = ["tea", "teascript"]
    filenames = ["*.tea"]
    mimetypes = ["text/x-teascript"]

    tokens = {
        "root": [
            include("whitespace"),
            include("comments"),
            include("hashlines"),

            # Raw strings must precede normal string rule (longer match wins
            # would not be reliable across enumerated lengths otherwise).
            *_raw_string_rules(),

            # Normal string opens.
            (r'"', String, "string"),

            # Numbers. Float (with optional exponent and/or fxx suffix) must
            # be tried before integer to avoid eating just the integer part.
            (r"\d+\.\d+(?:[eE][+-]?\d+)?(?:f32|f64)?", Number.Float),
            (r"\d+[eE][+-]?\d+(?:f32|f64)?", Number.Float),
            (r"\d+(?:f32|f64)", Number.Float),
            (r"0x[0-9a-fA-F]+(?:i64|u64|u8)?", Number.Hex),
            (r"\d+(?:i64|u64|u8)?", Number.Integer),

            # Keywords, operators-as-words, types, constants, builtins.
            (words(_KEYWORDS, prefix=r"\b", suffix=r"\b"), Keyword),
            (words(_OP_WORDS, prefix=r"\b", suffix=r"\b"), Operator.Word),
            (words(_TYPES, prefix=r"\b", suffix=r"\b"), Keyword.Type),
            (words(_CONSTANTS, prefix=r"\b", suffix=r"\b"), Keyword.Constant),
            (words(_BUILTINS, prefix=r"\b", suffix=r"\b"), Name.Builtin),
            (words(_SPECIAL_NAMES, prefix=r"\b", suffix=r"\b"), Name.Builtin.Pseudo),

            # Identifiers.
            (r"[A-Za-z_][A-Za-z0-9_]*", Name),

            # Multi-char operators first.
            (r":=|@=|@@|@\?|==|!=|<=|>=", Operator),
            (r"[+\-*/%<>=@]", Operator),

            (r"[(){}\[\]]", Punctuation),
            (r"[,.:|]", Punctuation),
        ],

        "whitespace": [
            (r"[ \t\r]+", Whitespace),
            (r"\n", Whitespace),
        ],

        "comments": [
            (r"//[^\n]*", Comment.Single),
            (r"/\*[\s\S]*?\*/", Comment.Multiline),
        ],

        # Hash lines must start at column 0. We accept it anywhere here for
        # simplicity -- the parser enforces the column-0 rule, the lexer just
        # colorizes. Shebang gets its own token; everything else (incl. `##`
        # directives like `##minimum_version 0.16`) is preprocessor-style.
        "hashlines": [
            (r"#![^\n]*", Comment.Hashbang),
            (r"##[^\n]*", Comment.Preproc),
            (r"#[^\n]*", Comment.Single),
        ],

        "string": [
            (r'\\[ntr"\\%]', String.Escape),
            (r'%\(', String.Interpol, "interpol"),
            (r'"', String, "#pop"),
            (r'[^"\\%]+', String),
            (r'%', String),  # bare % that isn't followed by (
        ],

        # In-string evaluation. Inside %(...) we lex normal TeaScript tokens.
        # Parens inside the expression nest via the same state, so we count
        # them implicitly via the state stack.
        "interpol": [
            include("whitespace"),
            include("comments"),

            *_raw_string_rules(),
            (r'"', String, "string"),

            (r"\d+\.\d+(?:[eE][+-]?\d+)?(?:f32|f64)?", Number.Float),
            (r"\d+[eE][+-]?\d+(?:f32|f64)?", Number.Float),
            (r"\d+(?:f32|f64)", Number.Float),
            (r"0x[0-9a-fA-F]+(?:i64|u64|u8)?", Number.Hex),
            (r"\d+(?:i64|u64|u8)?", Number.Integer),

            (words(_KEYWORDS, prefix=r"\b", suffix=r"\b"), Keyword),
            (words(_OP_WORDS, prefix=r"\b", suffix=r"\b"), Operator.Word),
            (words(_TYPES, prefix=r"\b", suffix=r"\b"), Keyword.Type),
            (words(_CONSTANTS, prefix=r"\b", suffix=r"\b"), Keyword.Constant),
            (words(_BUILTINS, prefix=r"\b", suffix=r"\b"), Name.Builtin),
            (words(_SPECIAL_NAMES, prefix=r"\b", suffix=r"\b"), Name.Builtin.Pseudo),

            (r"[A-Za-z_][A-Za-z0-9_]*", Name),

            (r":=|@=|@@|@\?|==|!=|<=|>=", Operator),
            (r"[+\-*/%<>=@]", Operator),

            (r"\(", Punctuation, "interpol"),
            (r"\)", String.Interpol, "#pop"),
            (r"[{}\[\]]", Punctuation),
            (r"[,.:|]", Punctuation),
        ],
    }
