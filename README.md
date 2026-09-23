# minigit

A small version control system written in C, built on **content-addressable storage** —
the same idea Git uses internally.

Every file, directory and commit is stored under the SHA-1 hash of its content.
Because the name comes from the content:

- **identical content is stored only once** (deduplication),
- **any object can be found directly by its hash**, and
- **any change to stored data is detected** by hashing it again.

minigit uses Git's object format, so its blob and tree hashes are identical to the ones
real Git produces for the same files.

## Build

Needs a C99 compiler and `make`.

```sh
make            # builds ./minigit
make test       # runs the end-to-end tests
```

Tested on Linux with gcc. The code only uses the standard C library plus `dirent.h` and
`sys/stat.h`; there is basic support for Windows (MinGW), but it has not been tested there.

## Commands

| Command | What it does |
|---|---|
| `minigit init` | create an empty repository in `.minigit/` |
| `minigit add <file\|dir>...` | store file contents as blobs and stage them |
| `minigit commit -m "msg"` | build tree objects from the staged files and save a commit |
| `minigit log [--oneline]` | show the history by following parent links |
| `minigit status` | show staged, modified and untracked files |
| `minigit checkout <commit\|main>` | restore the files of an older commit, or go back to `main` |
| `minigit hash-object [-w] <file>` | print a file's blob hash (`-w` also stores it) |
| `minigit cat-file [-t\|-s\|-p] <obj>` | show an object's type, size or content |
| `minigit verify [obj]` | re-hash objects and report any that were changed |
| `minigit stats` | object counts and how much space deduplication saved |

Objects can be referred to by full hash, by the first 4+ characters, by `HEAD`, or by `main`.
The author name comes from the `MINIGIT_AUTHOR` environment variable, e.g.
`export MINIGIT_AUTHOR="Your Name <you@example.com>"`.

## Example

```
$ minigit init
Initialized empty minigit repository in .minigit/

$ printf 'Hello World\n' > hello.txt
$ cp hello.txt copy.txt
$ minigit add .
  stored  557db03  copy.txt
  reused  557db03  hello.txt  (same content already stored)

$ minigit commit -m "Initial commit"
[main 3664e4e] Initial commit

$ printf 'Hello minigit\n' > hello.txt
$ minigit add hello.txt
  stored  7e399ce  hello.txt
$ minigit commit -m "Update greeting"
[main 7107194] Update greeting

$ minigit log --oneline
7107194 Update greeting
3664e4e Initial commit

$ minigit checkout 3664e4e
HEAD is now at 3664e4e Initial commit
(detached HEAD: run 'minigit checkout main' to go back)
$ cat hello.txt
Hello World
$ minigit checkout main
Switched to branch 'main' (7107194 Update greeting)

$ minigit verify
checked 6 objects: 6 ok, 0 corrupted
```

To see the integrity check work, change one byte of a stored object and run
`minigit verify` again:

```
$ printf 'X' >> .minigit/objects/55/7db03de997c86a4a028e1ebd3a1ceb225be238
$ minigit verify 557db03
CORRUPTED: 557db03de997c86a4a028e1ebd3a1ceb225be238
  expected  557db03de997c86a4a028e1ebd3a1ceb225be238
  actual    b43d29553e703c532ca57654be4a37eaaf729806
```

## How it works

```
.minigit/
├── HEAD                 "ref: refs/heads/main", or a commit hash when detached
├── index                staged files: one "<blob hash> <path>" per line
├── refs/heads/main      hash of the latest commit on main
└── objects/
    ├── 55/7db03de9...   one file per object, named by its SHA-1
    └── ...
```

Each object is stored as `"<type> <size>\0<content>"` and named by the SHA-1 of those
bytes. The first two hex characters become a sub-directory so no single directory
holds too many files.

| Object | Content |
|---|---|
| **blob** | the raw bytes of one file (no file name) |
| **tree** | one directory: entries of `<mode> <name>\0<20-byte hash>`, pointing to blobs and sub-trees |
| **commit** | `tree <hash>`, `parent <hash>`, author, time and message |

Commits point to their parent, which forms the history; every tree holds the hashes of
its children, so the commit hash depends on every byte of every file (a Merkle tree).
When one file changes, only that file's blob, the trees above it and the new commit are
written — everything else is reused.

## Source layout

| File | Purpose |
|---|---|
| `src/sha1.c` | SHA-1 hash (FIPS 180-4) and hex conversion |
| `src/object.c` | read, write, verify and look up objects |
| `src/tree.c` | build tree objects from the index, flatten a tree back into files |
| `src/commit.c` | create and parse commit objects |
| `src/refs.c` | `HEAD` and branch references |
| `src/filelist.c` | sorted path → hash list, used for the index and for trees |
| `src/util.c` | file helpers, atomic writes, directory walking |
| `src/main.c` | the commands |
| `tests/run_tests.sh` | end-to-end tests, including hash comparison with real Git |

## Complexity

| Operation | Cost |
|---|---|
| hash a file | O(k), k = file size |
| find / store an object | O(1), a direct path from the hash |
| check for a duplicate | O(1) after hashing |
| verify an object | O(k) |
| log | O(c), c = number of commits |
| index lookup | O(log n), binary search on the sorted list |

## Limitations

- Objects are stored uncompressed (Git compresses them with zlib and packs deltas).
- SHA-1 is used for Git compatibility; it is no longer considered collision-resistant,
  which is why Git is moving to SHA-256.
- Only one branch (`main`); no merge, diff or remote support.
- Unreachable objects are never removed (no garbage collection).
- File permissions are not tracked; every file is stored with mode `100644`.
