# s3g Thru 0.29.0-test.10

Local testing candidate with finer parameter-dial response.

- Parameter dials respond 25% more slowly for easier small adjustments.
- Chain BANK uses an additional fine response and advances at most one bank per encoder message, so accelerated input cannot skip several banks at once.
- BANK retains small movements between selections; it no longer speeds up again after each change.

