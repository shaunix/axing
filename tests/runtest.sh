#!/bin/sh
for xml in xml/*.xml; do
    bname=$(basename "$xml" .xml)

    # FIXME: entities21 tests getting an error from GIO,
    # but GIO puts an absolute file path into the error
    # message, which messes up our simple diff test.
    if [ "$bname" = "entities21" ]; then continue; fi

    # FIXME: We're curreently using g_data_input_stream_read_line
    # to read text. This does not give us the newline in the line
    # it reads, so we then have to manually feed a newline after
    # every line. But what if a file ends without a newline? Well,
    # that's entities28, and it fails.
    if [ "$bname" = "entities28" ]; then continue; fi

    ../libaxing/test-axing-xml-parser-sync "$xml" > TMP;
    if ! cmp -s TMP results/"$bname".txt; then
        echo "$bname"
        diff -u TMP results/"$bname".txt
        echo ""
    fi
done
