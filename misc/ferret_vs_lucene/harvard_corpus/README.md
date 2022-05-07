### Preparing the Harvard Corpus

Visit

[https://case.law/download/bulk_exports/20210921/by_jurisdiction/case_text_open/](https://case.law/download/bulk_exports/20210921/by_jurisdiction/case_text_open/)

and download the \*_text_\*.zip` files.
Unzip those.
Within ech of tue uncompressed directories there is a 'data' with a 'data.jsonl.xz'
Unxz the data.json.xz and move it to this 'harvard_corpus' directory,
so that it eventually looks like this:

21.09.2021  21:16       818.871.019 ark_text_20210921_data.jsonl
21.09.2021  22:21     2.765.741.526 ill_text_20210921_data.jsonl
21.09.2021  21:33     1.361.074.612 nc_text_20210921_data.jsonl
21.09.2021  21:09       366.868.640 nm_text_20210921_data.jsonl

Done. Now ferret_indexer.rb can be run with -h option, and it will take data from the harvard_corpus directory.