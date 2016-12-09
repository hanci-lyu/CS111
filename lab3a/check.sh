for f in super group bitmap inode directory indirect; do
    if bash -c "diff <(sort ${f}.csv) <(sort sol/${f}.csv)" > /dev/null; then
        echo "Pass: ${f}"
    else
        echo "Fail: ${f}"
    fi
done