# 13 "Not available while holding a coin"

Status: resolved (2026-11-02)
Type: task
Blocked by:

Found in the play test of issue 08. A top bar button reached with a coin in hand says "Back, not while holding a coin". "Back, not available while holding a coin" reads better. The words are `kNotWhileHoldingACoin` in `src/pc/a11y/css_names.hpp`; the unit tests in `test_css_speech.cpp` expect them too.

## Comments

2026-11-02: fixed in `css_names.hpp` (`kNotAvailableWhileHoldingACoin`); not yet heard by ear. A top bar target reached with a coin in hand says "Back, not available while holding a coin", or after words that end a sentence "Rules: 2-minute KO fest! Not available while holding a coin". The `css_speech` unit tests expect the new words; no drive script reaches them.
