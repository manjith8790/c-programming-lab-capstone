#!/bin/sh
# End-to-end tests for minigit. Run with: make test
# If git is installed, hashes are also compared against real Git.

MG="$(cd "$(dirname "$0")/.." && pwd)/minigit"
TMP=$(mktemp -d)
OUT=$(mktemp -d)
PASS=0
FAIL=0

check() {
    # check "<description>" <command...>
    desc=$1
    shift
    if "$@" >/dev/null 2>&1; then
        PASS=$((PASS + 1))
        echo "  ok    $desc"
    else
        FAIL=$((FAIL + 1))
        echo "  FAIL  $desc"
    fi
}

cd "$TMP" || exit 1
echo "running tests in $TMP"

# --- init
"$MG" init >/dev/null
check "init creates the repository"   test -f .minigit/HEAD -a -d .minigit/objects

# --- blob hashing matches Git
printf 'Hello World\n' > hello.txt
H=$("$MG" hash-object hello.txt)
check "blob hash is correct"          test "$H" = 557db03de997c86a4a028e1ebd3a1ceb225be238
printf '' > empty.txt
check "empty file hash is correct"    test "$("$MG" hash-object empty.txt)" = e69de29bb2d1d6434b8b29ae775ad8c2e48c5391
head -c 100000 /dev/urandom > big.bin
if command -v git >/dev/null 2>&1; then
    check "large binary file matches git" test "$("$MG" hash-object big.bin)" = "$(git hash-object big.bin)"
fi

# --- add + deduplication
cp hello.txt copy.txt
mkdir -p src/lib
printf 'int main(void) { return 0; }\n' > src/main.c
printf 'void f(void) {}\n' > src/lib/f.c
"$MG" add . > "$OUT/add.out"
check "identical file is reused, not stored twice" grep -q "reused.*\(copy\|hello\).txt" "$OUT/add.out"
check "only one blob for two identical files" test "$(find .minigit/objects -name "${H#??}" | wc -l)" -eq 1

# --- commit + tree hash matches Git
"$MG" commit -m "first commit" >/dev/null
C1=$("$MG" log --oneline | head -1 | cut -d' ' -f1)
check "commit is created"             test -n "$C1"
if command -v git >/dev/null 2>&1; then
    T1=$("$MG" cat-file -p "$C1" | sed -n 's/^tree //p')
    GT=$(git init -q . 2>/dev/null; git add -- . ':!.minigit' 2>/dev/null; git write-tree)
    rm -rf .git
    check "tree hash matches git write-tree" test "$T1" = "$GT"
fi
check "status is clean after commit"  sh -c "'$MG' status | grep -q 'working tree clean'"
check "nothing to commit twice"       sh -c "! '$MG' commit -m again"

# --- second commit
printf 'Hello minigit\n' > hello.txt
check "status shows modified file"    sh -c "'$MG' status | grep -q 'modified: *hello.txt'"
printf 'new\n' > notes.txt
check "status shows untracked file"   sh -c "'$MG' status | grep -q 'notes.txt'"
"$MG" add hello.txt notes.txt >/dev/null
"$MG" commit -m "second commit" >/dev/null
check "log shows two commits"         test "$("$MG" log --oneline | wc -l)" -eq 2

# --- checkout old commit restores content
"$MG" checkout "$C1" >/dev/null
check "checkout restores old content" grep -q "Hello World" hello.txt
check "checkout removes newer files"  test ! -f notes.txt
check "status reports detached HEAD"  sh -c "'$MG' status | grep -q 'detached'"
"$MG" checkout main >/dev/null
check "checkout main returns to latest" grep -q "Hello minigit" hello.txt
check "newer file is back"            test -f notes.txt

# --- checkout refuses to lose work
printf 'unsaved\n' >> notes.txt
check "checkout refuses with uncommitted changes" sh -c "! '$MG' checkout $C1"
"$MG" add notes.txt >/dev/null
"$MG" commit -m "third commit" >/dev/null

# --- deleting a file
rm copy.txt
"$MG" add . >/dev/null
"$MG" commit -m "remove copy" >/dev/null
check "deleted file is not in the new commit" sh -c "! '$MG' cat-file -p HEAD | grep -q copy.txt; T=\$('$MG' cat-file -p HEAD | sed -n 's/^tree //p'); ! '$MG' cat-file -p \$T | grep -q copy.txt"

# --- stats
check "stats reports saved space"     sh -c "'$MG' stats | grep -q 'Saved'"

# --- integrity
check "verify passes on a clean store" "$MG" verify
OBJ=.minigit/objects/$(echo "$H" | cut -c1-2)/$(echo "$H" | cut -c3-)
chmod u+w "$OBJ"
printf 'X' >> "$OBJ"
check "verify detects a changed object"   sh -c "! '$MG' verify $H"
check "full verify reports corruption"    sh -c "'$MG' verify | grep -q '1 corrupted'"

cd / && rm -rf "$TMP" "$OUT"
echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
