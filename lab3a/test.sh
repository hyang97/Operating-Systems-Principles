./lab3a disk-image

diff super.csv solns/super.csv > /dev/null         && echo -e "SUPER:     PASS" || echo -e \
"SUPER:     FAIL"
diff group.csv solns/group.csv > /dev/null         && echo -e "GROUP:     PASS" || echo -e \
"GROUP:     FAIL"
diff bitmap.csv solns/bitmap.csv > /dev/null       && echo -e "BITMAP:    PASS" || echo -e \
"BITMAP:    FAIL"
diff inode.csv solns/inode.csv > /dev/null         && echo -e "INODE:     PASS" || echo -e \
"INODE:     FAIL"
diff directory.csv solns/directory.csv > /dev/null && echo -e "DIRECTORY: PASS" || echo -e \
"DIRECTORY: FAIL"
diff indirect.csv solns/indirect.csv > /dev/null   && echo -e "INDIRECT:  PASS" || echo -e \
"INDIRECT:  FAIL"

echo "Tests complete."
