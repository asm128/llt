# **dedup**
### This project is a simple deduplication tool that helps you identify and remove duplicate files from your system. It scans specified directories, compares file contents, and provides options to delete or move duplicates.
Usage:

```text
dedup <source-folder> [<source-folder> ...] <target-folder>
dedup "D:\Photos" "E:\Backup Photos" "F:\Duplicated"
```

The final path is the destination. All preceding paths are scanned recursively into one combined list, so duplicates can be found within or across sources. The minimum file size remains 50 MiB. Repeated paths and overlapping source folders are collected only once per normalized file path. Symbolic-link and hard-link aliases are not resolved by this path check.
