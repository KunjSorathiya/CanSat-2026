# Minimal Pico SDK stubs (syntax-check only)

These headers declare just enough of the Raspberry Pi Pico SDK surface for the
flight/ground firmware `#ifdef PICO_BUILD` branches to be compiled with
`g++ -fsyntax-only -DPICO_BUILD` on a host without the real SDK. They are **not** a
functional SDK and must never be linked into a firmware image.

`tools/check_pico_syntax.sh` uses them.
