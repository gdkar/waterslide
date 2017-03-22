%thread(0) {
pcap -r ../*.pcap -> $data_in
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu2 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v5
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v5
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v5
}
