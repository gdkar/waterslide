%thread(0) {
pcap -r ../*.pcap -> $data_in
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../bench_regexes.list -L NPU_MATCH -m3 -M -v5
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../bench_regexes.list -L NPU_MATCH -m3 -M -v5
}
