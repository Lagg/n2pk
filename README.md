# n2pk #

This is an implementation of the archive format used by Neocore (Incredible Adventures of Van Helsing, etc.). As it's a fairly quick and dirty initial version as of writing this
it takes no opts besides an optional n2pk filename (and otherwise reads stdin) and will print out the files in the archive and unpack them to a directory named after the given archive.

## n2pk.xml ##

This is a binary format mapping file exported from the very cool HexEdit editor. If you don't want to directly use it with hexedit you can read the XML file directly since it has the offsets and data structures
used in the archive as well as a few comments. I'm considering it the defacto documentation right now.

# License #

Copyright (c) 2015+, Anthony Garcia <anthony@lagg.me>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
